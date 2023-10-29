/**
 * Copyright (c) 2022 Ventana Micro Systems Inc.
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
 * @file cpu_vcpu_sbi_xvisor.c
 * @author Anup Patel (apatel@ventanamicro.com)
 * @brief source of SBI Xvisor extension
 */

#include <vmm_error.h>
#include <vmm_macros.h>
#include <vmm_manager.h>
#include <vmm_guest_aspace.h>

#include <cpu_hwcap.h>
#include <cpu_vcpu_sbi.h>
#include <riscv_sbi.h>

#define SBI_EXT_XVISOR			(SBI_EXT_FIRMWARE_START + \
					 CPU_VCPU_SBI_IMPID)

#define SBI_EXT_XVISOR_ISA_EXT		0x0

static int vcpu_sbi_xvisor_ecall(struct vmm_vcpu *vcpu, unsigned long ext_id,
				 unsigned long func_id, unsigned long *args,
				 struct cpu_vcpu_sbi_return *out)
{
	switch (func_id) {
	case SBI_EXT_XVISOR_ISA_EXT:
		if (args[0] < RISCV_ISA_EXT_MAX) {
			out->value = __riscv_isa_extension_available(
						riscv_priv(vcpu)->isa,
						args[0]);
		} else {
			return SBI_ERR_INVALID_PARAM;
		}
		break;
	default:
		return SBI_ERR_NOT_SUPPORTED;
	}

	return 0;
}

const struct cpu_vcpu_sbi_extension vcpu_sbi_xvisor = {
	.name = "xvisor",
	.extid_start = SBI_EXT_XVISOR,
	.extid_end = SBI_EXT_XVISOR,
	.handle = vcpu_sbi_xvisor_ecall,
};
