/**
 * Copyright (c) 2013 Himanshu Chauhan.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * @file amd_svm.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief AMD SVM setup.
 */

#include <vmm_host_aspace.h>
#include <processor_flags.h>
#include <libs/bitops.h>
#include <libs/stringlib.h>
#include <vmm_error.h>
#include <vmm_stdio.h>
#include <cpu_features.h>
#include <cpu_vm.h>
#include <vm/amd_svm.h>
#include <vm/amd_intercept.h>

enum svm_init_mode {
	SVM_MODE_REAL,
	SVM_MODE_PROTECTED_NOPAGE,
	SVM_MODE_PROCTECTED_PAGED,
	SVM_MODE_LONG
};

#define SVM_FEATURE_NPT			(1 <<  0)
#define SVM_FEATURE_LBRV		(1 <<  1)
#define SVM_FEATURE_SVML		(1 <<  2)
#define SVM_FEATURE_NRIP		(1 <<  3)
#define SVM_FEATURE_PAUSE_FILTER	(1 << 10)

#define NESTED_EXIT_HOST		0   /* Exit handled on host level */
#define NESTED_EXIT_DONE		1   /* Exit caused nested vmexit  */
#define NESTED_EXIT_CONTINUE		2   /* Further checks needed  */

#define NR_SAVE_AREA_PAGES		1

/* AMD64 manual Vol. 2, p. 441 */
/* Host save area */
static virtual_addr_t host_save_area;

extern void handle_vcpuexit(struct vcpu_hw_context *context);

static virtual_addr_t alloc_host_save_area(void)
{
	virtual_addr_t hsa  = vmm_host_alloc_pages(NR_SAVE_AREA_PAGES,
                                               VMM_MEMORY_FLAGS_NORMAL);

	if (!hsa)
		return 0;

	memset((void *)hsa, 0, VMM_PAGE_SIZE);

	return hsa;
}

static struct vmcb *alloc_vmcb(void)
{
	struct vmcb *vmcb;

	vmcb  = (struct vmcb *)vmm_host_alloc_pages(NR_SAVE_AREA_PAGES,
						    VMM_MEMORY_FLAGS_NORMAL);
	if (!vmcb) return NULL;

	memset(vmcb, 0, sizeof (struct vmcb));

	return vmcb;
}

static void set_control_params (struct vmcb *vmcb)
{
	/* Enable/disable nested paging (See AMD64 manual Vol. 2, p. 409) */
	vmcb->np_enable = 1;
	vmcb->tlb_control = 0;
	vmcb->tsc_offset = 0;
	vmcb->guest_asid = 1;

	/* Intercept the VMRUN and VMMCALL instructions */
	vmcb->general2_intercepts = (INTRCPT_VMRUN | INTRCPT_VMMCALL);

	vmcb->general1_intercepts |= INTRCPT_INTN;
}

static void set_vm_to_powerup_state(struct vmcb *vmcb)
{
	memset(vmcb, 0, sizeof(vmcb));

	vmcb->cr0 = 0x0000000060000010;
	vmcb->cr2 = 0;
	vmcb->cr3 = 0;
	vmcb->cr4 = 0;
	vmcb->rflags = 0x2;
	vmcb->efer = EFER_SVME;

	vmcb->rip = 0xFFF0;
	vmcb->cs.sel = 0xF000;
	vmcb->cs.base = 0xFFFF0000;
	vmcb->cs.limit = 0xFFFF;

	vmcb->ds.sel = 0;
	vmcb->ds.limit = 0xFFFF;
	vmcb->es.sel = 0;
	vmcb->es.limit = 0xFFFF;
	vmcb->fs.sel = 0;
	vmcb->fs.limit = 0xFFFF;
	vmcb->gs.sel = 0;
	vmcb->gs.limit = 0xFFFF;
	vmcb->ss.sel = 0;
	vmcb->ss.limit = 0xFFFF;

	vmcb->gdtr.base = 0;
	vmcb->gdtr.limit = 0xFFFF;
	vmcb->idtr.base = 0;
	vmcb->idtr.limit = 0xFFFF;

	vmcb->ldtr.sel = 0;
	vmcb->ldtr.base = 0;
	vmcb->ldtr.limit = 0xFFFF;
	vmcb->tr.sel = 0;
	vmcb->tr.base = 0;
	vmcb->tr.limit = 0xFFFF;
}

