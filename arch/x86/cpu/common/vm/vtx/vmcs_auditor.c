/**
 * This is ported from bochs VMX checks and modified for Xvisor
 *
 * Copyright (c) 2009-2019 Stanislav Shwartsman
 * Copyright (c) 2021 Himanshu Chauhan.
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
 * @file vmcs_auditor.c
 * @author Himanshu Chauhan (hchauhan@xvisor-x86.org)
 * @brief VMCS configuration checker
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
#include <vmm_heap.h>
#include <control_reg_access.h>
#include <vm/vmcs.h>
#include <vm/vmx.h>
#include <vm/vmx_intercept.h>
#include <vm/vmcs_auditor.h>

UINT32 vmx_pin_vmexec_ctrl_supported_bits;
UINT32 vmx_proc_vmexec_ctrl_supported_bits;
UINT32 vmx_vmexec_ctrl2_supported_bits;
UINT32 vmx_vmexit_ctrl_supported_bits;
UINT32 vmx_vmentry_ctrl_supported_bits;
UINT64 vmx_ept_vpid_cap_supported_bits;
UINT64 vmx_vmfunc_supported_bits;
UINT32 cr0_suppmask_0;
UINT32 cr0_suppmask_1;
UINT32 cr4_suppmask_0;
UINT32 cr4_suppmask_1;
UINT32 vmx_extensions_bitmask;


Bit64u efer_suppmask = 0;

BxExceptionInfo exceptions_info[32] = {
	/* DE */ { BX_ET_CONTRIBUTORY, BX_EXCEPTION_CLASS_FAULT, 0 },
	/* DB */ { BX_ET_BENIGN,       BX_EXCEPTION_CLASS_FAULT, 0 },
	/* 02 */ { BX_ET_BENIGN,       BX_EXCEPTION_CLASS_FAULT, 0 },
	/* BP */ { BX_ET_BENIGN,       BX_EXCEPTION_CLASS_TRAP,  0 },
	/* OF */ { BX_ET_BENIGN,       BX_EXCEPTION_CLASS_TRAP,  0 },
	/* BR */ { BX_ET_BENIGN,       BX_EXCEPTION_CLASS_FAULT, 0 },
	/* UD */ { BX_ET_BENIGN,       BX_EXCEPTION_CLASS_FAULT, 0 },
	/* NM */ { BX_ET_BENIGN,       BX_EXCEPTION_CLASS_FAULT, 0 },
	/* DF */ { BX_ET_DOUBLE_FAULT, BX_EXCEPTION_CLASS_FAULT, 1 },
	/* 09 */ { BX_ET_BENIGN,       BX_EXCEPTION_CLASS_FAULT, 0 },
	/* TS */ { BX_ET_CONTRIBUTORY, BX_EXCEPTION_CLASS_FAULT, 1 },
	/* NP */ { BX_ET_CONTRIBUTORY, BX_EXCEPTION_CLASS_FAULT, 1 },
	/* SS */ { BX_ET_CONTRIBUTORY, BX_EXCEPTION_CLASS_FAULT, 1 },
	/* GP */ { BX_ET_CONTRIBUTORY, BX_EXCEPTION_CLASS_FAULT, 1 },
	/* PF */ { BX_ET_PAGE_FAULT,   BX_EXCEPTION_CLASS_FAULT, 1 },
	/* 15 */ { BX_ET_BENIGN,       BX_EXCEPTION_CLASS_FAULT, 0 },
	/* MF */ { BX_ET_BENIGN,       BX_EXCEPTION_CLASS_FAULT, 0 },
	/* AC */ { BX_ET_BENIGN,       BX_EXCEPTION_CLASS_FAULT, 1 },
	/* MC */ { BX_ET_BENIGN,       BX_EXCEPTION_CLASS_ABORT, 0 },
	/* XM */ { BX_ET_BENIGN,       BX_EXCEPTION_CLASS_FAULT, 0 },
	/* VE */ { BX_ET_PAGE_FAULT,   BX_EXCEPTION_CLASS_FAULT, 0 },
	/* 21 */ { BX_ET_BENIGN,       BX_EXCEPTION_CLASS_FAULT, 0 },
	/* 22 */ { BX_ET_BENIGN,       BX_EXCEPTION_CLASS_FAULT, 0 },
	/* 23 */ { BX_ET_BENIGN,       BX_EXCEPTION_CLASS_FAULT, 0 },
	/* 24 */ { BX_ET_BENIGN,       BX_EXCEPTION_CLASS_FAULT, 0 },
	/* 25 */ { BX_ET_BENIGN,       BX_EXCEPTION_CLASS_FAULT, 0 },
	/* 26 */ { BX_ET_BENIGN,       BX_EXCEPTION_CLASS_FAULT, 0 },
	/* 27 */ { BX_ET_BENIGN,       BX_EXCEPTION_CLASS_FAULT, 0 },
	/* 28 */ { BX_ET_BENIGN,       BX_EXCEPTION_CLASS_FAULT, 0 },
	/* 29 */ { BX_ET_BENIGN,       BX_EXCEPTION_CLASS_FAULT, 0 },
	/* 30 */ { BX_ET_BENIGN,       BX_EXCEPTION_CLASS_FAULT, 0 },
	/* 31 */ { BX_ET_BENIGN,       BX_EXCEPTION_CLASS_FAULT, 0 }
};


int audit_vmcs(BOOLEAN IsVMResume, UINT64 RevisionID, UINT64 VMXON_Pointer)
{
	/*  MSR_IA32_VMX_PINBASED_CTLS          0x481 */
	UINT32 _vmx_pin_vmexec_ctrl_supported_bits = cpu_read_msr(MSR_IA32_VMX_PINBASED_CTLS);
	/*  MSR_IA32_VMX_PROCBASED_CTLS         0x482 */
	UINT32 _vmx_proc_vmexec_ctrl_supported_bits = cpu_read_msr(MSR_IA32_VMX_PROCBASED_CTLS);
	/* MSR_IA32_VMX_PROCBASED_CTLS2        0x48B */
	UINT32 _vmx_vmexec_ctrl2_supported_bits =  cpu_read_msr(MSR_IA32_VMX_PROCBASED_CTLS2);
	/* MSR_IA32_VMX_EXIT_CTLS              0x483 */
	UINT32 _vmx_vmexit_ctrl_supported_bits = cpu_read_msr(MSR_IA32_VMX_EXIT_CTLS);
	/* MSR_IA32_VMX_ENTRY_CTLS             0x484 */
	UINT32 _vmx_vmentry_ctrl_supported_bits = cpu_read_msr(MSR_IA32_VMX_ENTRY_CTLS);
	/* MSR_IA32_VMX_EPT_VPID_CAP           0x48C */
	UINT64 _vmx_ept_vpid_cap_supported_bits = cpu_read_msr(MSR_IA32_VMX_EPT_VPID_CAP);
	/*  MSR_IA32_VMX_VMFUNC                 0x491 */
	UINT64 _vmx_vmfunc_supported_bits = cpu_read_msr(MSR_IA32_VMX_VMFUNC);
	/*  MSR_IA32_VMX_CR0_FIXED0             0x486 */
	UINT32 _cr0_suppmask_0 = cpu_read_msr(MSR_IA32_VMX_CR0_FIXED0);
	/*  MSR_IA32_VMX_CR0_FIXED1             0x487 */
	UINT32 _cr0_suppmask_1 = cpu_read_msr(MSR_IA32_VMX_CR0_FIXED1);
	/*  MSR_IA32_VMX_CR4_FIXED0             0x488 */
	UINT32 _cr4_suppmask_0 = cpu_read_msr(MSR_IA32_VMX_CR4_FIXED0);
	/*  MSR_IA32_VMX_CR4_FIXED1             0x489 */
	UINT32 _cr4_suppmask_1 = cpu_read_msr(MSR_IA32_VMX_CR4_FIXED1);

	VMCS_CACHE *vm = (VMCS_CACHE*)vmm_malloc(sizeof(VMCS_CACHE));

	if (vm == NULL) {
		vmm_printf("Failed to allocate memory for vmcs copy\n");
		return VMM_EFAIL;
	}

	/* Get host states */
	__vmread(HOST_CR0, &vm->host_state.cr0);
	__vmread(HOST_CR3, &vm->host_state.cr3);
	__vmread(HOST_CR4, &vm->host_state.cr4);
	__vmread(HOST_IA32_EFER, &vm->host_state.efer_msr);
	__vmread(HOST_FS_BASE, &vm->host_state.fs_base);
	__vmread(HOST_GDTR_BASE, &vm->host_state.gdtr_base);
	__vmread(HOST_GS_BASE, &vm->host_state.gs_base);
	__vmread(HOST_IDTR_BASE, &vm->host_state.idtr_base);
	__vmread(HOST_IA32_PAT, &vm->host_state.pat_msr);
	__vmread(HOST_ES_SELECTOR, (UINT64 *)&vm->host_state.segreg_selector[0]);
	__vmread(HOST_CS_SELECTOR, (UINT64 *)&vm->host_state.segreg_selector[1]);
	__vmread(HOST_SS_SELECTOR, (UINT64 *)&vm->host_state.segreg_selector[2]);
	__vmread(HOST_DS_SELECTOR, (UINT64 *)&vm->host_state.segreg_selector[3]);
	__vmread(HOST_FS_SELECTOR, (UINT64 *)&vm->host_state.segreg_selector[4]);
	__vmread(HOST_GS_SELECTOR, (UINT64 *)&vm->host_state.segreg_selector[5]);
	__vmread(HOST_IA32_SYSENTER_CS, (UINT64 *)&vm->host_state.sysenter_cs_msr);
	__vmread(HOST_IA32_SYSENTER_EIP, &vm->host_state.sysenter_eip_msr);
	__vmread(HOST_IA32_SYSENTER_ESP, (UINT64 *)&vm->host_state.sysenter_esp_msr);
	__vmread(HOST_TR_BASE, &vm->host_state.tr_base);

	return CheckVMXState(vm,IsVMResume, VMXON_Pointer, RevisionID,
			     _vmx_pin_vmexec_ctrl_supported_bits,
			     _vmx_proc_vmexec_ctrl_supported_bits,
			     _vmx_vmexec_ctrl2_supported_bits,
			     _vmx_vmexit_ctrl_supported_bits,
			     _vmx_vmentry_ctrl_supported_bits,
			     _vmx_ept_vpid_cap_supported_bits,
			     _vmx_vmfunc_supported_bits, _cr0_suppmask_0,
			     _cr0_suppmask_1,
			     _cr4_suppmask_0, _cr4_suppmask_1);
}

void VMexit(Bit32u reason, Bit64u qualification)
{
	vmm_printf("\n\n[*] The following configuration will cause VM-Exit "
		   "with reason (0x%x) and Exit-Qualification (%lx).\n",
		   reason, qualification);
}

void VMfail(Bit32u error_code)
{
	vmm_printf("\n\n[*] VMFail called with code (0x%x).\n", error_code);
}

bx_bool IsValidPhyAddr(bx_phy_address addr)
{
	return ((addr & BX_PHY_ADDRESS_RESERVED_BITS) == 0);
}

bx_bool CheckPDPTR(Bit64u *pdptr)
{
	for (unsigned n = 0; n < 4; n++) {
		if (pdptr[n] & 0x1) {
			if (pdptr[n] & PAGING_PAE_PDPTE_RESERVED_BITS) return 0;
		}
	}

	return 1; /* PDPTRs are fine */
}

bx_bool long_mode()
{
#if BX_SUPPORT_X86_64
	//We're definitely in long-mode when we reach here in our driver
	return 1;
#else
	return 0;
#endif
}

void init_vmx_extensions_bitmask()
{
	Bit32u features_bitmask = 0;

	features_bitmask |= BX_VMX_VIRTUAL_NMI;

#if BX_SUPPORT_X86_64
	static bx_bool x86_64_enabled = TRUE;
	if (x86_64_enabled) {
		features_bitmask |= BX_VMX_TPR_SHADOW |
			BX_VMX_APIC_VIRTUALIZATION |
			BX_VMX_WBINVD_VMEXIT;

#if BX_SUPPORT_VMX >= 2
		features_bitmask |= BX_VMX_PREEMPTION_TIMER |
			BX_VMX_PAT |
			BX_VMX_EFER |
			BX_VMX_EPT |
			BX_VMX_VPID |
			BX_VMX_UNRESTRICTED_GUEST |
			BX_VMX_DESCRIPTOR_TABLE_EXIT |
			BX_VMX_X2APIC_VIRTUALIZATION |
			BX_VMX_PAUSE_LOOP_EXITING |
			BX_VMX_EPT_ACCESS_DIRTY |
			BX_VMX_VINTR_DELIVERY |
			BX_VMX_VMCS_SHADOWING |
			BX_VMX_EPTP_SWITCHING | BX_VMX_EPT_EXCEPTION;

		features_bitmask |= BX_VMX_SAVE_DEBUGCTL_DISABLE |
			/* BX_VMX_MONITOR_TRAP_FLAG | */ // not implemented yet
			BX_VMX_PERF_GLOBAL_CTRL;
#endif
	}
#endif
	vmx_extensions_bitmask = features_bitmask;

}




