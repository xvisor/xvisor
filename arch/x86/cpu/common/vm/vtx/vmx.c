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
#include <control_reg_access.h>
#include <vm/vmcs.h>
#include <vm/vmx.h>
#include <vm/vmx_intercept.h>

extern struct vmcs *alloc_vmcs(void);
extern u32 vmxon_region_nr_pages;

/* VMM Setup */
/* Intel IA-32 Manual 3B 27.5 p. 221 */
static int enable_vmx (struct cpuinfo_x86 *cpuinfo)
{
	u32 eax, edx;
	int bios_locked;
	u64 cr0, vmx_cr0_fixed0, vmx_cr0_fixed1;

	/* FIXME: Detect VMX support */
	if (!cpuinfo->hw_virt_available) {
		vmm_printf("No VMX feature!\n");
		return VMM_EFAIL;
	}

	/* Determine the VMX capabilities */
	vmx_detect_capability();

	/* EPT and VPID support is required */
	if (!cpu_has_vmx_ept) {
		vmm_printf("No EPT support!\n");
		return VMM_EFAIL;
	}

	if (!cpu_has_vmx_vpid) {
		vmm_printf("No VPID support!\n");
		return VMM_EFAIL;
	}

	/* Enable VMX operation */
	set_in_cr4(X86_CR4_VMXE);

	/*
	 * Ensure the current processor operating mode meets
	 * the requred CRO fixed bits in VMX operation.
	 */
	cr0 = read_cr0();
	vmx_cr0_fixed0 = cpu_read_msr(MSR_IA32_VMX_CR0_FIXED0);
	vmx_cr0_fixed1 = cpu_read_msr(MSR_IA32_VMX_CR0_FIXED1);

	/*
	 * Appendix A.7 Intel Manual
	 * If bit is 1 in CR0_FIXED0, then that bit of CR0 is fixed to 1.
	 * if bit is 0 in CR0_FIXED1, then that bit of CR0 is fixed to 0.
	 */
	cr0 &= vmx_cr0_fixed1;
	cr0 |= vmx_cr0_fixed0;

	barrier();

	write_cr0(cr0);

	barrier();

	cr0 = read_cr0();

	if ((~cr0 & vmx_cr0_fixed0) || (cr0 & ~vmx_cr0_fixed1)) {
		vmm_printf("Some settings of host CR0 are not allowed in VMX"
			   " operation. (Host CR0: 0x%lx CR0 Fixed0: 0x%lx CR0 Fixed1: 0x%lx)\n",
			   cr0, vmx_cr0_fixed0, vmx_cr0_fixed1);
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
			vmm_printf("VMX disabled by BIOS.\n");
			return VMM_EFAIL;
		}
	}

	return VMM_OK;
}

static int __vmcs_run(struct vcpu_hw_context *context, bool resume)
{
	int rc;

	__asm__ __volatile__("cmpl $1, %1 \n\t"
			     /* Save host RSP */
			     "mov %%rsp, %%rax \n\t"
			     "vmwrite %%rax, %%rdx \n\t"
			     /*
			      * Check if vmlaunch or vmresume is needed, set the condition code
			      * appropriately for use below.
			      */
			     /* Load guest general purpose registers from the trap frame.
			      * Don't clobber flags.
			      */
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
			     "jne 1f                 \n\t"
			     "vmlaunch               \n\t"
			     "jmp 2f                 \n\t"
			     "1: " "vmresume         \n\t"
			     "2: "
			     "vmx_return: "
			     /*
			      * VM EXIT
			      * Save general purpose guest registers.
			      */
			     "movq %%rax, %c[rax](%[context]) \n\t"
			     "movq %%rbx, %c[rbx](%[context]) \n\t"
			     "movq %%rcx, %c[rcx](%[context]) \n\t"
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
			     "jz fail_valid \n\t"
			     "jc fail_invalid \n\t"
			     "movq $0, 0(%0) \n\t"
			     "jmp _done\n\t"
			     "fail_valid: movq $1, 0(%0)\n\t"
			     "fail_invalid: movq $2, 0(%0)\n\t"
			     "_done:\n\t"
			     :"=r"(rc):"m"(resume), "d"((unsigned long)HOST_RSP),
			      [context]"r"(context),
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
			     : "cc", "memory"
			       , "rax", "rbx", "rdi", "rsi"
			       , "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"
			     );

	/* TR is not reloaded back the cpu after VM exit. */
	reload_host_tss();

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

	context->vmx_on_region = alloc_vmx_on_region();
	if (context->vmx_on_region == NULL) {
		vmm_printf("Failed to create vmx on region.\n");
		ret = VMM_ENOMEM;
		goto _fail;
	}

	if (vmm_host_va2pa((virtual_addr_t)context->vmx_on_region,
			   &context->vmxon_region_pa) != VMM_OK) {
		vmm_printf("Critical conversion of vmx on regsion VA=>PA failed!\n");
		ret = VMM_EINVALID;
		goto _fail;
	}

	/* get in VMX ON  state */
	if ((ret = __vmxon(context->vmxon_region_pa)) != VMM_OK) {
		vmm_printf("VMXON returned with error: %d\n", ret);
		ret = VMM_EACCESS;
		goto _fail;
	}

	if ((ret = __vmptrld(context->vmcs_pa)) != VMM_OK) {
		vmm_printf("VMCS load failed with error: %d\n", ret);
		ret = VMM_EACCESS;
		goto _fail;
	}

	/* VMCLEAR: clear launched state */
	if ((ret = __vmpclear(context->vmcs_pa)) != VMM_OK) {
		vmm_printf("VMClear failed with error: %d\n", ret);
		ret = VMM_EACCESS;
		goto _fail;
	}

	context->vmcs_state &= ~(VMCS_STATE_LAUNCHED  | VMCS_STATE_ACTIVE | VMCS_STATE_CURRENT);

	if ((ret = vmx_set_control_params(context)) != VMM_OK)
		goto _fail;

	vmx_set_vm_to_powerup_state(context);

	context->vcpu_run = vmx_vcpu_run;
	context->vcpu_exit = vmx_vcpu_exit;

	/* Monitor the coreboot's debug port output */
	enable_ioport_intercept(context, 0x80);

	return VMM_OK;

 _fail:
	if (context->vmcs)
		vmm_host_free_pages((virtual_addr_t)context->vmcs, 1);

	if (context->vmx_on_region)
		vmm_host_free_pages((virtual_addr_t)context->vmx_on_region,
				    vmxon_region_nr_pages);

	return ret;
}

int __init intel_init(struct cpuinfo_x86 *cpuinfo)
{
	/* Enable VMX */
	if (enable_vmx(cpuinfo) != VMM_OK) {
		VM_LOG(LVL_ERR, "ERROR: Failed to enable virtual machine.\n");
		return VMM_EFAIL;
	}

	VM_LOG(LVL_VERBOSE, "INTEL VMX enabled successfully\n");

	return VMM_OK;
}
