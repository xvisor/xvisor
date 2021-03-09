/**
 * Copyright (c) 2015 Himanshu Chauhan.
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
 * @file intel_intercept.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief VMX intercept handling code.
 */
#include <vmm_error.h>
#include <vmm_types.h>
#include <vmm_stdio.h>
#include <vmm_host_aspace.h>
#include <vmm_guest_aspace.h>
#include <cpu_vm.h>
#include <cpu_inst_decode.h>
#include <cpu_features.h>
#include <cpu_mmu.h>
#include <cpu_pgtbl_helper.h>
#include <arch_guest_helper.h>
#include <vmm_devemu.h>
#include <vmm_manager.h>
#include <vmm_main.h>
#include <vm/vmcs.h>
#include <vm/vmx.h>
#include <vm/ept.h>
#include <vm/vmx_intercept.h>

static inline
int vmx_handle_guest_realmode_page_fault(struct vcpu_hw_context *context,
					 physical_addr_t fault_gphys,
					  physical_addr_t hphys_addr)
{
	int rc;

	fault_gphys &= PAGE_MASK;
	hphys_addr &= PAGE_MASK;

	VM_LOG(LVL_DEBUG, "Handle Page Fault: gphys: 0x%"PRIx64" hphys: 0x%"PRIx64"\n",
	       fault_gphys, hphys_addr);

	rc = ept_create_pte_map(context, fault_gphys, hphys_addr, PAGE_SIZE,
				(EPT_PROT_READ | EPT_PROT_WRITE | EPT_PROT_EXEC_S));
	VM_LOG(LVL_DEBUG, "ept_create_pte_map returned with %d\n", rc);

	return rc;
}

static inline
int guest_in_real_mode(struct vcpu_hw_context *context)
{

	if (VMX_GUEST_CR0(context) & X86_CR0_PG)
		return 0;

	if (is_guest_address_translated(VMX_GUEST_EQ(context)))
		return 0;

	return 1;
}

int vmx_handle_vmexit(struct vcpu_hw_context *context, u32 exit_reason)
{
	u32 gla, flags;
	int rc;
	physical_addr_t hphys_addr;
	physical_size_t availsz;
	physical_addr_t fault_gphys;
	struct vmm_guest *guest = x86_vcpu_hw_context_guest(context);
	vmx_io_exit_qualification_t ioe;

	if (unlikely(guest == NULL))
		vmm_panic("%s: NULL guest on vmexit\n", __func__);

	switch (exit_reason) {
	case EXIT_REASON_EPT_VIOLATION:
		gla = vmr(GUEST_LINEAR_ADDRESS);

		/* Guest in real mode */
		if (guest_in_real_mode(context)) {
			if (is_guest_linear_address_valid(VMX_GUEST_EQ(context))) {
				fault_gphys = (0xFFFF0000 + gla);

				VM_LOG(LVL_DEBUG, "(Real Mode) Looking for map from guest address: 0x%08lx\n",
				       (fault_gphys & PAGE_MASK));
				rc = vmm_guest_physical_map(guest, (fault_gphys & PAGE_MASK),
							    PAGE_SIZE, &hphys_addr, &availsz, &flags);
				if (rc) {
					VM_LOG(LVL_ERR, "ERROR: No region mapped to guest physical 0x%"PRIx32"\n", gla);
					goto guest_bad_fault;
				}

				if (availsz < PAGE_SIZE) {
					VM_LOG(LVL_ERR, "ERROR: Size of the available mapping less than page size (%lu)\n", availsz);
					goto guest_bad_fault;
				}

				if (flags & (VMM_REGION_REAL | VMM_REGION_ALIAS)) {
					VM_LOG(LVL_DEBUG, "GP: 0x%"PRIx32" HP: 0x%"PRIx64" Size: %lu\n", gla, hphys_addr, availsz);
					return vmx_handle_guest_realmode_page_fault(context, (gla & PAGE_MASK), (hphys_addr & PAGE_MASK));
				} else
					VM_LOG(LVL_ERR, "Unhandled guest fault region flags: 0x%"PRIx32"\n", flags);
			} else {
				VM_LOG(LVL_ERR, "(Realmode pagefault) VMX reported invalid linear address.\n");
				goto guest_bad_fault;
			}
		} else {
			VM_LOG(LVL_ERR, "Handle protected mode guest.\n");
			goto guest_bad_fault;
		}
		break;

	case EXIT_REASON_IO_INSTRUCTION:
		ioe.val = VMX_GUEST_EQ(context);

		if (ioe.bits.direction == 0) {
			if (ioe.bits.port == 0x80) {
				VM_LOG(LVL_INFO, "(0x%"PRIx64") CBDW: 0x%"PRIx64"\n",
				       VMX_GUEST_RIP(context), context->g_regs[GUEST_REGS_RAX]);
			}
		} else  {
			VM_LOG(LVL_ERR, "Read on IO Port: %d\n", ioe.bits.port);
			while(1);
		}

		__vmwrite(GUEST_RIP, VMX_GUEST_NEXT_RIP(context));

		return VMM_OK;
	default:
		goto guest_bad_fault;
	}

guest_bad_fault:
	return VMM_EFAIL;
}

void vmx_vcpu_exit(struct vcpu_hw_context *context)
{
	exit_reason_t _exit_reason;
	int rc;

	if (unlikely((rc = __vmread(VM_EXIT_REASON, &_exit_reason.r)) != VMM_OK))
		if (likely(context->vcpu_emergency_shutdown))
			context->vcpu_emergency_shutdown(context);

	if (unlikely(_exit_reason.bits.vm_entry_failure)) {
		switch(_exit_reason.bits.reason) {
		case 33:
			vmm_printf("VM Entry failed due to invalid guest state.\n");
			break;
		case 34:
			vmm_printf("VM Entry failed due to MSR loading.\n");
			break;
		case 41:
			vmm_printf("VM Entry failed due to machine-check event.\n");
			break;
		default:
			vmm_printf("VM Entry failed due to unknown reason %d.\n", _exit_reason.bits.reason);
			break;
		}
	} else {
		VM_LOG(LVL_INFO, "VM Exit reason: %d\n", _exit_reason.bits.reason);

		VMX_GUEST_SAVE_EQ(context);
		VMX_GUEST_SAVE_CR0(context);
		VMX_GUEST_SAVE_RIP(context);

		if (vmx_handle_vmexit(context, _exit_reason.bits.reason) != VMM_OK) {
			VM_LOG(LVL_ERR, "Error handling VMExit\n");
			goto unhandled_vm_exit;
		}

		return;
	}

unhandled_vm_exit:
	VM_LOG(LVL_ERR, "Unhandled vmexit\n");
	while(1);
	//context->vcpu_emergency_shutdown(context);
}
