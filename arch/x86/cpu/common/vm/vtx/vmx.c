/*
 * vmx.c: handling VMX architecture-related operations
 * Copyright (c) 2004, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place - Suite 330, Boston, MA 02111-1307 USA.
 *
 * Largely modified for Xvisor.
 */

#include <vmm_types.h>
#include <vmm_error.h>
#include <vmm_host_aspace.h>
#include <libs/bitops.h>
#include <libs/stringlib.h>
#include <cpu_features.h>
#include <cpu_vm.h>
#include <cpu_interrupts.h>
#include <arch_guest_helper.h>
#include <vmm_stdio.h>
#include <vmm_percpu.h>
#include <control_reg_access.h>
#include <vm/vmcs.h>
#include <vm/vmx.h>
#include <vm/vmx_intercept.h>

extern struct vmcs *alloc_vmcs(void);
extern u32 vmxon_region_nr_pages;
extern u32 vmcs_revision_id;

/* IMS: Table 30-1 Section 30.4 */
static char *ins_err_str[] = {
	"Index zero invalid\n",
	"VMCALL executed in VMX root operation",
	"VMCLEAR with invalid physical address",
	"VMCLEAR with VMXON pointer",
	"VMLAUNCH with non-clear VMCS",
	"VMRESUME with non-launched VMCS",
	"VMRESUME after VMXOFF (VMXOFF and VMXON between VMLAUNCH and VMRESUME)",
	"VM entry with invalid control field(s)",
	"VM entry with invalid host-state field(s)",
	"VMPTRLD with invalid physical address",
	"VMPTRLD with VMXON pointer",
	"VMPTRLD with incorrect VMCS revision identifier",
	"VMREAD/VMWRITE from/to unsupported VMCS component",
	"VMWRITE to read-only VMCS component",
	"VMXON executed in VMX root operation",
	"VM entry with invalid executive-VMCS pointer",
	"VM entry with non-launched executive VMCS",
	"VM entry with executive-VMCS pointer not VMXON pointer",
	"VMCALL with non-clear VMCS",
	"VMCALL with invalid VM-exit control fields",
	"VMCALL with incorrect MSEG revision identifier",
	"VMXOFF under dual-monitor treatment of SMIs and SMM",
	"VMCALL with invalid SMM-monitor features",
	"VM entry with invalid VM-execution control fields in executive VMCS",
	"VM entry with events blocked by MOV SS",
	"Invalid operand to INVEPT/INVVPID"
};

DEFINE_PER_CPU(physical_addr_t, vmxon_region_pa);
DEFINE_PER_CPU(virtual_addr_t, vmxon_region);