void parse_selector(Bit16u raw_selector, bx_selector_t *selector)
{
	selector->value = raw_selector;
	selector->index = raw_selector >> 3;
	selector->ti = (raw_selector >> 2) & 0x01;
	selector->rpl = raw_selector & 0x03;
}

bx_bool set_segment_ar_data(bx_segment_reg_t *seg, bx_bool valid,
	Bit16u raw_selector, bx_address base, Bit32u limit_scaled, Bit16u ar_data)
{
	parse_selector(raw_selector, &seg->selector);

	bx_descriptor_t *d = &seg->cache;

	d->p = (ar_data >> 7) & 0x1;
	d->dpl = (ar_data >> 5) & 0x3;
	d->segment = (ar_data >> 4) & 0x1;
	d->type = (ar_data & 0x0f);

	d->valid = valid;

	vmm_printf("%s: AR Data: 0x%x Present: %d DPL: %d Segment: %d Type: %d Valid %d\n",
		   __func__, ar_data, d->p, d->dpl, d->segment, d->type, d->valid);

	if (d->segment || !valid) { /* data/code segment descriptors */
		d->u.segment.g = (ar_data >> 15) & 0x1;
		d->u.segment.d_b = (ar_data >> 14) & 0x1;
#if BX_SUPPORT_X86_64
		d->u.segment.l = (ar_data >> 13) & 0x1;
#endif
		d->u.segment.avl = (ar_data >> 12) & 0x1;

		d->u.segment.base = base;
		d->u.segment.limit_scaled = limit_scaled;
	}
	else {
		switch (d->type) {
		case BX_SYS_SEGMENT_LDT:
		case BX_SYS_SEGMENT_AVAIL_286_TSS:
		case BX_SYS_SEGMENT_BUSY_286_TSS:
		case BX_SYS_SEGMENT_AVAIL_386_TSS:
		case BX_SYS_SEGMENT_BUSY_386_TSS:
			d->u.segment.avl = (ar_data >> 12) & 0x1;
			d->u.segment.d_b = (ar_data >> 14) & 0x1;
			d->u.segment.g = (ar_data >> 15) & 0x1;
			d->u.segment.base = base;
			d->u.segment.limit_scaled = limit_scaled;
			break;

		default:
			vmm_printf("\n%s: set_segment_ar_data(): case %u unsupported, valid=%d", __func__, (unsigned)d->type, d->valid);
		}
	}

	return d->valid;
}
bx_bool is_eptptr_valid(Bit64u eptptr)
{
	// [2:0] EPT paging-structure memory type
	//       0 = Uncacheable (UC)
	//       6 = Write-back (WB)
	Bit32u memtype = eptptr & 7;
	if (memtype != BX_MEMTYPE_UC && memtype != BX_MEMTYPE_WB) return 0;

	// [5:3] This value is 1 less than the EPT page-walk length
	Bit32u walk_length = (eptptr >> 3) & 7;
	if (walk_length != 3) return 0;

	// [6]   EPT A/D Enable
	if (!BX_SUPPORT_VMX_EXTENSION(BX_VMX_EPT_ACCESS_DIRTY)) {
		if (eptptr & 0x40) {
			vmm_printf("\nis_eptptr_valid: EPTPTR A/D enabled when not supported by CPU");
			return 0;
		}
	}

	return 1;
}

bx_bool IsLimitAccessRightsConsistent(Bit32u limit, Bit32u ar)
{
	bx_bool g = (ar >> 15) & 1;

	// access rights reserved bits set
	if (ar & 0xfffe0f00) return 0;

	if (g) {
		// if any of the bits in limit[11:00] are '0 <=> G must be '0
		if ((limit & 0xfff) != 0xfff)
			return 0;
	}
	else {
		// if any of the bits in limit[31:20] are '1 <=> G must be '1
		if ((limit & 0xfff00000) != 0)
			return 0;
	}

	return 1;
}

#if BX_SUPPORT_X86_64
bx_bool IsCanonical(bx_address offset)
{
	return ((Bit64u)((((Bit64s)(offset)) >> (BX_LIN_ADDRESS_WIDTH - 1)) + 1) < 2);
}
#endif

BOOLEAN IsValidPageAlignedPhyAddr(bx_phy_address addr)
{
	return ((addr & (BX_PHY_ADDRESS_RESERVED_BITS | 0xfff)) == 0);
}


Bit32u rotate_r(Bit32u val_32)
{
	return (val_32 >> 8) | (val_32 << 24);
}

Bit32u vmx_from_ar_byte_rd(Bit32u ar_field)
{
	//return rotate_r(ar_byte);
	// zero out bit 16
	ar_field &= 0xfffeffff;
	// Null bit to be copied back from bit 11 to bit 16
	ar_field |= ((ar_field & 0x00000800) << 5);
	// zero out the bit 17 to bit 31
	ar_field &= 0x0001ffff;
	// bits 8 to 11 should be set to 0
	ar_field &= 0xfffff0ff;

	return ar_field;
}

bx_bool isMemTypeValidMTRR(unsigned memtype)
{
	switch (memtype) {
	case BX_MEMTYPE_UC:
	case BX_MEMTYPE_WC:
	case BX_MEMTYPE_WT:
	case BX_MEMTYPE_WP:
	case BX_MEMTYPE_WB:
		return BX_TRUE;
	default:
		return BX_FALSE;
	}
}

bx_bool isMemTypeValidPAT(unsigned memtype)
{
	return (memtype == 0x07) /* UC- */ || isMemTypeValidMTRR(memtype);
}

#if 0 //Himanshu
bx_bool isValidMSR_PAT(Bit64u pat_val)
{
	// use packed register as 64-bit value with convinient accessors
	BxPackedRegister pat_msr = pat_val;
	for (unsigned i = 0; i < 8; i++)
		if (!isMemTypeValidPAT(pat_msr.ubyte(i))) return BX_FALSE;

	return BX_TRUE;
}
#endif

