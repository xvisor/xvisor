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
#include <vmm_manager.h>
#include <cpu_features.h>
#include <cpu_vm.h>
#include <cpu_interrupts.h>
#include <vm/amd_svm.h>
#include <vm/amd_intercept.h>
#include <emu/i8259.h>
#include <arch_guest_helper.h>

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
#define NR_IOPM_AREA_PAGES		3

/* AMD64 manual Vol. 2, p. 441 */
/* Host save area */
static virtual_addr_t host_save_area;

extern void handle_vcpuexit(struct vcpu_hw_context *context);

static virtual_addr_t alloc_host_save_area(void)
{
	/* FIXME: Make it write back */
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

static void set_control_params (struct vcpu_hw_context *context)
{
	struct vmcb *vmcb = context->vmcb;

	/* Enable/disable nested paging (See AMD64 manual Vol. 2, p. 409) */
	vmcb->np_enable = 0;
	vmcb->tlb_control = 1; /* Flush all TLBs global/local/asid wide */
	vmcb->tsc_offset = 0;
	vmcb->guest_asid = 1;

	/* enable EFLAGS.IF virtualization */
	vmcb->vintr.fields.intr_masking = 1;

	vmcb->cr_intercepts |= (INTRCPT_WRITE_CR3  | INTRCPT_READ_CR3 |
				INTRCPT_WRITE_CR0  | INTRCPT_READ_CR0 |
				INTRCPT_WRITE_CR2  | INTRCPT_READ_CR2 |
				INTRCPT_WRITE_CR1  | INTRCPT_READ_CR1 |
				INTRCPT_WRITE_CR4  | INTRCPT_READ_CR4);

	/* Intercept the VMRUN and VMMCALL instructions */
	vmcb->general2_intercepts = (INTRCPT_VMRUN | INTRCPT_VMMCALL);

	vmcb->general1_intercepts |= (INTRCPT_INTR       |
				      INTRCPT_VINTR	 |
				      INTRCPT_CR0_WR     |
				      INTRCPT_CPUID      |
				      INTRCPT_IOIO_PROT  |
				      INTRCPT_MSR_PROT   |
				      INTRCPT_TASKSWITCH |
				      INTRCPT_SHUTDOWN   |
				      INTRCPT_INVLPG     |
				      INTRCPT_INVLPGA    |
				      INTRCPT_HLT);

	vmcb->exception_intercepts |= (INTRCPT_EXC_DIV_ERR   |
				       INTRCPT_EXC_DB        |
				       INTRCPT_EXC_NMI       |
				       INTRCPT_EXC_BP        |
				       INTRCPT_EXC_OV        |
				       INTRCPT_EXC_BOUNDS    |
				       INTRCPT_EXC_INV_OPC   |
				       INTRCPT_EXC_NDEV      |
				       INTRCPT_EXC_DFAULT    |
				       INTRCPT_EXC_CP_OVRRUN |
				       INTRCPT_EXC_INV_TSS   |
				       INTRCPT_EXC_SEG_NP    |
				       INTRCPT_EXC_NO_STK_SEG|
				       INTRCPT_EXC_GPF       |
				       INTRCPT_EXC_PF);

	vmcb->exception_intercepts = 0xffffffffUL;
}

static void set_vm_to_powerup_state(struct vcpu_hw_context *context)
{
	physical_addr_t gcr3_pa;
	struct vmcb *vmcb = context->vmcb;

	context->g_cr0 = (X86_CR0_ET | X86_CR0_CD | X86_CR0_NW);
	context->g_cr1 = context->g_cr2 = context->g_cr3 = 0;

	/*
	 * NOTE: X86_CR0_PG with disabled PE is a new mode in SVM. Its
	 * called Paged Real Mode. It helps virtualization of the
	 * Real mode boot. AMD PACIFICA SPEC Section 2.15.
	 */
	vmcb->cr0 = (X86_CR0_ET | X86_CR0_CD | X86_CR0_NW | X86_CR0_PG);
	vmcb->cr2 = 0;
	vmcb->cr4 = 0;
	vmcb->rflags = 0x2;
	vmcb->efer = EFER_SVME;

	if (vmm_host_va2pa((virtual_addr_t)context->shadow32_pgt, &gcr3_pa) != VMM_OK)
		vmm_panic("ERROR: Couldn't convert guest shadow table virtual address to physical!\n");

	/* Since this VCPU is in power-up stage, two-fold 32-bit page table apply to it */
	vmcb->cr3 = gcr3_pa;

	/*
	 * Make the CS.RIP point to 0xFFFF0. The reset vector. The Bios seems
	 * to be linked in a fashion that the reset vectors lies at0x3fff0.
	 * The guest physical address will be 0xFFFF0 when the first page fault
	 * happens in paged real mode. Hence, the the bios is loaded at 0xc0c0000
	 * so that 0xc0c0000 + 0x3fff0 becomes 0xc0ffff0 => The host physical
	 * for reset vector. Everything else then just falls in place.
	 */
	vmcb->rip = 0xFFF0;
	vmcb->cs.sel = 0xF000;
	vmcb->cs.base = 0xF0000;
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
	vmcb->tr.limit = 0xffff;
	vmcb->tr.attrs.bytes = (1 << 15) | (11 << 8);
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
	clgi();
	asm volatile ("push %%rbp \n\t"
		      "mov %c[rbx](%[context]), %%rbx \n\t"
		      "mov %c[rcx](%[context]), %%rcx \n\t"
		      "mov %c[rdx](%[context]), %%rdx \n\t"
		      "mov %c[rsi](%[context]), %%rsi \n\t"
		      "mov %c[rdi](%[context]), %%rdi \n\t"
		      "mov %c[rbp](%[context]), %%rbp \n\t"
		      "mov %c[r8](%[context]),  %%r8  \n\t"
		      "mov %c[r9](%[context]),  %%r9  \n\t"
		      "mov %c[r10](%[context]), %%r10 \n\t"
		      "mov %c[r11](%[context]), %%r11 \n\t"
		      "mov %c[r12](%[context]), %%r12 \n\t"
		      "mov %c[r13](%[context]), %%r13 \n\t"
		      "mov %c[r14](%[context]), %%r14 \n\t"
		      "mov %c[r15](%[context]), %%r15 \n\t"

		      /* Enter guest mode */
		      "push %%rax \n\t"
		      "mov %c[vmcb](%[context]), %%rax \n\t"
		      "vmload\n\t"
		      "vmrun\n\t"
		      "vmsave\n\t"
		      "pop %%rax \n\t"

		      /* Save guest registers, load host registers */
		      "mov %%rbx, %c[rbx](%[context]) \n\t"
		      "mov %%rcx, %c[rcx](%[context]) \n\t"
		      "mov %%rdx, %c[rdx](%[context]) \n\t"
		      "mov %%rsi, %c[rsi](%[context]) \n\t"
		      "mov %%rdi, %c[rdi](%[context]) \n\t"
		      "mov %%rbp, %c[rbp](%[context]) \n\t"
		      "mov %%r8,  %c[r8](%[context])  \n\t"
		      "mov %%r9,  %c[r9](%[context])  \n\t"
		      "mov %%r10, %c[r10](%[context]) \n\t"
		      "mov %%r11, %c[r11](%[context]) \n\t"
		      "mov %%r12, %c[r12](%[context]) \n\t"
		      "mov %%r13, %c[r13](%[context]) \n\t"
		      "mov %%r14, %c[r14](%[context]) \n\t"
		      "mov %%r15, %c[r15](%[context]) \n\t"
		      "pop %%rbp\n\t"
		      :
		      : [context]"a"(context),
		      [vmcb]"i"(offsetof(struct vcpu_hw_context, vmcb_pa)),
		      [rbx]"i"(offsetof(struct vcpu_hw_context, g_regs[GUEST_REGS_RBX])),
		      [rcx]"i"(offsetof(struct vcpu_hw_context, g_regs[GUEST_REGS_RCX])),
		      [rdx]"i"(offsetof(struct vcpu_hw_context, g_regs[GUEST_REGS_RDX])),
		      [rsi]"i"(offsetof(struct vcpu_hw_context, g_regs[GUEST_REGS_RSI])),
		      [rdi]"i"(offsetof(struct vcpu_hw_context, g_regs[GUEST_REGS_RDI])),
		      [rbp]"i"(offsetof(struct vcpu_hw_context, g_regs[GUEST_REGS_RBP])),
		      [r8]"i"(offsetof(struct vcpu_hw_context,  g_regs[GUEST_REGS_R8])),
		      [r9]"i"(offsetof(struct vcpu_hw_context,  g_regs[GUEST_REGS_R9])),
		      [r10]"i"(offsetof(struct vcpu_hw_context, g_regs[GUEST_REGS_R10])),
		      [r11]"i"(offsetof(struct vcpu_hw_context, g_regs[GUEST_REGS_R11])),
		      [r12]"i"(offsetof(struct vcpu_hw_context, g_regs[GUEST_REGS_R12])),
		      [r13]"i"(offsetof(struct vcpu_hw_context, g_regs[GUEST_REGS_R13])),
		      [r14]"i"(offsetof(struct vcpu_hw_context, g_regs[GUEST_REGS_R14])),
		      [r15]"i"(offsetof(struct vcpu_hw_context, g_regs[GUEST_REGS_R15]))
		      : "cc", "memory"
			, "rbx", "rcx", "rdx", "rsi", "rdi"
			, "r8", "r9", "r10", "r11" , "r12", "r13", "r14", "r15"
		      );

	/* TR is not reloaded back the cpu after VM exit. */
	reload_host_tss();

	context->g_regs[GUEST_REGS_RAX] = context->vmcb->rax;

	/* invalidate the previously injected event */
	if (context->vmcb->eventinj.fields.v)
		context->vmcb->eventinj.fields.v = 0;

	stgi();

	arch_guest_handle_vm_exit(context);
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

	if (vmm_host_va2pa((virtual_addr_t)context->vmcb, &context->vmcb_pa) != VMM_OK)
		vmm_panic("Critical conversion of VMCB VA=>PA failed!\n");


	/* Set control params for this VM */
	set_control_params(context);

	if (context->icept_table.io_table_phys)
		context->vmcb->iopm_base_pa = context->icept_table.io_table_phys;

	VM_LOG(LVL_INFO, "IOPM Base physical address: 0x%lx\n", context->vmcb->iopm_base_pa);

	/*
	 * FIXME: VM: What state to load should come from VMCB.
	 * Ideally, if a bios image is provided the vm should
	 * be turned on with powerup state. Otherwise, it can
	 * be configured to run the MBR code.
	 */
	set_vm_to_powerup_state(context);

	context->vcpu_run = svm_run;
	context->vcpu_exit = handle_vcpuexit;

	/* Monitor the coreboot's debug port output */
	enable_ioport_intercept(context, 0x80);

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