/* VMM Setup */
/* Intel IA-32 Manual 3B 27.5 p. 221 */
static int enable_vmx (struct cpuinfo_x86 *cpuinfo)
{
	u32 eax, edx;
	int bios_locked;
	u64 cr0, cr4, vmx_cr0_fixed0, vmx_cr0_fixed1;
	u64 vmx_cr4_fixed0, vmx_cr4_fixed1;
	u32 *vmxon_rev = NULL;
	int ret = VMM_OK;
	void *vmx_on_region;
	physical_addr_t vmx_on_region_pa;

	/* FIXME: Detect VMX support */
	if (!cpuinfo->hw_virt_available) {
		VM_LOG(LVL_ERR, "No VMX feature!\n");
		return VMM_EFAIL;
	}

	/* Determine the VMX capabilities */
	vmx_detect_capability();

	/* EPT and VPID support is required */
	if (!cpu_has_vmx_ept) {
		VM_LOG(LVL_ERR, "No EPT support!\n");
		return VMM_EFAIL;
	}

	if (!cpu_has_vmx_vpid) {
		VM_LOG(LVL_ERR, "No VPID support!\n");
		return VMM_EFAIL;
	}

	/*
	 * Ensure the current processor operating mode meets
	 * the requred CRO fixed bits in VMX operation.
	 */
	cr0 = read_cr0();
	cr4 = read_cr4();

	vmx_cr0_fixed0 = cpu_read_msr(MSR_IA32_VMX_CR0_FIXED0);
	vmx_cr0_fixed1 = cpu_read_msr(MSR_IA32_VMX_CR0_FIXED1);
	vmx_cr4_fixed0 = cpu_read_msr(MSR_IA32_VMX_CR4_FIXED0);
	vmx_cr4_fixed1 = cpu_read_msr(MSR_IA32_VMX_CR4_FIXED1);

	/*
	 * Appendix A.7 Intel Manual
	 * If bit is 1 in CR0_FIXED0, then that bit of CR0 is fixed to 1.
	 * if bit is 0 in CR0_FIXED1, then that bit of CR0 is fixed to 0.
	 */
	cr0 &= vmx_cr0_fixed1;
	cr0 |= vmx_cr0_fixed0;
	cr4 &= vmx_cr4_fixed1;
	cr4 |= vmx_cr4_fixed0;

	barrier();

	write_cr0(cr0);
	write_cr4(cr4);

	barrier();

	/* Enable VMX operation */
	set_in_cr4(X86_CR4_VMXE);

	cr0 = read_cr0();
	cr4 = read_cr4();

	VM_LOG(LVL_VERBOSE, "CR0: 0x%lx CR4: 0x%lx\n", cr0, cr4);

	if ((~cr0 & vmx_cr0_fixed0) || (cr0 & ~vmx_cr0_fixed1)) {
		VM_LOG(LVL_ERR, "Some settings of host CR0 are not allowed in VMX"
		       " operation. (Host CR0: 0x%lx CR0 Fixed0: 0x%lx CR0 Fixed1: 0x%lx)\n",
		       cr0, vmx_cr0_fixed0, vmx_cr0_fixed1);
		return VMM_EFAIL;
	}

	if ((~cr4 & vmx_cr4_fixed0) || (cr4 & ~vmx_cr4_fixed1)) {
		VM_LOG(LVL_ERR, "Some settings of host CR4 are not allowed in VMX"
		       " operation. (Host CR4: 0x%lx CR4 Fixed0: 0x%lx CR4 Fixed1: 0x%lx)\n",
		       cr4, vmx_cr4_fixed0, vmx_cr4_fixed1);
		return VMM_EFAIL;
	}

	cpu_read_msr32(IA32_FEATURE_CONTROL_MSR, &edx, &eax);

	/*
	 * Ensure that the IA32_FEATURE_CONTROL MSR has been
	 * properly programmed.
	 */
	bios_locked = !!(eax & IA32_FEATURE_CONTROL_MSR_LOCK);
	if (bios_locked) {
		if (!(eax & IA32_FEATURE_CONTROL_MSR_ENABLE_VMXON_OUTSIDE_SMX) ) {
			VM_LOG(LVL_ERR, "VMX disabled by BIOS.\n");
			return VMM_EFAIL;
		}
	}

	vmx_on_region = alloc_vmx_on_region();
	if (vmx_on_region == NULL) {
		VM_LOG(LVL_ERR, "Failed to create vmx on region.\n");
		ret = VMM_ENOMEM;
		goto _fail;
	}

	if (vmm_host_va2pa((virtual_addr_t)vmx_on_region,
			   &vmx_on_region_pa) != VMM_OK) {
		VM_LOG(LVL_ERR, "Critical conversion of vmx on regsion VA=>PA failed!\n");
		ret = VMM_EINVALID;
		goto _fail;
	}

	VM_LOG(LVL_VERBOSE, "%s: VMCS Revision Identifier: 0x%x\n",
	       __func__, vmcs_revision_id);

	vmxon_rev  = (u32 *)vmx_on_region;
	*vmxon_rev = vmcs_revision_id;
	*vmxon_rev &= ~(0x1UL  << 31);

	VM_LOG(LVL_VERBOSE, "%s: VMXON PTR: 0x%lx\n", __func__,
	       (unsigned long)vmx_on_region_pa);

	/* get in VMX ON  state */
	if ((ret = __vmxon(vmx_on_region_pa)) != VMM_OK) {
		VM_LOG(LVL_ERR, "VMXON returned with error: %d\n", ret);
		ret = VMM_EACCESS;
		goto _fail;
	}

	VM_LOG(LVL_INFO, "%s: Entered VMX operations successfully!\n", __func__);

	this_cpu(vmxon_region) = (virtual_addr_t)vmx_on_region;
	this_cpu(vmxon_region_pa) = vmx_on_region_pa;

	return VMM_OK;

 _fail:
	return ret;
}