VMX_error_code VMenterLoadCheckVmControls(VMCS_CACHE *vm)
{
	int error;

	vm->vmexec_ctrls1 = __vmread_safe(VMCS_32BIT_CONTROL_PIN_BASED_EXEC_CONTROLS, &error);
	vm->vmexec_ctrls2 = __vmread_safe(VMCS_32BIT_CONTROL_PROCESSOR_BASED_VMEXEC_CONTROLS,
					  &error);

	if (vm->vmexec_ctrls2 & (VMX_VM_EXEC_CTRL2_SECONDARY_CONTROLS)) {
		vm->vmexec_ctrls3 = __vmread_safe(VMCS_32BIT_CONTROL_SECONDARY_VMEXEC_CONTROLS,
						  &error);

	} else
		vm->vmexec_ctrls3 = 0;

	vm->vm_exceptions_bitmap = __vmread_safe(VMCS_32BIT_CONTROL_EXECUTION_BITMAP,
						 &error);
	vm->vm_pf_mask = __vmread_safe(VMCS_32BIT_CONTROL_PAGE_FAULT_ERR_CODE_MASK,
				       &error);
	vm->vm_pf_match = __vmread_safe(VMCS_32BIT_CONTROL_PAGE_FAULT_ERR_CODE_MATCH,
					&error);
	vm->vm_cr0_mask = __vmread_safe(VMCS_CONTROL_CR0_GUEST_HOST_MASK, &error);
	vm->vm_cr4_mask = __vmread_safe(VMCS_CONTROL_CR4_GUEST_HOST_MASK, &error);
	vm->vm_cr0_read_shadow = __vmread_safe(VMCS_CONTROL_CR0_READ_SHADOW, &error);
	vm->vm_cr4_read_shadow = __vmread_safe(VMCS_CONTROL_CR4_READ_SHADOW, &error);
	vm->vm_cr3_target_cnt = __vmread_safe(VMCS_32BIT_CONTROL_CR3_TARGET_COUNT,
					      &error);


	for (int n = 0; n < VMX_CR3_TARGET_MAX_CNT; n++) {
		if (n == 0)
			vm->vm_cr3_target_value[n] = __vmread_safe(VMCS_CR3_TARGET0 +2 * n,
								   &error);

		if (n == 1)
			vm->vm_cr3_target_value[n] = __vmread_safe(VMCS_CR3_TARGET1 +2 * n,
								   &error);
		if (n == 2)
			vm->vm_cr3_target_value[n] = __vmread_safe(VMCS_CR3_TARGET2 + 2 * n,
								   &error);
		if (n == 3)
			vm->vm_cr3_target_value[n] = __vmread_safe(VMCS_CR3_TARGET3,
								   &error);
	}

	/*
	 * Check VM-execution control fields
	 */
	extern u32 vmx_pin_based_exec_default1;
	extern u32 vmx_pin_based_exec_default0;
	extern u32 vmx_cpu_based_exec_default0;
	extern u32 vmx_cpu_based_exec_default1;
	extern u32 vmx_secondary_exec_default0;
	extern u32 vmx_secondary_exec_default1;

	if (~vm->vmexec_ctrls1 & vmx_pin_based_exec_default0) {
		vmm_printf("\nVMFAIL: VMCS EXEC CTRL: VMX pin-based controls allowed 0-settings");
		return VMXERR_VMENTRY_INVALID_VM_CONTROL_FIELD;
	}
	if (vm->vmexec_ctrls1 & ~vmx_pin_based_exec_default1) {
		vmm_printf("\nVMFAIL: VMCS EXEC CTRL: VMX pin-based controls allowed 1-settings(CTRL: 0x%08x DEF: 0x%08x)",
			   vm->vmexec_ctrls1, vmx_pin_based_exec_default1);
		return VMXERR_VMENTRY_INVALID_VM_CONTROL_FIELD;
	}

	vmm_printf("Pinbased controls check PASSED.\n");

	if (~vm->vmexec_ctrls2 & vmx_cpu_based_exec_default0) {
		vmm_printf("\nVMFAIL: VMCS EXEC CTRL: VMX proc-based controls allowed 0-settings");
		return VMXERR_VMENTRY_INVALID_VM_CONTROL_FIELD;
	}
	if (vm->vmexec_ctrls2 & ~vmx_cpu_based_exec_default1) {
		vmm_printf("\nVMFAIL: VMCS EXEC CTRL: VMX proc-based controls allowed 1-settings");
		return VMXERR_VMENTRY_INVALID_VM_CONTROL_FIELD;
	}

	if (~vm->vmexec_ctrls3 & vmx_secondary_exec_default0) {
		vmm_printf("\nVMFAIL: VMCS EXEC CTRL: VMX secondary proc-based controls allowed 0-settings");
		return VMXERR_VMENTRY_INVALID_VM_CONTROL_FIELD;
	}
	if (vm->vmexec_ctrls3 & ~vmx_secondary_exec_default1) {
		vmm_printf("\nVMFAIL: VMCS EXEC CTRL: VMX secondary proc-based controls allowed 1-settings");
		return VMXERR_VMENTRY_INVALID_VM_CONTROL_FIELD;
	}

	if (vm->vm_cr3_target_cnt > VMX_CR3_TARGET_MAX_CNT) {
		vmm_printf("\nVMFAIL: VMCS EXEC CTRL: too may CR3 targets %d", vm->vm_cr3_target_cnt);
		return VMXERR_VMENTRY_INVALID_VM_CONTROL_FIELD;
	}

	if (vm->vmexec_ctrls2 & VMX_VM_EXEC_CTRL2_IO_BITMAPS) {
		vm->io_bitmap_addr[0] = __vmread_safe(VMCS_64BIT_CONTROL_IO_BITMAP_A,
						      &error);
		vm->io_bitmap_addr[1] = __vmread_safe(VMCS_64BIT_CONTROL_IO_BITMAP_B,
						      &error);

		/* I/O bitmaps control enabled */
		for (int bitmap = 0; bitmap < 2; bitmap++) {
			if (!IsValidPageAlignedPhyAddr(vm->io_bitmap_addr[bitmap])) {
				vmm_printf("\nVMFAIL: VMCS EXEC CTRL: I/O bitmap %c phy addr malformed", 'A' + bitmap);
				return VMXERR_VMENTRY_INVALID_VM_CONTROL_FIELD;
			}
		}
	}

	if (vm->vmexec_ctrls2 & VMX_VM_EXEC_CTRL2_MSR_BITMAPS) {
		// MSR bitmaps control enabled
		vm->msr_bitmap_addr = __vmread_safe(VMCS_64BIT_CONTROL_MSR_BITMAPS,
						    &error);
		if (!IsValidPageAlignedPhyAddr(vm->msr_bitmap_addr)) {
			vmm_printf("\nVMFAIL: VMCS EXEC CTRL: MSR bitmap phy addr malformed");
			return VMXERR_VMENTRY_INVALID_VM_CONTROL_FIELD;
		}
	}

	if (!(vm->vmexec_ctrls1 & VMX_VM_EXEC_CTRL1_NMI_EXITING)) {
		if (vm->vmexec_ctrls1 & VMX_VM_EXEC_CTRL1_VIRTUAL_NMI) {
			vmm_printf("\nVMFAIL: VMCS EXEC CTRL: misconfigured virtual NMI control");
			return VMXERR_VMENTRY_INVALID_VM_CONTROL_FIELD;
		}
	}

	if (!(vm->vmexec_ctrls1 & VMX_VM_EXEC_CTRL1_VIRTUAL_NMI)) {
		if (vm->vmexec_ctrls2 & VMX_VM_EXEC_CTRL2_NMI_WINDOW_EXITING) {
			vmm_printf("\nVMFAIL: VMCS EXEC CTRL: misconfigured virtual NMI control");
			return VMXERR_VMENTRY_INVALID_VM_CONTROL_FIELD;
		}
	}

#if BX_SUPPORT_VMX >= 2
	if (vm->vmexec_ctrls3 & VMX_VM_EXEC_CTRL3_VMCS_SHADOWING) {
		//# vm->vmread_bitmap_addr = (bx_phy_address)VMread64(VMCS_64BIT_CONTROL_VMREAD_BITMAP_ADDR);
		vm->vmread_bitmap_addr = (bx_phy_address)__vmread_safe(VMCS_64BIT_CONTROL_VMREAD_BITMAP_ADDR, &error);

		if (!IsValidPageAlignedPhyAddr(vm->vmread_bitmap_addr)) {
			vmm_printf("\nVMFAIL: VMCS EXEC CTRL: VMREAD bitmap phy addr malformed");
			return VMXERR_VMENTRY_INVALID_VM_CONTROL_FIELD;
		}
		//# vm->vmwrite_bitmap_addr = (bx_phy_address)VMread64(VMCS_64BIT_CONTROL_VMWRITE_BITMAP_ADDR);
		vm->vmwrite_bitmap_addr = (bx_phy_address)__vmread_safe(VMCS_64BIT_CONTROL_VMWRITE_BITMAP_ADDR, &error);

		if (!IsValidPageAlignedPhyAddr(vm->vmwrite_bitmap_addr)) {
			vmm_printf("\nVMFAIL: VMCS EXEC CTRL: VMWRITE bitmap phy addr malformed");
			return VMXERR_VMENTRY_INVALID_VM_CONTROL_FIELD;
		}
	}

	if (vm->vmexec_ctrls3 & VMX_VM_EXEC_CTRL3_EPT_VIOLATION_EXCEPTION) {
		//# vm->ve_info_addr = (bx_phy_address)VMread64(VMCS_64BIT_CONTROL_VE_EXCEPTION_INFO_ADDR);
		vm->ve_info_addr = (bx_phy_address)__vmread_safe(VMCS_64BIT_CONTROL_VE_EXCEPTION_INFO_ADDR, &error);

		if (!IsValidPageAlignedPhyAddr(vm->ve_info_addr)) {
			vmm_printf("\nVMFAIL: VMCS EXEC CTRL: broken #VE information address");
			return VMXERR_VMENTRY_INVALID_VM_CONTROL_FIELD;
		}
	}
#endif

#if BX_SUPPORT_X86_64
	if (vm->vmexec_ctrls2 & VMX_VM_EXEC_CTRL2_TPR_SHADOW) {
		//# vm->virtual_apic_page_addr = (bx_phy_address)VMread64(VMCS_64BIT_CONTROL_VIRTUAL_APIC_PAGE_ADDR);

		vm->virtual_apic_page_addr = (bx_phy_address)__vmread_safe(VMCS_64BIT_CONTROL_VIRTUAL_APIC_PAGE_ADDR, &error);

		if (!IsValidPageAlignedPhyAddr(vm->virtual_apic_page_addr)) {
			vmm_printf("\nVMFAIL: VMCS EXEC CTRL: virtual apic phy addr malformed");
			return VMXERR_VMENTRY_INVALID_VM_CONTROL_FIELD;
		}

#if BX_SUPPORT_VMX >= 2
		if (vm->vmexec_ctrls3 & VMX_VM_EXEC_CTRL3_VIRTUAL_INT_DELIVERY) {
			if (!(vm->vmexec_ctrls1 & VMX_VM_EXEC_CTRL1_EXTERNAL_INTERRUPT_VMEXIT)) {

				vmm_printf("\nVMFAIL: VMCS EXEC CTRL: virtual interrupt delivery must be set together with external interrupt exiting");
				return VMXERR_VMENTRY_INVALID_VM_CONTROL_FIELD;
			}

			for (int reg = 0; reg < 8; reg++) {
				if (reg == 0)
					vm->eoi_exit_bitmap[reg] = __vmread_safe(VMCS_64BIT_CONTROL_EOI_EXIT_BITMAP0, &error);
				if (reg == 1)
					vm->eoi_exit_bitmap[reg] = __vmread_safe(VMCS_64BIT_CONTROL_EOI_EXIT_BITMAP0_HI, &error);
				if (reg == 2)
					vm->eoi_exit_bitmap[reg] = __vmread_safe(VMCS_64BIT_CONTROL_EOI_EXIT_BITMAP1, &error);
				if (reg == 3)
					vm->eoi_exit_bitmap[reg] = __vmread_safe(VMCS_64BIT_CONTROL_EOI_EXIT_BITMAP1_HI, &error);
				if (reg == 4)
					vm->eoi_exit_bitmap[reg] = __vmread_safe(VMCS_64BIT_CONTROL_EOI_EXIT_BITMAP2, &error);
				if (reg == 5)
					vm->eoi_exit_bitmap[reg] = __vmread_safe(VMCS_64BIT_CONTROL_EOI_EXIT_BITMAP2_HI, &error);
				if (reg == 6)
					vm->eoi_exit_bitmap[reg] = __vmread_safe(VMCS_64BIT_CONTROL_EOI_EXIT_BITMAP3, &error);
				if (reg == 7)
					vm->eoi_exit_bitmap[reg] = __vmread_safe(VMCS_64BIT_CONTROL_EOI_EXIT_BITMAP3_HI, 0x0);
			}

			Bit16u guest_interrupt_status = __vmread_safe(VMCS_16BIT_GUEST_INTERRUPT_STATUS, &error);

			vm->rvi = guest_interrupt_status & 0xff;
			vm->svi = guest_interrupt_status >> 8;
		}
		else
#endif
		{
			vm->vm_tpr_threshold = __vmread_safe(VMCS_32BIT_CONTROL_TPR_THRESHOLD, &error);

			if (vm->vm_tpr_threshold & 0xfffffff0) {
				vmm_printf("\nVMFAIL: VMCS EXEC CTRL: TPR threshold too big");
				return VMXERR_VMENTRY_INVALID_VM_CONTROL_FIELD;
			}

			if (!(vm->vmexec_ctrls3 & VMX_VM_EXEC_CTRL3_VIRTUALIZE_APIC_ACCESSES)) {
				/* TODO: Add check for APIC address validation */
				vmm_printf("\nAPIC address validation skipped. Not supported.\n");
			}
		}
	}
#if BX_SUPPORT_VMX >= 2
	else { /* TPR shadow is disabled */
		if (vm->vmexec_ctrls3 & (VMX_VM_EXEC_CTRL3_VIRTUALIZE_X2APIC_MODE |
			VMX_VM_EXEC_CTRL3_VIRTUALIZE_APIC_REGISTERS |
			VMX_VM_EXEC_CTRL3_VIRTUAL_INT_DELIVERY))
		{
			vmm_printf("\nVMFAIL: VMCS EXEC CTRL: apic virtualization is enabled without TPR shadow");
			return VMXERR_VMENTRY_INVALID_VM_CONTROL_FIELD;
		}
	}
#endif // BX_SUPPORT_VMX >= 2

	if (vm->vmexec_ctrls3 & VMX_VM_EXEC_CTRL3_VIRTUALIZE_APIC_ACCESSES) {
		vm->apic_access_page = (bx_phy_address)__vmread_safe(VMCS_64BIT_CONTROL_APIC_ACCESS_ADDR, &error);
		if (!IsValidPageAlignedPhyAddr(vm->apic_access_page)) {
			vmm_printf("\nVMFAIL: VMCS EXEC CTRL: apic access page phy addr malformed");
			return VMXERR_VMENTRY_INVALID_VM_CONTROL_FIELD;
		}

#if BX_SUPPORT_VMX >= 2
		if (vm->vmexec_ctrls3 & VMX_VM_EXEC_CTRL3_VIRTUALIZE_X2APIC_MODE) {
			vmm_printf("\nVMFAIL: VMCS EXEC CTRL: virtualize X2APIC mode enabled together with APIC access virtualization");
			return VMXERR_VMENTRY_INVALID_VM_CONTROL_FIELD;
		}
#endif
	}

#if BX_SUPPORT_VMX >= 2
	if (vm->vmexec_ctrls3 & VMX_VM_EXEC_CTRL3_EPT_ENABLE) {
		vm->eptptr = (bx_phy_address)__vmread_safe(VMCS_64BIT_CONTROL_EPTPTR , &error);

		if (!is_eptptr_valid(vm->eptptr)) {
			vmm_printf("\nVMFAIL: VMCS EXEC CTRL: invalid EPTPTR value");
			return VMXERR_VMENTRY_INVALID_VM_CONTROL_FIELD;
		}
	}
	else {
		if (vm->vmexec_ctrls3 & VMX_VM_EXEC_CTRL3_UNRESTRICTED_GUEST) {
			vmm_printf("\nVMFAIL: VMCS EXEC CTRL: unrestricted guest without EPT");
			return VMXERR_VMENTRY_INVALID_VM_CONTROL_FIELD;
		}
	}

	if (vm->vmexec_ctrls3 & VMX_VM_EXEC_CTRL3_VPID_ENABLE) {
		vm->vpid = __vmread_safe(VMCS_16BIT_CONTROL_VPID, &error);

		if (vm->vpid == 0) {
			vmm_printf("\nVMFAIL: VMCS EXEC CTRL: guest VPID == 0");
			return VMXERR_VMENTRY_INVALID_VM_CONTROL_FIELD;
		}
	}

	if (vm->vmexec_ctrls3 & VMX_VM_EXEC_CTRL3_PAUSE_LOOP_VMEXIT) {
		vm->ple.pause_loop_exiting_gap = __vmread_safe(VMCS_32BIT_CONTROL_PAUSE_LOOP_EXITING_GAP, &error);
		vm->ple.pause_loop_exiting_window = __vmread_safe(VMCS_32BIT_CONTROL_PAUSE_LOOP_EXITING_WINDOW, &error);
	}

	if (vm->vmexec_ctrls3 & VMX_VM_EXEC_CTRL3_VMFUNC_ENABLE)
		vm->vmfunc_ctrls = __vmread_safe(VMCS_64BIT_CONTROL_VMFUNC_CTRLS, &error);
	else
		vm->vmfunc_ctrls = 0;

	if (vm->vmfunc_ctrls & ~VMX_VMFUNC_CTRL1_SUPPORTED_BITS) {
		vmm_printf("\nVMFAIL: VMCS VM Functions control reserved bits set");
		return VMXERR_VMENTRY_INVALID_VM_CONTROL_FIELD;
	}

	if (vm->vmfunc_ctrls & VMX_VMFUNC_EPTP_SWITCHING_MASK) {
		if ((vm->vmexec_ctrls3 & VMX_VM_EXEC_CTRL3_EPT_ENABLE) == 0) {
			vmm_printf("\nVMFAIL: VMFUNC EPTP-SWITCHING: EPT disabled");
			return VMXERR_VMENTRY_INVALID_VM_CONTROL_FIELD;
		}

		vm->eptp_list_address = __vmread_safe(VMCS_64BIT_CONTROL_EPTP_LIST_ADDRESS, &error);
		if (!IsValidPageAlignedPhyAddr(vm->eptp_list_address)) {
			vmm_printf("\nVMFAIL: VMFUNC EPTP-SWITCHING: eptp list phy addr malformed");
			return VMXERR_VMENTRY_INVALID_VM_CONTROL_FIELD;
		}
	}

	if (vm->vmexec_ctrls3 & VMX_VM_EXEC_CTRL3_PML_ENABLE) {
		if ((vm->vmexec_ctrls3 & VMX_VM_EXEC_CTRL3_EPT_ENABLE) == 0) {
			vmm_printf("\nVMFAIL: VMCS EXEC CTRL: PML is enabled without EPT");
			return VMXERR_VMENTRY_INVALID_VM_CONTROL_FIELD;
		}

		vm->pml_address = (bx_phy_address)__vmread_safe(VMCS_64BIT_CONTROL_PML_ADDRESS, &error);
		if (!IsValidPageAlignedPhyAddr(vm->pml_address)) {
			vmm_printf("\nVMFAIL: VMCS EXEC CTRL: PML base phy addr malformed");
			return VMXERR_VMENTRY_INVALID_VM_CONTROL_FIELD;
		}
		vm->pml_index = __vmread_safe(VMCS_16BIT_GUEST_PML_INDEX, &error);
	}
#endif

	if (vm->vmexec_ctrls3 & VMX_VM_EXEC_CTRL3_XSAVES_XRSTORS)
		vm->xss_exiting_bitmap = __vmread_safe(VMCS_64BIT_CONTROL_XSS_EXITING_BITMAP, &error);
	else
		vm->xss_exiting_bitmap = 0;

#endif // BX_SUPPORT_X86_64

	if (vm->vmexec_ctrls3 & VMX_VM_EXEC_CTRL3_TSC_SCALING) {
		if ((vm->tsc_multiplier = __vmread_safe(VMCS_64BIT_CONTROL_TSC_MULTIPLIER, &error)) == 0) {
			vmm_printf("\nVMFAIL: VMCS EXEC CTRL: TSC multiplier should be non zero");
			return VMXERR_VMENTRY_INVALID_VM_CONTROL_FIELD;
		}
	}

	/*
	 * Load VM-exit control fields to VMCS Cache
	 */
	vm->vmexit_ctrls = __vmread_safe(VMCS_32BIT_CONTROL_VMEXIT_CONTROLS, &error);
	vm->vmexit_msr_store_cnt = __vmread_safe(VMCS_32BIT_CONTROL_VMEXIT_MSR_STORE_COUNT, &error);
	vm->vmexit_msr_load_cnt = __vmread_safe(VMCS_32BIT_CONTROL_VMEXIT_MSR_LOAD_COUNT, &error);


	/*
	 * Check VM-exit control fields
	 */
	extern u32 vmx_vmexit_default1;
	extern u32 vmx_vmexit_default0;

	if (~vm->vmexit_ctrls & vmx_vmexit_default0) {
		vmm_printf("\nVMFAIL: VMCS EXEC CTRL: VMX vmexit controls allowed 0-settings");
		return VMXERR_VMENTRY_INVALID_VM_CONTROL_FIELD;
	}
	if (vm->vmexit_ctrls & ~vmx_vmexit_default1) {
		vmm_printf("\nVMFAIL: VMCS EXEC CTRL: VMX vmexit controls allowed 1-settings");
		return VMXERR_VMENTRY_INVALID_VM_CONTROL_FIELD;
	}

#if BX_SUPPORT_VMX >= 2
	if ((~vm->vmexec_ctrls1 & VMX_VM_EXEC_CTRL1_VMX_PREEMPTION_TIMER_VMEXIT) && (vm->vmexit_ctrls & VMX_VMEXIT_CTRL1_STORE_VMX_PREEMPTION_TIMER)) {
		vmm_printf("\nVMFAIL: save_VMX_preemption_timer VMEXIT control is set but VMX_preemption_timer VMEXEC control is clear");
		return VMXERR_VMENTRY_INVALID_VM_CONTROL_FIELD;
	}
#endif

	if (vm->vmexit_msr_store_cnt > 0) {
		vm->vmexit_msr_store_addr = __vmread_safe(VMCS_64BIT_CONTROL_VMEXIT_MSR_STORE_ADDR, &error);
		if ((vm->vmexit_msr_store_addr & 0xf) != 0 || !IsValidPhyAddr(vm->vmexit_msr_store_addr)) {
			vmm_printf("\nVMFAIL: VMCS VMEXIT CTRL: msr store addr malformed");
			return VMXERR_VMENTRY_INVALID_VM_CONTROL_FIELD;
		}

		Bit64u last_byte = vm->vmexit_msr_store_addr + (vm->vmexit_msr_store_cnt * 16) - 1;
		if (!IsValidPhyAddr(last_byte)) {
			vmm_printf("\nVMFAIL: VMCS VMEXIT CTRL: msr store addr too high");
			return VMXERR_VMENTRY_INVALID_VM_CONTROL_FIELD;
		}
	}

	if (vm->vmexit_msr_load_cnt > 0) {
		vm->vmexit_msr_load_addr = __vmread_safe(VMCS_64BIT_CONTROL_VMEXIT_MSR_LOAD_ADDR, &error);
		if ((vm->vmexit_msr_load_addr & 0xf) != 0 || !IsValidPhyAddr(vm->vmexit_msr_load_addr)) {
			vmm_printf("\nVMFAIL: VMCS VMEXIT CTRL: msr load addr malformed");
			return VMXERR_VMENTRY_INVALID_VM_CONTROL_FIELD;
		}

		Bit64u last_byte = (Bit64u)vm->vmexit_msr_load_addr + (vm->vmexit_msr_load_cnt * 16) - 1;
		if (!IsValidPhyAddr(last_byte)) {
			vmm_printf("\nVMFAIL: VMCS VMEXIT CTRL: msr load addr too high");
			return VMXERR_VMENTRY_INVALID_VM_CONTROL_FIELD;
		}
	}

	/*
	 * Load VM-entry control fields to VMCS Cache
	 */

	vm->vmentry_ctrls = __vmread_safe(VMCS_32BIT_CONTROL_VMENTRY_CONTROLS, &error);
	vm->vmentry_msr_load_cnt = __vmread_safe(VMCS_32BIT_CONTROL_VMENTRY_MSR_LOAD_COUNT, &error);
	vmm_printf("\nVMEntry Controls: 0x%08x Load Count: %d\n", vm->vmentry_ctrls, vm->vmentry_msr_load_cnt);

	/*
	 * Check VM-entry control fields
	 */
	extern u32 vmx_vmentry_default1;
	extern u32 vmx_vmentry_default0;

	if (~vm->vmentry_ctrls & vmx_vmentry_default0) {
		vmm_printf("\nVMFAIL: VMCS EXEC CTRL: VMX vmentry controls allowed 0-settings");
		return VMXERR_VMENTRY_INVALID_VM_CONTROL_FIELD;
	}
	if (vm->vmentry_ctrls & ~vmx_vmentry_default1) {
		vmm_printf("\nVMFAIL: VMCS EXEC CTRL: VMX vmentry controls allowed 1-settings");
		return VMXERR_VMENTRY_INVALID_VM_CONTROL_FIELD;
	}
	vmm_printf("vmx entry control default settings check passed\n");

	if (vm->vmentry_ctrls & VMX_VMENTRY_CTRL1_DEACTIVATE_DUAL_MONITOR_TREATMENT) {
		/* TODO: Add support for SMM checks */
		vmm_printf("Check for guest with SMM entry not supported.\n");
	}

	if (vm->vmentry_msr_load_cnt > 0) {
		vm->vmentry_msr_load_addr = __vmread_safe(VMCS_64BIT_CONTROL_VMENTRY_MSR_LOAD_ADDR, &error);

		if ((vm->vmentry_msr_load_addr & 0xf) != 0 || !IsValidPhyAddr(vm->vmentry_msr_load_addr)) {
			vmm_printf("\nVMFAIL: VMCS VMENTRY CTRL: msr load addr malformed");
			return VMXERR_VMENTRY_INVALID_VM_CONTROL_FIELD;
		}

		Bit64u last_byte = vm->vmentry_msr_load_addr + (vm->vmentry_msr_load_cnt * 16) - 1;
		if (!IsValidPhyAddr(last_byte)) {
			vmm_printf("\nVMFAIL: VMCS VMENTRY CTRL: msr load addr too high");
			return VMXERR_VMENTRY_INVALID_VM_CONTROL_FIELD;
		}
	}

	/*
	 * Check VM-entry event injection info
	 */
	vm->vmentry_interr_info = __vmread_safe(VMCS_32BIT_CONTROL_VMENTRY_INTERRUPTION_INFO, &error);
	vm->vmentry_excep_err_code = __vmread_safe(VMCS_32BIT_CONTROL_VMENTRY_EXCEPTION_ERR_CODE, &error);
	vm->vmentry_instr_length = __vmread_safe(VMCS_32BIT_CONTROL_VMENTRY_INSTRUCTION_LENGTH, &error);

	if (VMENTRY_INJECTING_EVENT(vm->vmentry_interr_info)) {
		/* the VMENTRY injecting event to the guest */
		unsigned vector = vm->vmentry_interr_info & 0xff;
		unsigned event_type = (vm->vmentry_interr_info >> 8) & 7;
		unsigned push_error = (vm->vmentry_interr_info >> 11) & 1;
		unsigned error_code = push_error ? vm->vmentry_excep_err_code : 0;

		unsigned push_error_reference = 0;
		if (event_type == BX_HARDWARE_EXCEPTION && vector < BX_CPU_HANDLED_EXCEPTIONS)
			push_error_reference = exceptions_info[vector].push_error;

		if (vm->vmentry_interr_info & 0x7ffff000) {
			vmm_printf("\nVMFAIL: VMENTRY broken interruption info field");
			return VMXERR_VMENTRY_INVALID_VM_CONTROL_FIELD;
		}

		switch (event_type) {
		case BX_EXTERNAL_INTERRUPT:
			break;

		case BX_NMI:
			if (vector != 2) {
				vmm_printf("\nVMFAIL: VMENTRY bad injected event vector %d", vector);
				return VMXERR_VMENTRY_INVALID_VM_CONTROL_FIELD;
			}
			/*
					 // injecting NMI
					 if (vm->vmexec_ctrls1 & VMX_VM_EXEC_CTRL1_VIRTUAL_NMI) {
					   if (guest.interruptibility_state & BX_VMX_INTERRUPTS_BLOCKED_NMI_BLOCKED) {
						 printf(("\nVMFAIL: VMENTRY injected NMI vector when blocked by NMI in interruptibility state", vector));
						 return VMXERR_VMENTRY_INVALID_VM_CONTROL_FIELD;
					   }
					 }
			*/
			break;

		case BX_HARDWARE_EXCEPTION:
			if (vector > 31) {
				vmm_printf("\nVMFAIL: VMENTRY bad injected event vector %d", vector);
				return VMXERR_VMENTRY_INVALID_VM_CONTROL_FIELD;
			}
			break;

		case BX_SOFTWARE_INTERRUPT:
		case BX_PRIVILEGED_SOFTWARE_INTERRUPT:
		case BX_SOFTWARE_EXCEPTION:
			if (vm->vmentry_instr_length == 0 || vm->vmentry_instr_length > 15) {
				vmm_printf("\nVMFAIL: VMENTRY bad injected event instr length");
				return VMXERR_VMENTRY_INVALID_VM_CONTROL_FIELD;
			}
			break;

		case 7: /* MTF */
			if (BX_SUPPORT_VMX_EXTENSION(BX_VMX_MONITOR_TRAP_FLAG)) {
				if (vector != 0) {
					vmm_printf("\nVMFAIL: VMENTRY bad MTF injection with vector=%d", vector);
					return VMXERR_VMENTRY_INVALID_VM_CONTROL_FIELD;
				}
			}
			break;

		default:
			vmm_printf("\nVMFAIL: VMENTRY bad injected event type %d", event_type);
			return VMXERR_VMENTRY_INVALID_VM_CONTROL_FIELD;
		}

#if BX_SUPPORT_VMX >= 2
		if (vm->vmexec_ctrls3 & VMX_VM_EXEC_CTRL3_UNRESTRICTED_GUEST) {
			unsigned protected_mode_guest = (Bit32u)__vmread_safe(VMCS_GUEST_CR0, &error)  & BX_CR0_PE_MASK;
			if (!protected_mode_guest) push_error_reference = 0;
		}
#endif

		if (push_error != push_error_reference) {
			vmm_printf("\nVMFAIL: VMENTRY injected event vector %d broken error code", vector);
			return VMXERR_VMENTRY_INVALID_VM_CONTROL_FIELD;
		}

		if (push_error) {
			if (error_code & 0xffff0000) {
				vmm_printf("\nVMFAIL: VMENTRY bad error code 0x%08x for injected event %d", error_code, vector);
				return VMXERR_VMENTRY_INVALID_VM_CONTROL_FIELD;
			}
		}
	}

	return VMXERR_NO_ERROR;
}

