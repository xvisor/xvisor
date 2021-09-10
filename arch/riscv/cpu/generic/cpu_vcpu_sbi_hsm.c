/**
 * Copyright (c) 2020 Western Digital Corporation or its affiliates.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * @file cpu_vcpu_sbi_hsm.c
 * @author Anup Patel (anup.patel@wdc.com)
 * @brief source of SBI v0.2 HSM extension
 */

#include <vmm_error.h>
#include <vmm_macros.h>
#include <vmm_manager.h>
#include <vmm_guest_aspace.h>
#include <cpu_vcpu_sbi.h>
#include <riscv_sbi.h>

static int vcpu_sbi_hsm_ecall(struct vmm_vcpu *vcpu,
			      unsigned long ext_id, unsigned long func_id,
			      unsigned long *args, unsigned long *out_val,
			      struct cpu_vcpu_trap *out_trap)
{
	int rc;
	u32 reg_flags = 0x0;
	physical_addr_t outaddr;
	physical_size_t availsz;
	struct vmm_vcpu *rvcpu;
	struct vmm_guest *guest = vcpu->guest;

	switch (func_id) {
	case SBI_EXT_HSM_HART_START:
		rvcpu = vmm_manager_guest_vcpu(guest, args[0]);
		if (!rvcpu || (rvcpu == vcpu) ||
		    (vmm_manager_vcpu_get_state(rvcpu) != VMM_VCPU_STATE_RESET))
			return SBI_ERR_INVALID_PARAM;

		rc = vmm_guest_physical_map(guest, args[1], 1,
				    &outaddr, &availsz, &reg_flags);
		if (rc || !(reg_flags & (VMM_REGION_ISRAM | VMM_REGION_ISROM)))
			return SBI_ERR_INVALID_ADDRESS;

		riscv_regs(rvcpu)->sepc = args[1];
		riscv_regs(rvcpu)->a0 = rvcpu->subid;
		riscv_regs(rvcpu)->a1 = args[2];
		if (vmm_manager_vcpu_kick(rvcpu))
			return SBI_ERR_FAILED;
		break;
	case SBI_EXT_HSM_HART_STOP:
		if (vmm_manager_vcpu_reset(vcpu))
			return SBI_ERR_FAILED;
		break;
	case SBI_EXT_HSM_HART_GET_STATUS:
		rvcpu = vmm_manager_guest_vcpu(guest, args[0]);
		if (!rvcpu)
			return SBI_ERR_INVALID_PARAM;
		if (vmm_manager_vcpu_get_state(rvcpu) != VMM_VCPU_STATE_RESET)
			*out_val = SBI_HSM_STATE_STARTED;
		else
			*out_val = SBI_HSM_STATE_STOPPED;
		break;
	default:
		return SBI_ERR_NOT_SUPPORTED;
	}

	return 0;
}

const struct cpu_vcpu_sbi_extension vcpu_sbi_hsm = {
	.extid_start = SBI_EXT_HSM,
	.extid_end = SBI_EXT_HSM,
	.handle = vcpu_sbi_hsm_ecall,
};
