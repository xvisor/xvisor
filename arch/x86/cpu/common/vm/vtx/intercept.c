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
int vmx_handle_guest_realmode_page_fault(struct vcpu_hw_context *context)
{
	int rc;
	u32 flags;
	physical_addr_t hphys_addr;
	physical_size_t availsz;
	physical_addr_t fault_gphys;
	struct vmm_guest *guest = x86_vcpu_hw_context_guest(context);

	physical_addr_t gla = vmr(GUEST_LINEAR_ADDRESS);

	VM_LOG(LVL_DEBUG, "[Real Mode] Faulting Address: 0x%"PRIx64"\n", gla);

	fault_gphys = (0xFFFF0000ULL + gla);

	VM_LOG(LVL_DEBUG, "(Real Mode) Looking for map from guest address: 0x%08lx\n",
	       (fault_gphys & PAGE_MASK));

	rc = vmm_guest_physical_map(guest, (fault_gphys & PAGE_MASK),
				    PAGE_SIZE, &hphys_addr, &availsz, &flags);
	if (rc) {
		VM_LOG(LVL_ERR, "ERROR: No region mapped to guest physical 0x%"PRIx64"\n", fault_gphys);
		goto guest_bad_fault;
	}

	if (availsz < PAGE_SIZE) {
		VM_LOG(LVL_ERR, "ERROR: Size of the available mapping less than page size (%lu)\n", availsz);
		rc = VMM_EFAIL;
		goto guest_bad_fault;
	}

	if (flags & (VMM_REGION_REAL | VMM_REGION_ALIAS)) {
		VM_LOG(LVL_DEBUG, "GP: 0x%"PRIx64" HP: 0x%"PRIx64" Size: %lu\n", gla, hphys_addr, availsz);

		gla &= PAGE_MASK;
		hphys_addr &= PAGE_MASK;

		VM_LOG(LVL_DEBUG, "Handle Page Fault: gphys: 0x%"PRIx64" hphys: 0x%"PRIx64"\n",
		       fault_gphys, hphys_addr);

		rc = ept_create_pte_map(context, gla, hphys_addr, PAGE_SIZE,
					(EPT_PROT_READ | EPT_PROT_WRITE | EPT_PROT_EXEC_S));
		VM_LOG(LVL_DEBUG, "ept_create_pte_map returned with %d\n", rc);
	} else
		rc = VMM_EFAIL;

 guest_bad_fault:
	return rc;
}

static inline
int vmx_handle_guest_protected_mode_page_fault(struct vcpu_hw_context *context)
{
	physical_addr_t fault_gphys, hphys_addr;
	physical_size_t availsz;
	int rc;
	u32 flags;
	struct vmm_guest *guest = x86_vcpu_hw_context_guest(context);

	fault_gphys = vmr(GUEST_LINEAR_ADDRESS);

	VM_LOG(LVL_DEBUG, "(Protected Mode) Looking for map from guest address: 0x%08lx\n",
	       (fault_gphys & PAGE_MASK));

	rc = vmm_guest_physical_map(guest, (fault_gphys & PAGE_MASK),
				    PAGE_SIZE, &hphys_addr, &availsz, &flags);
	if (rc) {
		VM_LOG(LVL_ERR, "ERROR: No region mapped to guest physical 0x%"PRIx64"\n", fault_gphys);
		return VMM_EFAIL;
	}

	if (availsz < PAGE_SIZE) {
		VM_LOG(LVL_ERR, "ERROR: Size of the available mapping less than page size (%lu)\n", availsz);
		return VMM_EFAIL;
	}

	fault_gphys &= PAGE_MASK;
	hphys_addr &= PAGE_MASK;

	VM_LOG(LVL_DEBUG, "GP: 0x%"PRIx64" HP: 0x%"PRIx64" Size: %lu\n", fault_gphys, hphys_addr, availsz);

	return ept_create_pte_map(context, fault_gphys, hphys_addr, PAGE_SIZE,
				  (EPT_PROT_READ | EPT_PROT_WRITE | EPT_PROT_EXEC_S));
}

static inline
int guest_in_real_mode(struct vcpu_hw_context *context)
{

	if (VMX_GUEST_CR0(context) & X86_CR0_PE)
		return 0;

	if (is_guest_address_translated(VMX_GUEST_EQ(context)))
		return 0;

	return 1;
}

static inline
int vmx_handle_io_instruction_exit(struct vcpu_hw_context *context)
{
	vmx_io_exit_qualification_t ioe;
	u32 wval, io_sz;

	ioe.val = VMX_GUEST_EQ(context);
	io_sz = (ioe.bits.io_size == 0 ? 1 : (ioe.bits.io_size == 1 ? 2 : 4));

	if (ioe.bits.direction == 0) {
		if (ioe.bits.port == 0x80) {
			VM_LOG(LVL_DEBUG, "(0x%"PRIx64") CBDW: 0x%"PRIx64"\n",
			       VMX_GUEST_RIP(context), context->g_regs[GUEST_REGS_RAX]);
		} else {
			wval = (u32)context->g_regs[GUEST_REGS_RAX];

			if (vmm_devemu_emulate_iowrite(context->assoc_vcpu, ioe.bits.port,
						       &wval, io_sz, VMM_DEVEMU_NATIVE_ENDIAN) != VMM_OK) {
				vmm_printf("Failed to emulate OUT instruction in"
					   " guest.\n");
				goto guest_bad_fault;
			}
		}
	} else  {
		VM_LOG(LVL_DEBUG, "Read on IO Port: %d\n", ioe.bits.port);
		if (vmm_devemu_emulate_ioread(context->assoc_vcpu, ioe.bits.port, &wval, io_sz,
					      VMM_DEVEMU_NATIVE_ENDIAN) != VMM_OK) {
			vmm_printf("Failed to emulate IO instruction in "
				   "guest.\n");
			goto guest_bad_fault;
		}

		context->g_regs[GUEST_REGS_RAX] = wval;
	}

	__vmwrite(GUEST_RIP, VMX_GUEST_NEXT_RIP(context));

	return VMM_OK;

 guest_bad_fault:
	return VMM_EFAIL;
}