VMX_error_code VMenterLoadCheckHostState(VMCS_CACHE *vm)
{
	int error;
	VMCS_HOST_STATE *host_state = &vm->host_state;
	bx_bool x86_64_host = 0, x86_64_guest = 0;

	Bit32u vmexit_ctrls = vm->vmexit_ctrls;
	if (vmexit_ctrls & VMX_VMEXIT_CTRL1_HOST_ADDR_SPACE_SIZE) {
		x86_64_host = 1;
	}
	Bit32u vmentry_ctrls = vm->vmentry_ctrls;
	if (vmentry_ctrls & VMX_VMENTRY_CTRL1_X86_64_GUEST) {
		x86_64_guest = 1;
	}

#if BX_SUPPORT_X86_64
	if (long_mode()) {
		if (!x86_64_host) {
			vmm_printf("\nVMFAIL: VMCS x86-64 host control invalid on VMENTRY");
			return VMXERR_VMENTRY_INVALID_VM_HOST_STATE_FIELD;
		}
	}
	else
#endif
	{
		if (x86_64_host || x86_64_guest) {
			vmm_printf("\nVMFAIL: VMCS x86-64 guest(%d)/host(%d) controls invalid on VMENTRY", x86_64_guest, x86_64_host);
			return VMXERR_VMENTRY_INVALID_VM_HOST_STATE_FIELD;
		}
	}

	if (~host_state->cr0 & VMX_MSR_CR0_FIXED0) {
		vmm_printf("\nVMFAIL: VMCS host state invalid CR0 0x%08x", (Bit32u)host_state->cr0);
		return VMXERR_VMENTRY_INVALID_VM_HOST_STATE_FIELD;
	}

	if (host_state->cr0 & ~VMX_MSR_CR0_FIXED1) {
		vmm_printf("\nVMFAIL: VMCS host state invalid CR0 0x%08x", (Bit32u)host_state->cr0);
		return VMXERR_VMENTRY_INVALID_VM_HOST_STATE_FIELD;
	}

#if BX_SUPPORT_X86_64
	if (!IsValidPhyAddr(host_state->cr3)) {
		vmm_printf("\nVMFAIL: VMCS host state invalid CR3");
		return VMXERR_VMENTRY_INVALID_VM_HOST_STATE_FIELD;
	}
#endif

	if (~host_state->cr4 & VMX_MSR_CR4_FIXED0) {
		vmm_printf("\nVMFAIL: VMCS host state invalid CR4 0x%16lx", host_state->cr4);
		return VMXERR_VMENTRY_INVALID_VM_HOST_STATE_FIELD;
	}
	if (host_state->cr4 & ~VMX_MSR_CR4_FIXED1) {
		vmm_printf("\nVMFAIL: VMCS host state invalid CR4 0x%16lx", host_state->cr4);
		return VMXERR_VMENTRY_INVALID_VM_HOST_STATE_FIELD;
	}

	for (int n = 0; n < 6; n++) {
		if (n == 0)
			host_state->segreg_selector[n] = __vmread_safe(VMCS_16BIT_HOST_ES_SELECTOR, &error);
		if (n == 1)
			host_state->segreg_selector[n] = __vmread_safe(VMCS_16BIT_HOST_CS_SELECTOR, &error);
		if (n == 2)
			host_state->segreg_selector[n] = __vmread_safe(VMCS_16BIT_HOST_SS_SELECTOR, &error);
		if (n == 3)
			host_state->segreg_selector[n] = __vmread_safe(VMCS_16BIT_HOST_DS_SELECTOR, &error);
		if (n == 4)
			host_state->segreg_selector[n] = __vmread_safe(VMCS_16BIT_HOST_FS_SELECTOR, &error);
		if (n == 5)
			host_state->segreg_selector[n] = __vmread_safe(VMCS_16BIT_HOST_GS_SELECTOR, &error);

		if (host_state->segreg_selector[n] & 7) {
			vmm_printf("\nVMFAIL: VMCS host segreg %d TI/RPL != 0", n);
			return VMXERR_VMENTRY_INVALID_VM_HOST_STATE_FIELD;
		}
	}

	if (host_state->segreg_selector[BX_SEG_REG_CS] == 0) {
		vmm_printf("\nVMFAIL: VMCS host CS selector 0");
		return VMXERR_VMENTRY_INVALID_VM_HOST_STATE_FIELD;
	}

	if (!x86_64_host && host_state->segreg_selector[BX_SEG_REG_SS] == 0) {
		vmm_printf("\nVMFAIL: VMCS host SS selector 0");
		return VMXERR_VMENTRY_INVALID_VM_HOST_STATE_FIELD;
	}

	if (!host_state->tr_selector || (host_state->tr_selector & 7) != 0) {
		vmm_printf("\nVMFAIL: VMCS invalid host TR selector");
		return VMXERR_VMENTRY_INVALID_VM_HOST_STATE_FIELD;
	}

#if BX_SUPPORT_X86_64
	if (!IsCanonical(host_state->tr_base)) {
		vmm_printf("\nVMFAIL: VMCS host TR BASE non canonical");
		return VMXERR_VMENTRY_INVALID_VM_HOST_STATE_FIELD;
	}
#endif

#if BX_SUPPORT_X86_64
	if (!IsCanonical(host_state->fs_base)) {
		vmm_printf("\nVMFAIL: VMCS host FS BASE non canonical");
		return VMXERR_VMENTRY_INVALID_VM_HOST_STATE_FIELD;
	}
	if (!IsCanonical(host_state->gs_base)) {
		vmm_printf("\nVMFAIL: VMCS host GS BASE non canonical");
		return VMXERR_VMENTRY_INVALID_VM_HOST_STATE_FIELD;
	}
#endif

#if BX_SUPPORT_X86_64
	if (!IsCanonical(host_state->gdtr_base)) {
		vmm_printf("\nVMFAIL: VMCS host GDTR BASE non canonical");
		return VMXERR_VMENTRY_INVALID_VM_HOST_STATE_FIELD;
	}
	if (!IsCanonical(host_state->idtr_base)) {
		vmm_printf("\nVMFAIL: VMCS host IDTR BASE non canonical");
		return VMXERR_VMENTRY_INVALID_VM_HOST_STATE_FIELD;
	}
#endif

#if BX_SUPPORT_X86_64
	if (!IsCanonical(host_state->sysenter_esp_msr)) {
		vmm_printf("\nVMFAIL: VMCS host SYSENTER_ESP_MSR non canonical");
		return VMXERR_VMENTRY_INVALID_VM_HOST_STATE_FIELD;
	}

	if (!IsCanonical(host_state->sysenter_eip_msr)) {
		vmm_printf("\nVMFAIL: VMCS host SYSENTER_EIP_MSR non canonical");
		return VMXERR_VMENTRY_INVALID_VM_HOST_STATE_FIELD;
	}
#endif

#if BX_SUPPORT_VMX >= 2
	if (vmexit_ctrls & VMX_VMEXIT_CTRL1_LOAD_PAT_MSR) {
		//# host_state->pat_msr = VMread64(VMCS_64BIT_HOST_IA32_PAT);
		//TODO: Himanshu: Fix the packed MSR thing.
		//if (!isValidMSR_PAT(host_state->pat_msr)) {
		//	vmm_printf("\nVMFAIL: invalid Memory Type in host MSR_PAT");
		//	return VMXERR_VMENTRY_INVALID_VM_HOST_STATE_FIELD;
		//}
	}
#endif

#if BX_SUPPORT_X86_64

#if BX_SUPPORT_VMX >= 2
	if (vmexit_ctrls & VMX_VMEXIT_CTRL1_LOAD_EFER_MSR) {
		if (host_state->efer_msr & ~((Bit64u)efer_suppmask)) {
			vmm_printf("\nVMFAIL: VMCS host EFER reserved bits set !");
			return VMXERR_VMENTRY_INVALID_VM_HOST_STATE_FIELD;
		}
		bx_bool lme = (host_state->efer_msr >> 8) & 0x1;
		bx_bool lma = (host_state->efer_msr >> 10) & 0x1;
		if (lma != lme || lma != x86_64_host) {
			vmm_printf("\nVMFAIL: VMCS host EFER (0x%08x) inconsistent value !", (Bit32u)host_state->efer_msr);
			return VMXERR_VMENTRY_INVALID_VM_HOST_STATE_FIELD;
		}
	}
#endif

	if (x86_64_host) {
		if ((host_state->cr4 & BX_CR4_PAE_MASK) == 0) {
			vmm_printf("\nVMFAIL: VMCS host CR4.PAE=0 with x86-64 host");
			return VMXERR_VMENTRY_INVALID_VM_HOST_STATE_FIELD;
		}
		if (!IsCanonical(host_state->rip)) {
			vmm_printf("\nVMFAIL: VMCS host RIP non-canonical");
			return VMXERR_VMENTRY_INVALID_VM_HOST_STATE_FIELD;
		}
	}
	else {
		if (GET32H(host_state->rip) != 0) {
			vmm_printf("\nVMFAIL: VMCS host RIP > 32 bit");
			return VMXERR_VMENTRY_INVALID_VM_HOST_STATE_FIELD;
		}
		if (host_state->cr4 & BX_CR4_PCIDE_MASK) {
			vmm_printf("\nVMFAIL: VMCS host CR4.PCIDE set");
			return VMXERR_VMENTRY_INVALID_VM_HOST_STATE_FIELD;
		}
	}
#endif

	return VMXERR_NO_ERROR;
}