static int __vmcs_run(struct vcpu_hw_context *context, bool resume)
{
	int rc = 0;
	u64 ins_err = 0;

	__asm__ __volatile__("cli\n\t"
			     "pushfq\n\t" /* Save flags */
			     "movq %%rsp, %%rax\n\t" /* Save host RSP */
			     "vmwrite %%rax, %%rdx\n\t"
			     "movq $vmx_return, %%rax\n\t"
			     "vmwrite %%rax, %%rbx\n\t" /* Save host RIP */
			     "pushq %%rbp\n\t"
			     "pushq %%rdi\n\t"
			     "pushq %%rsi\n\t"
			     "pushq %%r8\n\t"
			     "pushq %%r9\n\t"
			     "pushq %%r10\n\t"
			     "pushq %%r11\n\t"
			     "pushq %%r12\n\t"
			     "pushq %%r13\n\t"
			     "pushq %%r14\n\t"
			     "pushq %%r15\n\t"
			     "pushq %%rcx\n\t"
			     /*
			      * Check if vmlaunch or vmresume is needed, set the condition code
			      * appropriately for use below.
			      */
			     /* Load guest general purpose registers from the trap frame.
			      * Don't clobber flags.
			      */
			     "cmpl $1, %[resume]\n\t"
			     "movq %c[rax](%[context]), %%rax \n\t"
			     "movq %c[rbx](%[context]), %%rbx \n\t"
			     "movq %c[rdx](%[context]), %%rdx \n\t"
			     "movq %c[rbp](%[context]), %%rbp \n\t"
			     "movq %c[rdi](%[context]), %%rdi \n\t"
			     "movq %c[rsi](%[context]), %%rsi \n\t"
			     "movq %c[r8](%[context]),  %%r8  \n\t"
			     "movq %c[r9](%[context]),  %%r9  \n\t"
			     "movq %c[r10](%[context]), %%r10 \n\t"
			     "movq %c[r11](%[context]), %%r11 \n\t"
			     "movq %c[r12](%[context]), %%r12 \n\t"
			     "movq %c[r13](%[context]), %%r13 \n\t"
			     "movq %c[r14](%[context]), %%r14 \n\t"
			     "movq %c[r15](%[context]), %%r15 \n\t"
			     "movq %c[rcx](%[context]), %%rcx \n\t"
			     /* Check if above comparison holds if yes vmlaunch else vmresume */
			     "je 3f                 \n\t"
			     "1:vmlaunch               \n\t"
			     "jz 5f \n\t"
			     "jc 6f \n\t"
			      /*
			       * We can't come here, if we do there is serious problem
			       * so we want an unfixed #UD
			       */
			     "ud2\n\t"
			     ".section .fixup,\"ax\"\n"
			     "2:sub $3, %0 ; jmp 7f\n" /* Return -3 if #UD or #GF */
			     ".previous\n"
			     ".section __ex_table,\"a\"\n"
			     "   "__FIXUP_ALIGN"\n"
			     "   "__FIXUP_WORD" 1b,2b\n"
			     ".previous\n"
			     "3: vmresume         \n\t"
			     "jz 5f \n\t"
			     "jc 6f \n\t"
			      /*
			       * We can't come here, if we do there is serious problem
			       * so we want an unfixed #UD
			       */
			     "ud2\n\t"
			     ".section .fixup,\"ax\"\n"
			     "4:sub $4, %0 ; jmp 7f\n" /* Return -4 if #UD or #GF */
			     ".previous\n"
			     ".section __ex_table,\"a\"\n"
			     "   "__FIXUP_ALIGN"\n"
			     "   "__FIXUP_WORD" 3b,4b\n"
			     ".previous\n"

			     /* We shall come here only on successful VMEXIT */
			     "vmx_return: \n\t"
			     /*
			      * VM EXIT
			      * Save general purpose guest registers.
			      */
			     "pushq %%rcx\n\t" /* save guest rcx */
			     "movq 8(%%rsp), %%rcx\n\t" /* load our rcx(context) */
			     "movq %%rax, %c[rax](%[context]) \n\t" /* save guest rax and free it */
			     "popq %%rax\n\t" /* pop the guest rcx in rax */
			     "movq %%rax, %c[rcx](%[context]) \n\t" /* save the guest rcx */
			     "movq %%rbx, %c[rbx](%[context]) \n\t"
			     "movq %%rdx, %c[rdx](%[context]) \n\t"
			     "movq %%rbp, %c[rbp](%[context]) \n\t"
			     "movq %%rdi, %c[rdi](%[context]) \n\t"
			     "movq %%rsi, %c[rsi](%[context]) \n\t"
			     "movq %%r8,  %c[r8](%[context])  \n\t"
			     "movq %%r9,  %c[r9](%[context])  \n\t"
			     "movq %%r10, %c[r10](%[context]) \n\t"
			     "movq %%r11, %c[r11](%[context]) \n\t"
			     "movq %%r12, %c[r12](%[context]) \n\t"
			     "movq %%r13, %c[r13](%[context]) \n\t"
			     "movq %%r14, %c[r14](%[context]) \n\t"
			     "movq %%r15, %c[r15](%[context]) \n\t"
			     /* restore the host state */
			     "popq %%rcx\n\t"
			     "popq %%r15\n\t"
			     "popq %%r14\n\t"
			     "popq %%r13\n\t"
			     "popq %%r12\n\t"
			     "popq %%r11\n\t"
			     "popq %%r10\n\t"
			     "popq %%r9\n\t"
			     "popq %%r8\n\t"
			     "popq %%rsi\n\t"
			     "popq %%rdi\n\t"
			     "popq %%rbp\n\t"
			     "popfq\n\t"
			     "jmp 7f\n\t"
			     "5:popq %%rcx\n\t"
			     "popq %%r15\n\t"
			     "popq %%r14\n\t"
			     "popq %%r13\n\t"
			     "popq %%r12\n\t"
			     "popq %%r11\n\t"
			     "popq %%r10\n\t"
			     "popq %%r9\n\t"
			     "popq %%r8\n\t"
			     "popq %%rsi\n\t"
			     "popq %%rdi\n\t"
			     "popq %%rbp\n\t"
			     "popfq\n\t"
			     "sub $1, %0\n\t" /* -1 valid failure */
			     "jmp 7f\n\t"
			     "6:popq %%rcx\n\t"
			     "popq %%r15\n\t"
			     "popq %%r14\n\t"
			     "popq %%r13\n\t"
			     "popq %%r12\n\t"
			     "popq %%r11\n\t"
			     "popq %%r10\n\t"
			     "popq %%r9\n\t"
			     "popq %%r8\n\t"
			     "popq %%rsi\n\t"
			     "popq %%rdi\n\t"
			     "popq %%rbp\n\t"
			     "popfq\n\t"
			     "sub $2, %0\n\t" /* -2 invalid failure */
			     "7:sti\n\t"
			     :"=q"(rc)
			     :[resume]"m"(resume), "d"((unsigned long)HOST_RSP),
			      [context]"c"(context), "b"((unsigned long)HOST_RIP),
			      [rax]"i"(offsetof(struct vcpu_hw_context, g_regs[GUEST_REGS_RAX])),
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
			     : "cc", "memory", "rax"
			     );

	/* TR is not reloaded back the cpu after VM exit. */
	reload_host_tss();

	if (rc == -1) {
		if ((rc = __vmread(VM_INSTRUCTION_ERROR, &ins_err)) == VMM_OK) {
			vmm_printf("Instruction Error: (%s:%ld)\n", ins_err_str[ins_err], ins_err);
			//vmcs_dump(context);
		} else
			vmm_printf("Failed to read instruction error (%d)\n", rc);
		while(1);
		//BUG();
	} else if (rc == -2) {
		/* Invalid error: which probably means there is not current VMCS: Problem! */
		if (context->vcpu_emergency_shutdown)
			context->vcpu_emergency_shutdown(context);
	}

	arch_guest_handle_vm_exit(context);

	return VMM_OK;
}