static inline
int vmx_handle_crx_exit(struct vcpu_hw_context *context)
{
	vmx_crx_move_eq_t crx_eq;

	crx_eq.val = VMX_GUEST_EQ(context);

	if (crx_eq.bits.reg > GUEST_REGS_R15) {
		VM_LOG(LVL_ERR, "Guest Move to CR0 with invalid reg %d\n", crx_eq.bits.reg);
		goto guest_bad_fault;
	}

	/* Move to CRx */
	if (crx_eq.bits.type == 0) {
		switch(crx_eq.bits.cr_num) {
		case 0:
			__vmwrite(GUEST_CR0, (VMX_GUEST_CR0(context) | context->g_regs[crx_eq.bits.reg]));
			VM_LOG(LVL_DEBUG, "Moving %d register (value: 0x%"PRIx64") to CR0\n",
			       crx_eq.bits.reg, (VMX_GUEST_CR0(context) | context->g_regs[crx_eq.bits.reg]));
			break;
		case 3:
			__vmwrite(GUEST_CR3, context->g_regs[crx_eq.bits.reg]);
			VM_LOG(LVL_DEBUG, "Moving %d register (value: 0x%"PRIx64") to CR3\n",
			       crx_eq.bits.reg, context->g_regs[crx_eq.bits.reg]);
			break;
		case 4:
			__vmwrite(GUEST_CR4, context->g_regs[crx_eq.bits.reg]);
			VM_LOG(LVL_DEBUG, "Moving %d register (value: 0x%"PRIx64") to CR4\n",
			       crx_eq.bits.reg, context->g_regs[crx_eq.bits.reg]);
			break;
		default:
			VM_LOG(LVL_ERR, "Guest trying to write to reserved CR%d\n", crx_eq.bits.cr_num);
			goto guest_bad_fault;
		}
	} else if (crx_eq.bits.type == 1) { /* Move from CRx */
		switch(crx_eq.bits.cr_num) {
		case 0:
			//context->g_regs[crx_eq.bits.reg] = vmr(GUEST_CR0);
			//VM_LOG(LVL_DEBUG, "Moving CR3 to register %d\n",
			//       crx_eq.bits.reg);
			break;
		case 3:
			context->g_regs[crx_eq.bits.reg] = vmr(GUEST_CR3);
			VM_LOG(LVL_DEBUG, "Moving CR3 to register %d\n",
			       crx_eq.bits.reg);
			break;
		case 4:
			context->g_regs[crx_eq.bits.reg] = vmr(GUEST_CR3);
			VM_LOG(LVL_DEBUG, "Moving CR4 to register %d\n",
			       crx_eq.bits.reg);
			break;
		default:
			VM_LOG(LVL_ERR, "Guest trying to write to reserved CR%d\n", crx_eq.bits.cr_num);
			goto guest_bad_fault;
		}
	} else {
		VM_LOG(LVL_ERR, "LMSW not supported yet\n");
		goto guest_bad_fault;
	}
	__vmwrite(GUEST_RIP, VMX_GUEST_NEXT_RIP(context));

	return VMM_OK;

 guest_bad_fault:
	return VMM_EFAIL;

}

int vmx_handle_vmexit(struct vcpu_hw_context *context, u32 exit_reason)
{
	switch (exit_reason) {
	case EXIT_REASON_EPT_VIOLATION:
		/* Guest in real mode */
		if (guest_in_real_mode(context)) {
			if (is_guest_linear_address_valid(VMX_GUEST_EQ(context))) {
				return vmx_handle_guest_realmode_page_fault(context);
			} else {
				VM_LOG(LVL_ERR, "(Realmode pagefault) VMX reported invalid linear address.\n");
				return VMM_EFAIL;
			}
		} else { /* Protected mode */
			return vmx_handle_guest_protected_mode_page_fault(context);
		}
		break;

	case EXIT_REASON_IO_INSTRUCTION:
		return vmx_handle_io_instruction_exit(context);

	case EXIT_REASON_CR_ACCESS:
		return vmx_handle_crx_exit(context);

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
		VM_LOG(LVL_DEBUG, "VM Exit reason: %d\n", _exit_reason.bits.reason);

		VMX_GUEST_SAVE_EQ(context);
		VMX_GUEST_SAVE_CR0(context);
		VMX_GUEST_SAVE_RIP(context);
		VM_LOG(LVL_DEBUG, "Guest RIP: 0x%"PRIx64"\n", VMX_GUEST_RIP(context));

		if (vmx_handle_vmexit(context, _exit_reason.bits.reason) != VMM_OK) {
			VM_LOG(LVL_DEBUG, "Error handling VMExit (Reason: %d)\n", _exit_reason.bits.reason);
			goto unhandled_vm_exit;
		}

		return;
	}

unhandled_vm_exit:
	VM_LOG(LVL_DEBUG, "Unhandled vmexit\n");
	context->vcpu_emergency_shutdown(context);
}