Bit32u VMenterLoadCheckGuestState(VMCS_CACHE *vm, Bit64u *qualification, UINT64 VMXON_Pointer, INT32 RevisionID)
{
	int error;
	static const char *segname[] = { "ES", "CS", "SS", "DS", "FS", "GS" };
	int n;

	VMCS_GUEST_STATE guest;

	*qualification = VMENTER_ERR_NO_ERROR;

	guest.rflags = __vmread_safe(VMCS_GUEST_RFLAGS, &error);
	vmm_printf("Guest RFLAGS: 0x%08lx\n", guest.rflags);

	/* RFLAGS reserved bits [63:22], bit 15, bit 5, bit 3 must be zero */
	if (guest.rflags & 0xFFFFFFFFFFC08028ULL) {
		vmm_printf("%s ERROR: RFLAGS reserved bits are set\n", __func__);
		return VMX_VMEXIT_VMENTRY_FAILURE_GUEST_STATE;
	}

	/* RFLAGS[1] must be always set */
	if ((guest.rflags & 0x2) == 0) {
		vmm_printf("%s ERROR: RFLAGS[1] cleared\n", __func__);
		return VMX_VMEXIT_VMENTRY_FAILURE_GUEST_STATE;
	}

	bx_bool v8086_guest = 0;
	if (guest.rflags & EFlagsVMMask) {
		vmm_printf("%s INFO: Guest in v8086 mode\n", __func__);
		v8086_guest = 1;
	} else {
		vmm_printf("Guest is not in v8086 mode\n");
	}

	bx_bool x86_64_guest = 0; // can't be 1 if X86_64 is not supported (checked before)
	Bit32u vmentry_ctrls = vm->vmentry_ctrls;
#if BX_SUPPORT_X86_64
	if (vmentry_ctrls & VMX_VMENTRY_CTRL1_X86_64_GUEST) {
		vmm_printf("%s INFO: x86-64 guest\n", __func__);
		x86_64_guest = 1;
	} else {
		vmm_printf("%s INFO: Not an x86-64 guest\n", __func__);
	}
#endif

	if (x86_64_guest && v8086_guest) {
		vmm_printf("%s FAIL: Enter to x86-64 guest with RFLAGS.VM\n", __func__);
		return VMX_VMEXIT_VMENTRY_FAILURE_GUEST_STATE;
	}

	guest.cr0 = __vmread_safe(VMCS_GUEST_CR0, &error);
	vmm_printf("Guest CR0: 0x%08lx\n", guest.cr0);
#if BX_SUPPORT_VMX >= 2
	if (vm->vmexec_ctrls3 & VMX_VM_EXEC_CTRL3_UNRESTRICTED_GUEST) {
		vmm_printf("%s INFO: Restricted guest is enabled\n", __func__);
		if (~guest.cr0 & (VMX_MSR_CR0_FIXED0 & ~(BX_CR0_PE_MASK | BX_CR0_PG_MASK))) {
			vmm_printf("%s FAIL: VMCS guest invalid CR0\n", __func__);
			return VMX_VMEXIT_VMENTRY_FAILURE_GUEST_STATE;
		}

		bx_bool pe = (guest.cr0 & BX_CR0_PE_MASK) != 0;
		bx_bool pg = (guest.cr0 & BX_CR0_PG_MASK) != 0;
		if (pg && !pe) {
			vmm_printf("%s FAIL: VMCS unrestricted guest CR0.PG without CR0.PE\n", __func__);
			return VMX_VMEXIT_VMENTRY_FAILURE_GUEST_STATE;
		}
	}
	else
#endif
	{
		if (~guest.cr0 & VMX_MSR_CR0_FIXED0) {
			vmm_printf("\nVMENTER FAIL: VMCS guest invalid CR0 (Check default0 settings)\n");
			return VMX_VMEXIT_VMENTRY_FAILURE_GUEST_STATE;
		}
	}

	if (guest.cr0 & ~VMX_MSR_CR0_FIXED1) {
		vmm_printf("\nVMENTER FAIL: VMCS guest invalid CR0 (Checked default1 settings)\n");
		return VMX_VMEXIT_VMENTRY_FAILURE_GUEST_STATE;
	}

	bx_bool real_mode_guest = 0;
	if (!(guest.cr0 & BX_CR0_PE_MASK)) {
		vmm_printf("%s INFO: Real mode guest (PE Bit not set)\n", __func__);
		real_mode_guest = 1;
	} else {
		vmm_printf("%s INFO: Non-real-mode guest (PE Bit set)\n", __func__);
	}

	guest.cr3 = __vmread_safe(VMCS_GUEST_CR3, &error);
	vmm_printf("Guest CR3: 0x%08lx\n", guest.cr3);
#if BX_SUPPORT_X86_64
	if (!IsValidPhyAddr(guest.cr3)) {
		vmm_printf("%s VMENTER FAIL: VMCS guest invalid CR3\n", __func__);
		return VMX_VMEXIT_VMENTRY_FAILURE_GUEST_STATE;
	}
#endif

	guest.cr4 = __vmread_safe(VMCS_GUEST_CR4, &error);
	vmm_printf("Guest CR4: 0x%08lx\n", guest.cr4);
	if (~guest.cr4 & VMX_MSR_CR4_FIXED0) {
		vmm_printf("%s: VMENTER FAIL: VMCS guest invalid CR4 (Check fixed0 settings)\n", __func__);
		return VMXERR_VMENTRY_INVALID_VM_HOST_STATE_FIELD;
	} else
		vmm_printf("CR4 Fixed 0 Settings are ok\n");

	if (guest.cr4 & ~VMX_MSR_CR4_FIXED1) {
		vmm_printf("%s VMENTER FAIL: VMCS guest invalid CR4 (Check fixed1 settings)\n", __func__);
		return VMXERR_VMENTRY_INVALID_VM_HOST_STATE_FIELD;
	} else
		vmm_printf("CR4 Fixed1 settings are ok\n");

#if BX_SUPPORT_X86_64
	if (x86_64_guest) {
		vmm_printf("x86_64_guest: Checking if PAE bit is set\n");
		if ((guest.cr4 & BX_CR4_PAE_MASK) == 0) {
			vmm_printf("%s VMENTER FAIL: VMCS guest CR4.PAE=0 in x86-64 mode\n", __func__);
			return VMX_VMEXIT_VMENTRY_FAILURE_GUEST_STATE;
		}
		vmm_printf("OK\n");
	}
	else {
		vmm_printf("non x86_64_guest: Checking for PCIDE Mask\n");
		if (guest.cr4 & BX_CR4_PCIDE_MASK) {
			vmm_printf("%s VMENTER FAIL: VMCS CR4.PCIDE set in 32-bit guest\n", __func__);
			return VMX_VMEXIT_VMENTRY_FAILURE_GUEST_STATE;
		}
		vmm_printf("OK\n");
	}
#endif

	vmm_printf("Guest CR4 check passed\n");

#if BX_SUPPORT_X86_64
	if (vmentry_ctrls & VMX_VMENTRY_CTRL1_LOAD_DBG_CTRLS) {
		vmm_printf("%s INFO CTRL1_LOAD_DBG_CTRLS is set\n", __func__);
		guest.dr7 = __vmread_safe(VMCS_GUEST_DR7, &error);
		if (GET32H(guest.dr7)) {
			vmm_printf("%s VMENTER FAIL: VMCS guest invalid DR7 (It should be 0)\n", __func__);
			return VMX_VMEXIT_VMENTRY_FAILURE_GUEST_STATE;
		}
	}
#endif

	vmm_printf("%s: checking guest segment registers\n", __func__);

	for (n = 0; n < 6; n++) {
		Bit16u selector;
		bx_address base;
		Bit32u limit;
		Bit32u ar;

		if (n == 0)
		{
			vmm_printf("%s: Checking guest ES...\n", __func__);
			selector = __vmread_safe(VMCS_16BIT_GUEST_ES_SELECTOR, &error);
			base = (bx_address)__vmread_safe(VMCS_GUEST_ES_BASE, &error);
			limit = __vmread_safe(VMCS_32BIT_GUEST_ES_LIMIT, &error);
			ar = __vmread_safe(VMCS_32BIT_GUEST_ES_ACCESS_RIGHTS, &error);
			vmm_printf("%s: Access Rights(AR): 0x%08x\n", __func__, ar);
		}
		if (n == 1)
		{
			vmm_printf("%s: Checking guest CS...\n", __func__);
			selector = __vmread_safe(VMCS_16BIT_GUEST_CS_SELECTOR, &error);
			base = (bx_address)__vmread_safe(VMCS_GUEST_CS_BASE, &error);
			limit = __vmread_safe(VMCS_32BIT_GUEST_CS_LIMIT, &error);
			ar = __vmread_safe(VMCS_32BIT_GUEST_CS_ACCESS_RIGHTS, &error);
			vmm_printf("%s: Access Rights(AR): 0x%08x\n", __func__, ar);
		}

		if (n == 2)
		{
			vmm_printf("%s: Checking guest SS...\n", __func__);
			selector = __vmread_safe(VMCS_16BIT_GUEST_SS_SELECTOR, &error);
			base = (bx_address)__vmread_safe(VMCS_GUEST_SS_BASE, &error);
			limit = __vmread_safe(VMCS_32BIT_GUEST_SS_LIMIT, &error);
			ar = __vmread_safe(VMCS_32BIT_GUEST_SS_ACCESS_RIGHTS, &error);
			vmm_printf("%s: AR: 0x%x\n", __func__, ar);
		}

		if (n == 3)
		{
			vmm_printf("Checking Guest DS...\n");
			selector = __vmread_safe(VMCS_16BIT_GUEST_DS_SELECTOR, &error);
			base = (bx_address)__vmread_safe(VMCS_GUEST_DS_BASE, &error);
			limit = __vmread_safe(VMCS_32BIT_GUEST_DS_LIMIT, &error);
			ar = __vmread_safe(VMCS_32BIT_GUEST_DS_ACCESS_RIGHTS, &error);
			vmm_printf("AR: 0x%x\n", ar);
		}

		if (n == 4)
		{
			vmm_printf("Checking Guest FS...\n");
			selector = __vmread_safe(VMCS_16BIT_GUEST_FS_SELECTOR, &error);
			base = (bx_address)__vmread_safe(VMCS_GUEST_FS_BASE, &error);
			limit = __vmread_safe(VMCS_32BIT_GUEST_FS_LIMIT, &error);
			ar = __vmread_safe(VMCS_32BIT_GUEST_FS_ACCESS_RIGHTS, &error);
			vmm_printf("AR: 0x%x\n", ar);
		}

		if (n == 5)
		{
			vmm_printf("Checking Guest GS...\n");
			selector = __vmread_safe(VMCS_16BIT_GUEST_GS_SELECTOR, &error);
			base = (bx_address)__vmread_safe(VMCS_GUEST_GS_BASE, &error);
			limit = __vmread_safe(VMCS_32BIT_GUEST_GS_LIMIT, &error);
			ar = __vmread_safe(VMCS_32BIT_GUEST_GS_ACCESS_RIGHTS, &error);
			vmm_printf("AR: 0x%x\n", ar);
		}

		ar = vmx_from_ar_byte_rd(ar);

		bx_bool invalid = (ar >> 16) & 1;

		vmm_printf("AR is valid\n");

		set_segment_ar_data(&guest.sregs[n], !invalid,
			(Bit16u)selector, base, limit, (Bit16u)ar);

		if (v8086_guest) {
			vmm_printf("Guest in v8086 mode. Checking v8086 settings.\n");
			/* guest in V8086 mode */
			if (base != ((bx_address)(selector << 4))) {
				vmm_printf("\nVMENTER FAIL: VMCS v8086 guest bad %s.BASE", segname[n]);
				return VMX_VMEXIT_VMENTRY_FAILURE_GUEST_STATE;
			}
			if (limit != 0xffff) {
				vmm_printf("\nVMENTER FAIL: VMCS v8086 guest %s.LIMIT != 0xFFFF", segname[n]);
				return VMX_VMEXIT_VMENTRY_FAILURE_GUEST_STATE;
			}
			// present, expand-up read/write accessed, segment, DPL=3
			if (ar != 0xF3) {
				vmm_printf("\nVMENTER FAIL: VMCS v8086 guest %s.AR != 0xF3", segname[n]);
				return VMX_VMEXIT_VMENTRY_FAILURE_GUEST_STATE;
			}

			continue; // go to next segment register
		}

#if BX_SUPPORT_X86_64
		if (n >= BX_SEG_REG_FS) {
			if (!IsCanonical(base)) {
				vmm_printf("\nVMENTER FAIL: VMCS guest %s.BASE non canonical", segname[n]);
				return VMX_VMEXIT_VMENTRY_FAILURE_GUEST_STATE;
			}
		}
#endif

		if (n != BX_SEG_REG_CS && invalid)
			continue;

#if BX_SUPPORT_X86_64
		if (n == BX_SEG_REG_SS && (selector & BX_SELECTOR_RPL_MASK) == 0) {
			// SS is allowed to be NULL selector if going to 64-bit guest
			if (x86_64_guest && guest.sregs[BX_SEG_REG_CS].cache.u.segment.l)
				continue;
		}

		if (n < BX_SEG_REG_FS) {
			if (GET32H(base) != 0) {
				vmm_printf("\nVMENTER FAIL: VMCS guest %s.BASE > 32 bit", segname[n]);
				return VMX_VMEXIT_VMENTRY_FAILURE_GUEST_STATE;
			}
		}
#endif

		if (!guest.sregs[n].cache.segment) {
			vmm_printf("\nVMENTER FAIL: VMCS guest %s not segment", segname[n]);
			return VMX_VMEXIT_VMENTRY_FAILURE_GUEST_STATE;
		}

		if (!guest.sregs[n].cache.p) {
			vmm_printf("\nVMENTER FAIL: VMCS guest %s not present", segname[n]);
			return VMX_VMEXIT_VMENTRY_FAILURE_GUEST_STATE;
		}

		if (!IsLimitAccessRightsConsistent(limit, ar)) {
			vmm_printf("\nVMENTER FAIL: VMCS guest %s.AR/LIMIT malformed", segname[n]);
			return VMX_VMEXIT_VMENTRY_FAILURE_GUEST_STATE;
		}

		if (n == BX_SEG_REG_CS) {
			// CS checks
			switch (guest.sregs[BX_SEG_REG_CS].cache.type) {
			case BX_CODE_EXEC_ONLY_ACCESSED:
			case BX_CODE_EXEC_READ_ACCESSED:
				// non-conforming segment
				if (guest.sregs[BX_SEG_REG_CS].selector.rpl != guest.sregs[BX_SEG_REG_CS].cache.dpl) {
					vmm_printf("\nVMENTER FAIL: VMCS guest non-conforming CS.RPL <> CS.DPL");
					return VMX_VMEXIT_VMENTRY_FAILURE_GUEST_STATE;
				}
				break;
			case BX_CODE_EXEC_ONLY_CONFORMING_ACCESSED:
			case BX_CODE_EXEC_READ_CONFORMING_ACCESSED:
				// conforming segment
				if (guest.sregs[BX_SEG_REG_CS].selector.rpl < guest.sregs[BX_SEG_REG_CS].cache.dpl) {
					vmm_printf("\nVMENTER FAIL: VMCS guest non-conforming CS.RPL < CS.DPL");
					return VMX_VMEXIT_VMENTRY_FAILURE_GUEST_STATE;
				}
				break;
#if BX_SUPPORT_VMX >= 2
			case BX_DATA_READ_WRITE_ACCESSED:
				if (vm->vmexec_ctrls3 & VMX_VM_EXEC_CTRL3_UNRESTRICTED_GUEST) {
					if (guest.sregs[BX_SEG_REG_CS].cache.dpl != 0) {
						vmm_printf("\nVMENTER FAIL: VMCS unrestricted guest CS.DPL != 0");
						return VMX_VMEXIT_VMENTRY_FAILURE_GUEST_STATE;
					}
					break;
				}
				// fall through
#endif
			default:
				vmm_printf("\nVMENTER FAIL: VMCS guest CS.TYPE");
				return VMX_VMEXIT_VMENTRY_FAILURE_GUEST_STATE;
			}

#if BX_SUPPORT_X86_64
			if (x86_64_guest) {
				if (guest.sregs[BX_SEG_REG_CS].cache.u.segment.d_b && guest.sregs[BX_SEG_REG_CS].cache.u.segment.l) {
					vmm_printf("\nVMENTER FAIL: VMCS x86_64 guest wrong CS.D_B/L");
					return VMX_VMEXIT_VMENTRY_FAILURE_GUEST_STATE;
				}
			}
#endif
		}
		else if (n == BX_SEG_REG_SS) {
			// SS checks
			switch (guest.sregs[BX_SEG_REG_SS].cache.type) {
			case BX_DATA_READ_WRITE_ACCESSED:
			case BX_DATA_READ_WRITE_EXPAND_DOWN_ACCESSED:
				break;
			default:
				vmm_printf("\nVMENTER FAIL: VMCS guest SS.TYPE");
				return VMX_VMEXIT_VMENTRY_FAILURE_GUEST_STATE;
			}
		}
		else {
			// DS, ES, FS, GS
			if ((guest.sregs[n].cache.type & 0x1) == 0) {
				vmm_printf("\nVMENTER FAIL: VMCS guest %s not ACCESSED", segname[n]);
				return VMX_VMEXIT_VMENTRY_FAILURE_GUEST_STATE;
			}

			if (guest.sregs[n].cache.type & 0x8) {
				if ((guest.sregs[n].cache.type & 0x2) == 0) {
					vmm_printf("\nVMENTER FAIL: VMCS guest CODE segment %s not READABLE", segname[n]);
					return VMX_VMEXIT_VMENTRY_FAILURE_GUEST_STATE;
				}
			}

			if (!(vm->vmexec_ctrls3 & VMX_VM_EXEC_CTRL3_UNRESTRICTED_GUEST)) {
				if (guest.sregs[n].cache.type < 11) {
					// data segment or non-conforming code segment
					if (guest.sregs[n].selector.rpl > guest.sregs[n].cache.dpl) {
						vmm_printf("\nVMENTER FAIL: VMCS guest non-conforming %s.RPL < %s.DPL", segname[n], segname[n]);
						return VMX_VMEXIT_VMENTRY_FAILURE_GUEST_STATE;
					}
				}
			}
		}
	}

	if (!v8086_guest) {
		if (!(vm->vmexec_ctrls3 & VMX_VM_EXEC_CTRL3_UNRESTRICTED_GUEST)) {
			vmm_printf("Guest not in v8086 mode and unrestricted guest is also not set\n");
			if (guest.sregs[BX_SEG_REG_SS].selector.rpl != guest.sregs[BX_SEG_REG_CS].selector.rpl) {
				vmm_printf("\nVMENTER FAIL: VMCS guest CS.RPL != SS.RPL");
				return VMX_VMEXIT_VMENTRY_FAILURE_GUEST_STATE;
			}
			if (guest.sregs[BX_SEG_REG_SS].selector.rpl != guest.sregs[BX_SEG_REG_SS].cache.dpl) {
				vmm_printf("\nVMENTER FAIL: VMCS guest SS.RPL <> SS.DPL");
				return VMX_VMEXIT_VMENTRY_FAILURE_GUEST_STATE;
			}
		}
#if BX_SUPPORT_VMX >= 2
		else { // unrestricted guest
			vmm_printf("Unrestricted guest mode: Real mode: %s\n", (real_mode_guest ? "YES": "NO"));
			if (real_mode_guest || guest.sregs[BX_SEG_REG_CS].cache.type == BX_DATA_READ_WRITE_ACCESSED) {
				if (guest.sregs[BX_SEG_REG_SS].cache.dpl != 0) {
					vmm_printf("\nVMENTER FAIL: VMCS unrestricted guest SS.DPL != 0");
					return VMX_VMEXIT_VMENTRY_FAILURE_GUEST_STATE;
				}
			}
		}
#endif
	}

	Bit64u gdtr_base = __vmread_safe(VMCS_GUEST_GDTR_BASE, &error);
	Bit32u gdtr_limit = __vmread_safe(VMCS_32BIT_GUEST_GDTR_LIMIT, &error);
	Bit64u idtr_base = __vmread_safe(VMCS_GUEST_IDTR_BASE, &error);
	Bit32u idtr_limit = __vmread_safe(VMCS_32BIT_GUEST_IDTR_LIMIT, &error);

#if BX_SUPPORT_X86_64
	if (!IsCanonical(gdtr_base) || !IsCanonical(idtr_base)) {
		vmm_printf("\nVMENTER FAIL: VMCS guest IDTR/IDTR.BASE non canonical");
		return VMX_VMEXIT_VMENTRY_FAILURE_GUEST_STATE;
	}
#endif
	if (gdtr_limit > 0xffff || idtr_limit > 0xffff) {
		vmm_printf("\nVMENTER FAIL: VMCS guest GDTR/IDTR limit > 0xFFFF");
		return VMX_VMEXIT_VMENTRY_FAILURE_GUEST_STATE;
	}

	Bit16u ldtr_selector = __vmread_safe(VMCS_16BIT_GUEST_LDTR_SELECTOR, &error);
	Bit64u ldtr_base = __vmread_safe(VMCS_GUEST_LDTR_BASE, &error);
	Bit32u ldtr_limit = __vmread_safe(VMCS_32BIT_GUEST_LDTR_LIMIT, &error);
	Bit32u ldtr_ar = __vmread_safe(VMCS_32BIT_GUEST_LDTR_ACCESS_RIGHTS, &error);

	ldtr_ar = vmx_from_ar_byte_rd(ldtr_ar);
	vmm_printf("LDTR AR: 0x%08x\n", ldtr_ar);

	bx_bool ldtr_invalid = (ldtr_ar >> 16) & 1;
	vmm_printf("LDTR is %svalid\n", (ldtr_invalid ? "NOT ":  ""));
	if (set_segment_ar_data(&guest.ldtr, !ldtr_invalid,
		(Bit16u)ldtr_selector, ldtr_base, ldtr_limit, (Bit16u)(ldtr_ar)))
	{
		// ldtr is valid
		if (guest.ldtr.selector.ti) {
			vmm_printf("\nVMENTER FAIL: VMCS guest LDTR.TI set");
			return VMX_VMEXIT_VMENTRY_FAILURE_GUEST_STATE;
		}
		if (guest.ldtr.cache.type != BX_SYS_SEGMENT_LDT) {
			vmm_printf("\nVMENTER FAIL: VMCS guest incorrect LDTR type (%d)", guest.ldtr.cache.type);
			return VMX_VMEXIT_VMENTRY_FAILURE_GUEST_STATE;
		}
		if (guest.ldtr.cache.segment) {
			vmm_printf("\nVMENTER FAIL: VMCS guest LDTR is not system segment");
			return VMX_VMEXIT_VMENTRY_FAILURE_GUEST_STATE;
		}
		if (!guest.ldtr.cache.p) {
			vmm_printf("\nVMENTER FAIL: VMCS guest LDTR not present");
			return VMX_VMEXIT_VMENTRY_FAILURE_GUEST_STATE;
		}
		if (!IsLimitAccessRightsConsistent(ldtr_limit, ldtr_ar)) {
			vmm_printf("\nVMENTER FAIL: VMCS guest LDTR.AR/LIMIT malformed");
			return VMX_VMEXIT_VMENTRY_FAILURE_GUEST_STATE;
		}
#if BX_SUPPORT_X86_64
		if (!IsCanonical(ldtr_base)) {
			vmm_printf("\nVMENTER FAIL: VMCS guest LDTR.BASE non canonical");
			return VMX_VMEXIT_VMENTRY_FAILURE_GUEST_STATE;
		}
#endif
	}

	Bit16u tr_selector = __vmread_safe(VMCS_16BIT_GUEST_TR_SELECTOR, &error);
	Bit64u tr_base = __vmread_safe(VMCS_GUEST_TR_BASE, &error);
	Bit32u tr_limit = __vmread_safe(VMCS_32BIT_GUEST_TR_LIMIT, &error);
	Bit32u tr_ar = __vmread_safe(VMCS_32BIT_GUEST_TR_ACCESS_RIGHTS, &error);

	tr_ar = vmx_from_ar_byte_rd(tr_ar);
	bx_bool tr_invalid = (tr_ar >> 16) & 1;

#if BX_SUPPORT_X86_64
	if (!IsCanonical(tr_base)) {
		vmm_printf("\nVMENTER FAIL: VMCS guest TR.BASE non canonical");
		return VMX_VMEXIT_VMENTRY_FAILURE_GUEST_STATE;
	}
#endif

	set_segment_ar_data(&guest.tr, !tr_invalid,
		(Bit16u)tr_selector, tr_base, tr_limit, (Bit16u)(tr_ar));

	if (tr_invalid) {
		vmm_printf("\nVMENTER FAIL: VMCS guest TR invalid");
		return VMX_VMEXIT_VMENTRY_FAILURE_GUEST_STATE;
	}
	if (guest.tr.selector.ti) {
		vmm_printf("\nVMENTER FAIL: VMCS guest TR.TI set");
		return VMX_VMEXIT_VMENTRY_FAILURE_GUEST_STATE;
	}
	if (guest.tr.cache.segment) {
		vmm_printf("\nVMENTER FAIL: VMCS guest TR is not system segment");
		return VMX_VMEXIT_VMENTRY_FAILURE_GUEST_STATE;
	}
	if (!guest.tr.cache.p) {
		vmm_printf("\nVMENTER FAIL: VMCS guest TR not present");
		return VMX_VMEXIT_VMENTRY_FAILURE_GUEST_STATE;
	}
	if (!IsLimitAccessRightsConsistent(tr_limit, tr_ar)) {
		vmm_printf("\nVMENTER FAIL: VMCS guest TR.AR/LIMIT malformed");
		return VMX_VMEXIT_VMENTRY_FAILURE_GUEST_STATE;
	}

	switch (guest.tr.cache.type) {
	case BX_SYS_SEGMENT_BUSY_386_TSS:
		break;
	case BX_SYS_SEGMENT_BUSY_286_TSS:
		if (!x86_64_guest) break;
		// fall through
	default:
		vmm_printf("\nVMENTER FAIL: VMCS guest incorrect TR type");
		return VMX_VMEXIT_VMENTRY_FAILURE_GUEST_STATE;
	}

	guest.ia32_debugctl_msr = __vmread_safe(VMCS_64BIT_GUEST_IA32_DEBUGCTL, &error);
	guest.smbase = __vmread_safe(VMCS_32BIT_GUEST_SMBASE, &error);
	guest.sysenter_esp_msr = __vmread_safe(VMCS_GUEST_IA32_SYSENTER_ESP_MSR, &error);
	guest.sysenter_eip_msr = __vmread_safe(VMCS_GUEST_IA32_SYSENTER_EIP_MSR, &error);
	guest.sysenter_cs_msr = __vmread_safe(VMCS_32BIT_GUEST_IA32_SYSENTER_CS_MSR,
					      &error);

#if BX_SUPPORT_X86_64
	if (!IsCanonical(guest.sysenter_esp_msr)) {
		vmm_printf("\nVMENTER FAIL: VMCS guest SYSENTER_ESP_MSR non canonical");
		return VMX_VMEXIT_VMENTRY_FAILURE_GUEST_STATE;
	}
	if (!IsCanonical(guest.sysenter_eip_msr)) {
		vmm_printf("\nVMENTER FAIL: VMCS guest SYSENTER_EIP_MSR non canonical");
		return VMX_VMEXIT_VMENTRY_FAILURE_GUEST_STATE;
	}
#endif

#if BX_SUPPORT_VMX >= 2
	if (vmentry_ctrls & VMX_VMENTRY_CTRL1_LOAD_PAT_MSR) {
		guest.pat_msr = __vmread_safe(VMCS_64BIT_GUEST_IA32_PAT, &error);
		// TODO: Himansu Fix the packed register thing.
		//if (!isValidMSR_PAT(guest.pat_msr)) {
		//	vmm_printf("\nVMENTER FAIL: invalid Memory Type in guest MSR_PAT"));
		//	return VMX_VMEXIT_VMENTRY_FAILURE_GUEST_STATE;
		//}
	}
#endif

	guest.rip = __vmread_safe(VMCS_GUEST_RIP, &error);
	guest.rsp = __vmread_safe(VMCS_GUEST_RSP, &error);

#if BX_SUPPORT_VMX >= 2 && BX_SUPPORT_X86_64
	if (vmentry_ctrls & VMX_VMENTRY_CTRL1_LOAD_EFER_MSR) {
		guest.efer_msr = __vmread_safe(VMCS_64BIT_GUEST_IA32_EFER, &error);

		if (guest.efer_msr & ~((Bit64u)efer_suppmask)) {
			vmm_printf("\nVMENTER FAIL: VMCS guest EFER reserved bits set !");
			return VMX_VMEXIT_VMENTRY_FAILURE_GUEST_STATE;
		}
		bx_bool lme = (guest.efer_msr >> 8) & 0x1;
		bx_bool lma = (guest.efer_msr >> 10) & 0x1;
		if (lma != x86_64_guest) {
			vmm_printf("\nVMENTER FAIL: VMCS guest EFER.LMA doesn't match x86_64_guest !");
			return VMX_VMEXIT_VMENTRY_FAILURE_GUEST_STATE;
		}
		if (lma != lme && (guest.cr0 & BX_CR0_PG_MASK) != 0) {
			vmm_printf("\nVMENTER FAIL: VMCS guest EFER (0x%08x) inconsistent value !", (Bit32u)guest.efer_msr);
			return VMX_VMEXIT_VMENTRY_FAILURE_GUEST_STATE;
		}
	}

	if (!x86_64_guest || !guest.sregs[BX_SEG_REG_CS].cache.u.segment.l) {
		if (GET32H(guest.rip) != 0) {
			vmm_printf("\nVMENTER FAIL: VMCS guest RIP > 32 bit");
			return VMX_VMEXIT_VMENTRY_FAILURE_GUEST_STATE;
		}
	}
#endif

	vm->vmcs_linkptr = __vmread_safe(VMCS_64BIT_GUEST_LINK_POINTER, &error);

	if (vm->vmcs_linkptr != BX_INVALID_VMCSPTR) {
		if (!IsValidPageAlignedPhyAddr(vm->vmcs_linkptr)) {
			*qualification = (Bit64u)VMENTER_ERR_GUEST_STATE_LINK_POINTER;
			vmm_printf("\nVMFAIL: VMCS link pointer malformed");
			return VMX_VMEXIT_VMENTRY_FAILURE_GUEST_STATE;
		}

		Bit32u revision = RevisionID;
		if (vm->vmexec_ctrls3 & VMX_VM_EXEC_CTRL3_VMCS_SHADOWING) {
			if ((revision & BX_VMCS_SHADOW_BIT_MASK) == 0) {
				*qualification = (Bit64u)VMENTER_ERR_GUEST_STATE_LINK_POINTER;
				vmm_printf("\nVMFAIL: VMCS link pointer must indicate shadow VMCS revision ID = %d", revision);
				return VMX_VMEXIT_VMENTRY_FAILURE_GUEST_STATE;
			}
			revision &= ~BX_VMCS_SHADOW_BIT_MASK;
		}
	}

	guest.tmpDR6 = (Bit32u)__vmread_safe(VMCS_GUEST_PENDING_DBG_EXCEPTIONS, &error);
	if (guest.tmpDR6 & BX_CONST64(0xFFFFFFFFFFFFAFF0)) {
		vmm_printf("\nVMENTER FAIL: VMCS guest tmpDR6 reserved bits");
		return VMX_VMEXIT_VMENTRY_FAILURE_GUEST_STATE;
	}

	guest.activity_state = __vmread_safe(VMCS_32BIT_GUEST_ACTIVITY_STATE, &error);

	if (guest.activity_state > BX_VMX_LAST_ACTIVITY_STATE) {
		vmm_printf("\nVMENTER FAIL: VMCS guest activity state %d", guest.activity_state);
		return VMX_VMEXIT_VMENTRY_FAILURE_GUEST_STATE;
	}

	if (guest.activity_state == BX_ACTIVITY_STATE_HLT) {
		if (guest.sregs[BX_SEG_REG_SS].cache.dpl != 0) {
			vmm_printf("\nVMENTER FAIL: VMCS guest HLT state with SS.DPL=%d", guest.sregs[BX_SEG_REG_SS].cache.dpl);
			return VMX_VMEXIT_VMENTRY_FAILURE_GUEST_STATE;
		}
	}

	guest.interruptibility_state = __vmread_safe(VMCS_32BIT_GUEST_INTERRUPTIBILITY_STATE, &error);
	if (guest.interruptibility_state & ~BX_VMX_INTERRUPTIBILITY_STATE_MASK) {
		vmm_printf("\nVMENTER FAIL: VMCS guest interruptibility state broken");
		return VMX_VMEXIT_VMENTRY_FAILURE_GUEST_STATE;
	}

	if (guest.interruptibility_state & 0x3) {
		if (guest.activity_state != BX_ACTIVITY_STATE_ACTIVE) {
			vmm_printf("\nVMENTER FAIL: VMCS guest interruptibility state broken when entering non active CPU state %d", guest.activity_state);
			return VMX_VMEXIT_VMENTRY_FAILURE_GUEST_STATE;
		}
	}

	if ((guest.interruptibility_state & BX_VMX_INTERRUPTS_BLOCKED_BY_STI) &&
		(guest.interruptibility_state & BX_VMX_INTERRUPTS_BLOCKED_BY_MOV_SS)) {
		vmm_printf("\nVMENTER FAIL: VMCS guest interruptibility state broken");
		return VMX_VMEXIT_VMENTRY_FAILURE_GUEST_STATE;
	}

	if ((guest.rflags & EFlagsIFMask) == 0) {
		if (guest.interruptibility_state & BX_VMX_INTERRUPTS_BLOCKED_BY_STI) {
			vmm_printf("\nVMENTER FAIL: VMCS guest interrupts can't be blocked by STI when EFLAGS.IF = 0");
			return VMX_VMEXIT_VMENTRY_FAILURE_GUEST_STATE;
		}
	}

	if (VMENTRY_INJECTING_EVENT(vm->vmentry_interr_info)) {
		unsigned event_type = (vm->vmentry_interr_info >> 8) & 7;
		unsigned vector = vm->vmentry_interr_info & 0xff;
		if (event_type == BX_EXTERNAL_INTERRUPT) {
			if ((guest.interruptibility_state & 0x3) != 0 || (guest.rflags & EFlagsIFMask) == 0) {
				vmm_printf("\nVMENTER FAIL: VMCS guest interrupts blocked when injecting external interrupt");
				return VMX_VMEXIT_VMENTRY_FAILURE_GUEST_STATE;
			}
		}
		if (event_type == BX_NMI) {
			if ((guest.interruptibility_state & 0x3) != 0) {
				vmm_printf("\nVMENTER FAIL: VMCS guest interrupts blocked when injecting NMI");
				return VMX_VMEXIT_VMENTRY_FAILURE_GUEST_STATE;
			}
		}
		if (guest.activity_state == BX_ACTIVITY_STATE_WAIT_FOR_SIPI) {
			vmm_printf("\nVMENTER FAIL: No guest interruptions are allowed when entering Wait-For-Sipi state");
			return VMX_VMEXIT_VMENTRY_FAILURE_GUEST_STATE;
		}
		if (guest.activity_state == BX_ACTIVITY_STATE_SHUTDOWN && event_type != BX_NMI && vector != BX_MC_EXCEPTION) {
			vmm_printf("\nVMENTER FAIL: Only NMI or #MC guest interruption is allowed when entering shutdown state");
			return VMX_VMEXIT_VMENTRY_FAILURE_GUEST_STATE;
		}
	}

	if (vmentry_ctrls & VMX_VMENTRY_CTRL1_SMM_ENTER) {
		if (!(guest.interruptibility_state & BX_VMX_INTERRUPTS_BLOCKED_SMI_BLOCKED)) {
			vmm_printf("\nVMENTER FAIL: VMCS SMM guest should block SMI");
			return VMX_VMEXIT_VMENTRY_FAILURE_GUEST_STATE;
		}

		if (guest.activity_state == BX_ACTIVITY_STATE_WAIT_FOR_SIPI) {
			vmm_printf("\nVMENTER FAIL: The activity state must not indicate the wait-for-SIPI state if entering to SMM guest");
			return VMX_VMEXIT_VMENTRY_FAILURE_GUEST_STATE;
		}
	}

	if (!x86_64_guest && (guest.cr4 & BX_CR4_PAE_MASK) != 0 && (guest.cr0 & BX_CR0_PG_MASK) != 0) {
#if BX_SUPPORT_VMX >= 2
		if (vm->vmexec_ctrls3 & VMX_VM_EXEC_CTRL3_EPT_ENABLE) {
			for (n = 0; n < 4; n++) {
				if (n == 0)
					guest.pdptr[n] = __vmread_safe(VMCS_64BIT_GUEST_IA32_PDPTE0 + 2 * n, &error);

				if (n == 1)
					guest.pdptr[n] = __vmread_safe(VMCS_64BIT_GUEST_IA32_PDPTE1 + 2 * n, &error);

				if (n == 2)
					guest.pdptr[n] = __vmread_safe(VMCS_64BIT_GUEST_IA32_PDPTE2, &error);

				if (n == 3)
					guest.pdptr[n] = __vmread_safe(VMCS_64BIT_GUEST_IA32_PDPTE3, &error);
			}

			if (!CheckPDPTR(guest.pdptr)) {
				*qualification = VMENTER_ERR_GUEST_STATE_PDPTR_LOADING;
				vmm_printf("\nVMENTER: EPT Guest State PDPTRs Checks Failed");
				return VMX_VMEXIT_VMENTRY_FAILURE_GUEST_STATE;
			}
		}
		else
#endif
		{
			if (!CheckPDPTR((Bit64u*)guest.cr3)) {
				*qualification = VMENTER_ERR_GUEST_STATE_PDPTR_LOADING;
				vmm_printf("\nVMENTER: Guest State PDPTRs Checks Failed");
				return VMX_VMEXIT_VMENTRY_FAILURE_GUEST_STATE;
			}
		}
	}

	vmm_printf("\nAll the guest-state checks are performed successfully.");

	return VMXERR_NO_ERROR;
}