static void vmx_vcpu_run(struct vcpu_hw_context *context)
{
	int rc;
	physical_addr_t phys;

	if (unlikely(!(context->vmcs_state & VMCS_STATE_ACTIVE))) {
		/*
		 * If the current VMCS is not same as we are going to load
		 * make the current VMCS non-current.
		 */
		if (current_vmcs(&phys) && phys != context->vmcs_pa)
			context->vmcs_state &= ~VMCS_STATE_CURRENT;

		/* VMPTRLD: mark this vmcs active, current & clear */
		__vmptrld(context->vmcs_pa);
		context->vmcs_state  |=  (VMCS_STATE_ACTIVE | VMCS_STATE_CURRENT);
	}

	if (likely(context->vmcs_state & VMCS_STATE_LAUNCHED)) {
		rc = __vmcs_run(context, true);
	} else {
		context->vmcs_state |= VMCS_STATE_LAUNCHED;
		rc = __vmcs_run(context, false);
	}

	BUG_ON(rc != VMM_OK);
}

int intel_setup_vm_control(struct vcpu_hw_context *context)
{
	/* Create a VMCS */
	struct vmcs *vmcs;
	int ret = VMM_EFAIL;

	vmcs = create_vmcs();
	if (vmcs == NULL) {
		vmm_printf("Failed to create VMCS.\n");
		ret = VMM_ENOMEM;
		goto _fail;
	}

	context->vmcs = vmcs;

	if (vmm_host_va2pa((virtual_addr_t)context->vmcs,
			   &context->vmcs_pa) != VMM_OK) {
		vmm_printf("Critical conversion of VMCB VA=>PA failed!\n");
		ret = VMM_EINVALID;
		goto _fail;
	}

	if ((ret = __vmpclear(context->vmcs_pa)) != VMM_OK) {
	  vmm_printf("VMCS clear failed with error: %d\n", ret);
	  ret = VMM_EACCESS;
	  goto _fail;
	}

	if ((ret = __vmptrld(context->vmcs_pa)) != VMM_OK) {
		vmm_printf("VMCS load failed with error: %d\n", ret);
		ret = VMM_EACCESS;
		goto _fail;
	}

	context->vmcs_state &= ~(VMCS_STATE_LAUNCHED);
	context->vmcs_state |= (VMCS_STATE_ACTIVE | VMCS_STATE_CURRENT);

	if ((ret = vmx_set_control_params(context)) != VMM_OK) {
		vmm_printf("Failed to set control parameters of VCPU.\n");
		goto _fail;
	}

	vmx_set_vm_to_powerup_state(context);

	context->vcpu_run = vmx_vcpu_run;
	context->vcpu_exit = vmx_vcpu_exit;

	/* Monitor the coreboot's debug port output */
	enable_ioport_intercept(context, 0x80);

	return VMM_OK;

 _fail:
	if (context->vmcs)
		vmm_host_free_pages((virtual_addr_t)context->vmcs, 1);

	return ret;
}

int __init intel_init(struct cpuinfo_x86 *cpuinfo)
{
	/* Enable VMX */
	if (enable_vmx(cpuinfo) != VMM_OK) {
		VM_LOG(LVL_ERR, "ERROR: Failed to enable virtual machine.\n");
		return VMM_EFAIL;
	}

	return VMM_OK;
}
