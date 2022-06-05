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
#include <x86_debug_log.h>

DEFINE_X86_DEBUG_LOG_SUBSYS_LEVEL(vtx_intercept, X86_DEBUG_LOG_LVL_INFO);

extern u64 vmx_cr0_fixed0;
extern u64 vmx_cr0_fixed1;
extern u64 vmx_cr4_fixed0;
extern u64 vmx_cr4_fixed1;

#define GUEST_CRx_FILTER(x, __value)			\
	({	u64 _v = __value;			\
		(_v |= vmx_cr##x ##_fixed0);		\
		(_v &= vmx_cr##x ##_fixed1);		\
		(_v);					\
	})

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
	"Undefined error code",
	"VMXON executed in VMX root operation",
	"VM entry with invalid executive-VMCS pointer",
	"VM entry with non-launched executive VMCS",
	"VM entry with executive-VMCS pointer not VMXON pointer",
	"VMCALL with non-clear VMCS",
	"VMCALL with invalid VM-exit control fields",
	"Undefined error code",
	"VMCALL with incorrect MSEG revision identifier",
	"VMXOFF under dual-monitor treatment of SMIs and SMM",
	"VMCALL with invalid SMM-monitor features",
	"VM entry with invalid VM-execution control fields in executive VMCS",
	"VM entry with events blocked by MOV SS",
	"Undefined error code",
	"Invalid operand to INVEPT/INVVPID"
};

static inline
int vmx_handle_guest_realmode_page_fault(struct vcpu_hw_context *context)
{
	int rc;
	u32 flags;
	physical_addr_t hphys_addr;
	physical_size_t availsz;
	struct vmm_guest *guest = x86_vcpu_hw_context_guest(context);
	u8 is_reset = 0;

	physical_addr_t gla = vmr(GUEST_LINEAR_ADDRESS);
	X86_DEBUG_LOG(vtx_intercept, LVL_DEBUG, "[Real Mode] Guest Linear Address: 0x%"PRIx64"\n", gla);
	if (gla == 0xFFF0) {
		is_reset = 1;
		/* effective address = segment selector * 16 + offset.
		 * we will have a region for effective address. */
		gla = ((0xF000 << 4) | gla);
	}
	X86_DEBUG_LOG(vtx_intercept, LVL_DEBUG, "[Real Mode] Faulting Address: 0x%"PRIx64"\n", gla);

	/*
	 * At reset, the offset of execution is 0xfff0.
	 */
	rc = vmm_guest_physical_map(guest, (gla & PAGE_MASK),
				    PAGE_SIZE, &hphys_addr, &availsz, &flags);
	if (rc) {
		X86_DEBUG_LOG(vtx_intercept, LVL_ERR, "ERROR: No region mapped to guest physical 0x%"PRIx64"\n", gla);
		goto guest_bad_fault;
	}

	if (availsz < PAGE_SIZE) {
		X86_DEBUG_LOG(vtx_intercept, LVL_ERR, "ERROR: Size of the available mapping less "
		       "than page size (%lu)\n", availsz);
		rc = VMM_EFAIL;
		goto guest_bad_fault;
	}

	if (flags & (VMM_REGION_REAL | VMM_REGION_ALIAS)) {
		X86_DEBUG_LOG(vtx_intercept, LVL_DEBUG, "GP: 0x%"PRIx64" HP: 0x%"PRIx64" Size: %lu\n",
		       gla, hphys_addr, availsz);

		gla &= PAGE_MASK;
		/* page table entry will go only for offset. */
		if (is_reset)
			gla &= 0xFFFFUL;
		hphys_addr &= PAGE_MASK;

		X86_DEBUG_LOG(vtx_intercept, LVL_DEBUG, "Handle Page Fault: gphys: 0x%"PRIx64" hphys: 0x%"PRIx64"\n",
		       gla, hphys_addr);

		rc = ept_create_pte_map(context, gla, hphys_addr, PAGE_SIZE,
					(EPT_PROT_READ | EPT_PROT_WRITE | EPT_PROT_EXEC_S));
		X86_DEBUG_LOG(vtx_intercept, LVL_DEBUG, "ept_create_pte_map returned with %d\n", rc);
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

	X86_DEBUG_LOG(vtx_intercept, LVL_DEBUG, "(Protected Mode) Looking for map from guest address: 0x%08lx\n",
	       (fault_gphys & PAGE_MASK));

	rc = vmm_guest_physical_map(guest, (fault_gphys & PAGE_MASK),
				    PAGE_SIZE, &hphys_addr, &availsz, &flags);
	if (rc) {
		X86_DEBUG_LOG(vtx_intercept, LVL_ERR, "ERROR: No region mapped to guest physical 0x%"PRIx64"\n", fault_gphys);
		return VMM_EFAIL;
	}

	if (availsz < PAGE_SIZE) {
		X86_DEBUG_LOG(vtx_intercept, LVL_ERR, "ERROR: Size of the available mapping less than page size (%lu)\n", availsz);
		return VMM_EFAIL;
	}

	fault_gphys &= PAGE_MASK;
	hphys_addr &= PAGE_MASK;

	X86_DEBUG_LOG(vtx_intercept, LVL_DEBUG, "GP: 0x%"PRIx64" HP: 0x%"PRIx64" Size: %lu\n", fault_gphys, hphys_addr, availsz);

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

void vmx_handle_cpuid(struct vcpu_hw_context *context)
{
	struct x86_vcpu_priv *priv = x86_vcpu_priv(context->assoc_vcpu);
	struct cpuid_response *func;

	switch (context->g_regs[GUEST_REGS_RAX]) {
	case CPUID_BASE_LFUNCSTD:
		func = &priv->standard_funcs[CPUID_BASE_LFUNCSTD];
		context->g_regs[GUEST_REGS_RAX] = func->resp_eax;
		context->g_regs[GUEST_REGS_RBX] = func->resp_ebx;
		context->g_regs[GUEST_REGS_RCX] = func->resp_ecx;
		context->g_regs[GUEST_REGS_RDX] = func->resp_edx;
		X86_DEBUG_LOG(vtx_intercept, LVL_DEBUG, "RAX: 0x%"PRIx32" RBX: 0x%"PRIx32" RCX: 0x%"PRIx32" RDX: 0x%"PRIx32"\n",
		       func->resp_eax, func->resp_ebx, func->resp_ecx, func->resp_edx);
		break;

	case CPUID_BASE_FEATURES:
		func = &priv->standard_funcs[CPUID_BASE_FEATURES];
		context->g_regs[GUEST_REGS_RAX] = func->resp_eax;
		context->g_regs[GUEST_REGS_RBX] = func->resp_ebx;
		context->g_regs[GUEST_REGS_RCX] = func->resp_ecx;
		context->g_regs[GUEST_REGS_RDX] = func->resp_edx;
		X86_DEBUG_LOG(vtx_intercept, LVL_DEBUG, "CPUID: 0x%"PRIx64" RAX: 0x%"PRIx32" RBX: 0x%"PRIx32" RCX: 0x%"PRIx32" RDX: 0x%"PRIx32"\n",
		       context->g_regs[GUEST_REGS_RAX], func->resp_eax, func->resp_ebx, func->resp_ecx, func->resp_edx);

		break;

	case CPUID_EXTENDED_LFUNCEXTD:
	case CPUID_EXTENDED_BRANDSTRING:
	case CPUID_EXTENDED_BRANDSTRINGMORE:
	case CPUID_EXTENDED_BRANDSTRINGEND:
	case CPUID_EXTENDED_L2_CACHE_TLB_IDENTIFIER:
	case CPUID_EXTENDED_ADDR_BITS:
		func = &priv->extended_funcs[context->g_regs[GUEST_REGS_RAX]
					     - CPUID_EXTENDED_LFUNCEXTD];
		X86_DEBUG_LOG(vtx_intercept, LVL_DEBUG, "CPUID: 0x%"PRIx64": EAX: 0x%"PRIx32" EBX: 0x%"PRIx32" ECX: 0x%"PRIx32" EDX: 0x%"PRIx32"\n",
		       context->g_regs[GUEST_REGS_RAX], func->resp_eax, func->resp_ebx, func->resp_ecx, func->resp_edx);
		context->g_regs[GUEST_REGS_RAX] = func->resp_eax;
		context->g_regs[GUEST_REGS_RBX] = func->resp_ebx;
		context->g_regs[GUEST_REGS_RCX] = func->resp_ecx;
		context->g_regs[GUEST_REGS_RDX] = func->resp_edx;
		break;

	case CPUID_BASE_FEAT_FLAGS:
	case CPUID_EXTENDED_FEATURES:
	case CPUID_BASE_PWR_MNG:
		context->g_regs[GUEST_REGS_RAX] = 0;
		context->g_regs[GUEST_REGS_RBX] = 0;
		context->g_regs[GUEST_REGS_RCX] = 0;
		context->g_regs[GUEST_REGS_RDX] = 0;
		break;

	/* Reserved for VM */
	case CPUID_VM_CPUID_BASE ... CPUID_VM_CPUID_MAX:
		X86_DEBUG_LOG(vtx_intercept, LVL_DEBUG, "CPUID: 0x%"PRIx64" will read zeros\n",
		       context->g_regs[GUEST_REGS_RAX]);
		context->g_regs[GUEST_REGS_RAX] = 0;
		context->g_regs[GUEST_REGS_RBX] = 0;
		context->g_regs[GUEST_REGS_RCX] = 0;
		context->g_regs[GUEST_REGS_RDX] = 0;
		break;

	default:
		X86_DEBUG_LOG(vtx_intercept, LVL_ERR, "GCPUID/R: Func: 0x%"PRIx64"\n",
		       context->g_regs[GUEST_REGS_RAX]);
		goto _fail;
	}

	__vmwrite(GUEST_RIP, VMX_GUEST_NEXT_RIP(context));

	return;

 _fail:
	if (context->vcpu_emergency_shutdown) {
		context->vcpu_emergency_shutdown(context);
	}
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
			X86_DEBUG_LOG(vtx_intercept, LVL_DEBUG, "(0x%"PRIx64") CBDW: 0x%"PRIx64"\n",
			       VMX_GUEST_RIP(context), context->g_regs[GUEST_REGS_RAX]);
		} else {
			X86_DEBUG_LOG(vtx_intercept, LVL_DEBUG, "Write on IO Port: 0x%04x\n", ioe.bits.port);
			wval = (u32)context->g_regs[GUEST_REGS_RAX];

			if (vmm_devemu_emulate_iowrite(context->assoc_vcpu, ioe.bits.port,
						       &wval, io_sz, VMM_DEVEMU_NATIVE_ENDIAN) != VMM_OK) {
				X86_DEBUG_LOG(vtx_intercept, LVL_ERR, "Failed to emulate OUT instruction in"
				       " guest.\n");
				goto guest_bad_fault;
			}
		}
	} else  {
		X86_DEBUG_LOG(vtx_intercept, LVL_DEBUG, "Read on IO Port: 0x%04x\n", ioe.bits.port);
		if (vmm_devemu_emulate_ioread(context->assoc_vcpu, ioe.bits.port, &wval, io_sz,
					      VMM_DEVEMU_NATIVE_ENDIAN) != VMM_OK) {
			X86_DEBUG_LOG(vtx_intercept, LVL_ERR, "Failed to emulate IO instruction in "
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
	u64 gcr0;

	crx_eq.val = VMX_GUEST_EQ(context);

	if (crx_eq.bits.reg > GUEST_REGS_R15) {
		X86_DEBUG_LOG(vtx_intercept, LVL_ERR, "Guest Move to CR0 with invalid reg %d\n", crx_eq.bits.reg);
		goto guest_bad_fault;
	}

	/* Move to CRx */
	if (crx_eq.bits.type == 0) {
		switch(crx_eq.bits.cr_num) {
		case 0:
			gcr0 = GUEST_CRx_FILTER(0,
						(context->g_regs[crx_eq.bits.reg]
						 |X86_CR0_ET | X86_CR0_CD | X86_CR0_NW));
			gcr0 &= ~X86_CR0_PG;
			__vmwrite(GUEST_CR0, gcr0);
			VMX_GUEST_CR0(context) = (X86_CR0_ET | X86_CR0_CD | X86_CR0_NW
						  | context->g_regs[crx_eq.bits.reg]);
			//__vmwrite(GUEST_CR0, (VMX_GUEST_CR0(context) | context->g_regs[crx_eq.bits.reg]));
			X86_DEBUG_LOG(vtx_intercept, LVL_DEBUG, "Moving %d register (value: 0x%"PRIx64") to CR0\n",
			       crx_eq.bits.reg, gcr0);
			break;
		case 3:
			__vmwrite(GUEST_CR3, context->g_regs[crx_eq.bits.reg]);
			X86_DEBUG_LOG(vtx_intercept, LVL_DEBUG, "Moving %d register (value: 0x%"PRIx64") to CR3\n",
			       crx_eq.bits.reg, context->g_regs[crx_eq.bits.reg]);
			break;
		case 4:
			__vmwrite(GUEST_CR4, context->g_regs[crx_eq.bits.reg]);
			X86_DEBUG_LOG(vtx_intercept, LVL_DEBUG, "Moving %d register (value: 0x%"PRIx64") to CR4\n",
			       crx_eq.bits.reg, context->g_regs[crx_eq.bits.reg]);
			break;
		default:
			X86_DEBUG_LOG(vtx_intercept, LVL_ERR, "Guest trying to write to reserved CR%d\n", crx_eq.bits.cr_num);
			goto guest_bad_fault;
		}
	} else if (crx_eq.bits.type == 1) { /* Move from CRx */
		switch(crx_eq.bits.cr_num) {
		case 0:
			//context->g_regs[crx_eq.bits.reg] = vmr(GUEST_CR0);
			context->g_regs[crx_eq.bits.reg] = VMX_GUEST_CR0(context);
			X86_DEBUG_LOG(vtx_intercept, LVL_DEBUG, "Moving CR3 to register %d\n",
			       crx_eq.bits.reg);
			break;
		case 3:
			context->g_regs[crx_eq.bits.reg] = vmr(GUEST_CR3);
			X86_DEBUG_LOG(vtx_intercept, LVL_DEBUG, "Moving CR3 to register %d\n",
			       crx_eq.bits.reg);
			break;
		case 4:
			context->g_regs[crx_eq.bits.reg] = vmr(GUEST_CR4);
			X86_DEBUG_LOG(vtx_intercept, LVL_DEBUG, "Moving CR4 to register %d\n",
			       crx_eq.bits.reg);
			break;
		default:
			X86_DEBUG_LOG(vtx_intercept, LVL_ERR, "Guest trying to write to reserved CR%d\n", crx_eq.bits.cr_num);
			goto guest_bad_fault;
		}
	} else {
		X86_DEBUG_LOG(vtx_intercept, LVL_ERR, "LMSW not supported yet\n");
		goto guest_bad_fault;
	}

	__vmwrite(GUEST_RIP, VMX_GUEST_NEXT_RIP(context));

	return VMM_OK;

 guest_bad_fault:
	return VMM_EFAIL;

}

u64 ext_intrs = 0;

int vmx_handle_vmexit(struct vcpu_hw_context *context, u32 exit_reason)
{
	switch (exit_reason) {
	case EXIT_REASON_EPT_VIOLATION:
		/* Guest in real mode */
		if (guest_in_real_mode(context)) {
			if (is_guest_linear_address_valid(VMX_GUEST_EQ(context))) {
				return vmx_handle_guest_realmode_page_fault(context);
			} else {
				X86_DEBUG_LOG(vtx_intercept, LVL_ERR, "(Realmode pagefault) VMX reported invalid linear address.\n");
				return VMM_EFAIL;
			}
		} else { /* Protected mode */
			return vmx_handle_guest_protected_mode_page_fault(context);
		}
		break;

	case EXIT_REASON_IO_INSTRUCTION:
		return vmx_handle_io_instruction_exit(context);

	case EXIT_REASON_CR_ACCESS:
		X86_DEBUG_LOG(vtx_intercept, LVL_DEBUG, "CRx Access\n");
		return vmx_handle_crx_exit(context);

	case EXIT_REASON_CPUID:
		X86_DEBUG_LOG(vtx_intercept, LVL_DEBUG, "Guest CPUID Request: 0x%"PRIx64"\n", context->g_regs[GUEST_REGS_RAX]);
		vmx_handle_cpuid(context);
		return VMM_OK;

	case EXIT_REASON_INVD:
		__vmwrite(GUEST_RIP, VMX_GUEST_NEXT_RIP(context));
		return VMM_OK;

	case EXIT_REASON_EXTERNAL_INTERRUPT:
		ext_intrs++;
		return VMM_OK;

	default:
		X86_DEBUG_LOG(vtx_intercept, LVL_DEBUG, "Unhandled VM Exit reason: %d\n", exit_reason);
		goto guest_bad_fault;
	}

guest_bad_fault:
	return VMM_EFAIL;
}

void vmx_vcpu_exit(struct vcpu_hw_context *context)
{
	exit_reason_t _exit_reason;
	int rc;
	u64 ins_err = 0;

	if (unlikely(context->instruction_error)) {
		if ((rc = __vmread(VM_INSTRUCTION_ERROR, &ins_err)) == VMM_OK) {
			X86_DEBUG_LOG(vtx_intercept, LVL_ERR, "Instruction Error: (%ld:%s)\n", ins_err, ins_err_str[ins_err]);
		}

		if (context->instruction_error == -1) {
			if ((rc = __vmread(VM_INSTRUCTION_ERROR, &ins_err)) == VMM_OK) {
				X86_DEBUG_LOG(vtx_intercept, LVL_ERR, "Instruction Error: (%ld:%s)\n", ins_err, ins_err_str[ins_err]);
			}
		} else if (context->instruction_error == -2) {
			X86_DEBUG_LOG(vtx_intercept, LVL_ERR, "vmlaunch/resume without an active VMCS!\n");
		}

		X86_DEBUG_LOG(vtx_intercept, LVL_DEBUG, "VM Entry Failure with Error: %d\n", context->instruction_error);
		goto unhandled_vm_exit;
	}

	if (unlikely((rc = __vmread(VM_EXIT_REASON, &_exit_reason.r)) != VMM_OK))
		if (likely(context->vcpu_emergency_shutdown))
			context->vcpu_emergency_shutdown(context);

	if (unlikely(_exit_reason.bits.vm_entry_failure)) {
		switch(_exit_reason.bits.reason) {
		case 33:
			X86_DEBUG_LOG(vtx_intercept, LVL_ERR, "VM Entry failed due to invalid guest state.\n");
			break;
		case 34:
			X86_DEBUG_LOG(vtx_intercept, LVL_ERR, "VM Entry failed due to MSR loading.\n");
			break;
		case 41:
			X86_DEBUG_LOG(vtx_intercept, LVL_ERR, "VM Entry failed due to machine-check event.\n");
			break;
		default:
			X86_DEBUG_LOG(vtx_intercept, LVL_ERR, "VM Entry failed due to unknown reason %d.\n", _exit_reason.bits.reason);
			break;
		}
	} else {
		VMX_GUEST_SAVE_EQ(context);
		VMX_GUEST_SAVE_CR0(context);
		VMX_GUEST_SAVE_RIP(context);
		X86_DEBUG_LOG(vtx_intercept, LVL_DEBUG, "Guest RIP: 0x%"PRIx64"\n", VMX_GUEST_RIP(context));

		if (vmx_handle_vmexit(context, _exit_reason.bits.reason) != VMM_OK) {
			X86_DEBUG_LOG(vtx_intercept, LVL_DEBUG, "Error handling VMExit (Reason: %d)\n", _exit_reason.bits.reason);
			goto unhandled_vm_exit;
		}

		return;
	}

 unhandled_vm_exit:
	X86_DEBUG_LOG(vtx_intercept, LVL_DEBUG, "Unhandled VM Exit\n");
	context->vcpu_emergency_shutdown(context);
}