BOOLEAN CheckVMXState(VMCS_CACHE *pVm, BOOLEAN IsVMResume, UINT64 VMXON_Pointer, INT32 RevisionID,
	UINT32 _vmx_pin_vmexec_ctrl_supported_bits, UINT32 _vmx_proc_vmexec_ctrl_supported_bits,
	UINT32 _vmx_vmexec_ctrl2_supported_bits, UINT32 _vmx_vmexit_ctrl_supported_bits,
	UINT32 _vmx_vmentry_ctrl_supported_bits, UINT64 _vmx_ept_vpid_cap_supported_bits,
	UINT64 _vmx_vmfunc_supported_bits, UINT32 _cr0_suppmask_0, UINT32 _cr0_suppmask_1,
	UINT32 _cr4_suppmask_0, UINT32 _cr4_suppmask_1)
{
	int _error;
       
	vmx_pin_vmexec_ctrl_supported_bits = _vmx_pin_vmexec_ctrl_supported_bits;
	vmx_proc_vmexec_ctrl_supported_bits = _vmx_proc_vmexec_ctrl_supported_bits;
	vmx_vmexec_ctrl2_supported_bits = _vmx_vmexec_ctrl2_supported_bits;
	vmx_vmexit_ctrl_supported_bits = _vmx_vmexit_ctrl_supported_bits;
	vmx_vmentry_ctrl_supported_bits = _vmx_vmentry_ctrl_supported_bits;
	vmx_ept_vpid_cap_supported_bits = _vmx_ept_vpid_cap_supported_bits;
	vmx_vmfunc_supported_bits = _vmx_ept_vpid_cap_supported_bits;

	/*
	 * If bit in X_FIXED0 is 1 then it should be also fixed 1
	 * If bit in X_FIXED1 is 0 then it should be also fixed to 0
	 * So FIXED0 and FIXED1 cannot have different values
	 * X_FIXED1 is almost 0xffffffff means that all of the are allowed to be 1
	 */

	/*
	 * The restrictions on CR0.PE and CR0.PG imply that VMX operation is supported only in paged protected mode.
	 * Therefore, guest software cannot be run in unpaged protected mode or in real-address mode.
	 *
	 * Later processors support a VM-execution control called “unrestricted guest”.
	 * If this control is 1, CR0.PE and CR0.PG may be 0 in VMX non-root
	 * operation (even if the capability MSR IA32_VMX_CR0_FIXED0 reports otherwise).
	 * Such processors allow guest software to run in unpaged protected mode or in real-address mode.
	*/

	cr4_suppmask_0 = _cr4_suppmask_0;
	cr4_suppmask_1 = _cr4_suppmask_1;
	cr0_suppmask_0 = _cr0_suppmask_0;
	cr0_suppmask_1 = _cr0_suppmask_1;

	efer_suppmask = 0xFFFFFFFF;

	init_vmx_extensions_bitmask();


	unsigned vmlaunch = 0;

	if (IsVMResume) {
		vmm_printf("\n\n[*] VMLAUNCH VMCS CALLED ON CURRENT PROCESSOR VMCS PTR.");
		vmlaunch = 1;
	}
	else {
		vmm_printf("\n\n[*] VMRESUME VMCS CALLED ON CURRENT PROCESSOR VMCS PTR.");
	}

	Bit32u launch_state = __vmread_safe(VMCS_LAUNCH_STATE_FIELD_ENCODING, &_error);
	vmm_printf("Launch State: 0x%08x\n", launch_state);

	if (vmlaunch) {
		if (launch_state != VMCS_STATE_CLEAR) {
			vmm_printf("\nVMFAIL: VMLAUNCH with non-clear VMCS!");
			VMfail(VMXERR_VMLAUNCH_NON_CLEAR_VMCS);

		}
	}
	else {
		if (launch_state != VMCS_STATE_LAUNCHED) {
			vmm_printf("\nVMFAIL: VMRESUME with non-launched VMCS!");
			VMfail(VMXERR_VMRESUME_NON_LAUNCHED_VMCS);

		}
	}

	VMX_error_code error = VMenterLoadCheckVmControls(pVm);
	if (error != VMXERR_NO_ERROR) {
		VMfail(error);

	}

	error = VMenterLoadCheckHostState(pVm);
	if (error != VMXERR_NO_ERROR) {
		VMfail(error);

	}

	Bit64u qualification = VMENTER_ERR_NO_ERROR;
	Bit32u state_load_error = VMenterLoadCheckGuestState(pVm, &qualification, VMXON_Pointer, RevisionID);
	if (state_load_error) {
		vmm_printf("\nVMEXIT: Guest State Checks Failed");
		VMexit(VMX_VMEXIT_VMENTRY_FAILURE_GUEST_STATE | (1 << 31), qualification);
	}

	vmm_printf("\nAll the states checked successfully\n");

	return TRUE;
}
