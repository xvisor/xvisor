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
 * @file cpu_vcpu_sbi_replace.c
 * @author Anup Patel (anup.patel@wdc.com)
 * @brief source of SBI v0.2 replacement extensions
 */

#include <vmm_error.h>
#include <vmm_macros.h>
#include <vmm_manager.h>
#include <vmm_vcpu_irq.h>
#include <cpu_vcpu_sbi.h>
#include <riscv_sbi.h>

static int vcpu_sbi_ipi_ecall(struct vmm_vcpu *vcpu,
			       unsigned long ext_id, unsigned long func_id,
			       unsigned long *args, unsigned long *out_val,
			       struct cpu_vcpu_trap *out_trap)
{
	struct vmm_vcpu *rvcpu;
	struct vmm_guest *guest = vcpu->guest;
	unsigned long hmask = args[0], hbase = args[1];

	if (func_id != SBI_EXT_IPI_SEND_IPI)
		return SBI_ERR_NOT_SUPPORTED;

	vmm_manager_for_each_guest_vcpu(rvcpu, guest) {
		if (!(vmm_manager_vcpu_get_state(rvcpu) &
		      VMM_VCPU_STATE_INTERRUPTIBLE))
			continue;
		if (hbase != -1UL) {
			if (rvcpu->subid < hbase)
				continue;
			if (!(hmask & (1UL << (rvcpu->subid - hbase))))
				continue;
		}
		vmm_vcpu_irq_assert(rvcpu, IRQ_VS_SOFT, 0x0);
	}

	return 0;
}

const struct cpu_vcpu_sbi_extension vcpu_sbi_ipi = {
	.extid_start = SBI_EXT_IPI,
	.extid_end = SBI_EXT_IPI,
	.handle = vcpu_sbi_ipi_ecall,
};