static __unused void set_vm_to_mbr_start_state(struct vmcb* vmcb, enum svm_init_mode mode)
{
	/*
	 * Prepare to load GRUB for the second time. Basically copy
	 * the state when GRUB is first started. Note: some other
	 * states will be set in svm_asm.S, at load_guest_states:
	 * ebx, ecx, edx, esi, edi, ebp.
	 */
	vmcb->rax = 0;

	vmcb->rip = 0x7c00;

	vmcb->cs.attrs.bytes = 0x019B;
	vmcb->cs.limit = 0xFFFF;
	vmcb->cs.base = 0;
	vmcb->cs.sel = 0;

	vmcb->ds.sel=0x0040;
	vmcb->fs.sel=0xE717;
	vmcb->gs.sel=0xF000;

	int i;
	struct seg_selector *segregs [] = {&vmcb->ss, &vmcb->ds, &vmcb->es, &vmcb->fs, &vmcb->gs, NULL};
	for (i = 0; segregs [i] != NULL; i++) {
		struct seg_selector * x = segregs [i];
		x->attrs.bytes = 0x93;
		x->base = 0;
		x->limit = 0xFFFF;
	}

	vmcb->rsp=0x000003E2;

	vmcb->ss.attrs.bytes = 0x193;
	vmcb->ds.base = 00000400;
	vmcb->fs.base = 0xE7170;
	vmcb->gs.base = 0xF0000;

	vmcb->efer = EFER_SVME;

	vmcb->cr0 = 0x0000000000000010;

	vmcb->idtr.limit = 0x3FF;
	vmcb->idtr.base = 0;

	vmcb->gdtr.limit = 0x20;
	vmcb->gdtr.base = 0x06E127;

	vmcb->rflags = 0x2206;

	vmcb->cpl = 0;

	/*
	 * Each page table entry use 3 flags: PAT PCD PWT to specify index of the
	 * corresponding PAT entry, which then specify the type of memory access for that page
		PA0 = 110	- Writeback
		PA1 = 100	- Writethrough
		PA2 = 111	- UC-
		PA3 = 000	- Unchachable
		PA4 = 110	- Writeback
		PA5 = 100	- Writethrough
		PA6 = 111	- UC-
		PA7 = 000	- Unchachable
	 This is also the default PAT */
	vmcb->g_pat = 0x7040600070406ULL;

	switch(mode) {
	case SVM_MODE_REAL:
		/* Legacy real mode */
		vmcb->cr0 = X86_CR0_ET;
		vmcb->cr4 = 0;
		break;

	case SVM_MODE_PROTECTED_NOPAGE:
		/* Legacy protected mode, paging disabled */
		vmcb->cr0 = X86_CR0_PE | X86_CR0_ET;
		vmcb->cr3 = 0;
		vmcb->cr4 = 0;
		break;

	case SVM_MODE_PROCTECTED_PAGED:
		/* Legacy protected mode, paging enabled (4MB pages) */
		vmcb->cr0 = X86_CR0_PE | X86_CR0_ET | X86_CR0_PG;
		vmcb->cr3 = 0x07000000;
		vmcb->cr4 = X86_CR4_PSE;
		break;

	case SVM_MODE_LONG:
		vmcb->cr0 = X86_CR0_PE | X86_CR0_MP | X86_CR0_ET | X86_CR0_NE | X86_CR0_PG;
		vmcb->cr4 = X86_CR4_PAE;
		vmcb->cr3 = 0x07000000;
		vmcb->efer |= (EFER_LME | EFER_LMA);
		break;
	}
}

static void svm_run(struct vcpu_hw_context *context)
{
	/* Set the pointer to VMCB to %rax (vol. 2, p. 440) */
	__asm__("pushq %%rax; movq %0, %%rax" :: "r" (context->vmcb));

	svm_launch();

	__asm__("popq %rax");
}

static int enable_svme(void)
{
	u64 msr_val;

	msr_val = cpu_read_msr(MSR_EFER);
	msr_val |= EFER_SVME;
	cpu_write_msr(MSR_EFER, msr_val);

	return VMM_OK;
}

static int enable_svm (struct cpuinfo_x86 *c)
{
	u64 phys_hsa;

	if (!c->hw_virt_available) {
		VM_LOG(LVL_ERR, "ERROR: Hardware virtualization is not support but Xvisor needs it.\n");
		return VMM_EFAIL;
	}

	if (!c->hw_nested_paging)
		VM_LOG(LVL_INFO, "Nested pagetables are not supported.\n"
		       "Enabling software walking of page tables.\n");

	/*
	 * Before SVM instructions can be used, EFER.SVME must be set.
	 */
	enable_svme();

	VM_LOG(LVL_VERBOSE, "Allocating host save area.\n");

	/* Initialize the Host Save Area */
	host_save_area = alloc_host_save_area();
	if (vmm_host_va2pa(host_save_area, (physical_addr_t *)&phys_hsa) != VMM_OK) {
		VM_LOG(LVL_ERR, "Host va2pa for host save area failed.\n");
		return VMM_EFAIL;
	}

	VM_LOG(LVL_VERBOSE, "Write HSAVE PA.\n");
	cpu_write_msr(MSR_K8_VM_HSAVE_PA, phys_hsa);

	VM_LOG(LVL_VERBOSE, "All fine.\n");
	return VMM_OK;
}

int amd_setup_vm_control(struct vcpu_hw_context *context)
{
	/* Allocate a new page inside host memory for storing VMCB. */
	context->vmcb = alloc_vmcb();

	/* Set control params for this VM */
	set_control_params(context->vmcb);

	/*
	 * FIXME: VM: What state to load should come from VMCB.
	 * Ideally, if a bios image is provided the vm should
	 * be turned on with powerup state. Otherwise, it can
	 * be configured to run the MBR code.
	 */
	set_vm_to_powerup_state(context->vmcb);

	context->vcpu_run = svm_run;
	context->vcpu_exit = handle_vcpuexit;

	return VMM_OK;
}

int init_amd(struct cpuinfo_x86 *cpuinfo)
{
	/* FIXME: SMP: This should be done by all CPUs? */
	if (enable_svm (cpuinfo) != VMM_OK) {
		VM_LOG(LVL_ERR, "ERROR: Failed to enable virtual machine.\n");
		return VMM_EFAIL;
	}

	VM_LOG(LVL_VERBOSE, "AMD SVM enable success!\n");

	return VMM_OK;
}
