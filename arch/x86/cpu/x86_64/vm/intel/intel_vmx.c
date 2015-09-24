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
#include <vmm_stdio.h>
#include <control_reg_access.h>
#include <vm/intel_vmcs.h>
#include <vm/intel_vmx.h>
#include <vm/intel_intercept.h>

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

	cpu_read_msr32(IA32_FEATURE_CONTROL_MSR, &eax, &edx);

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

static void vmx_vcpu_run(struct vcpu_hw_context *context)
{
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
	vmm_printf("VMCS location: %x\n", vmcs);

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
	__vmxon(context->vmxon_region_pa);

	/* VMCLEAR: clear launched state */
	__vmpclear(context->vmcs_pa);

	/* VMPTRLD: mark this vmcs active, current & clear */
	__vmptrld(context->vmcs_pa);

	vmx_set_control_params(context);

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
