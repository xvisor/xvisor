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
 * @file cpu_vcpu_sbi_base.c
 * @author Anup Patel (anup.patel@wdc.com)
 * @brief source of SBI v0.2 base extension
 */

#include <vmm_error.h>
#include <vmm_macros.h>
#include <vmm_manager.h>
#include <vmm_version.h>
#include <cpu_sbi.h>
#include <cpu_vcpu_sbi.h>
#include <riscv_sbi.h>

static int vcpu_sbi_base_ecall(struct vmm_vcpu *vcpu, unsigned long ext_id,
			       unsigned long func_id, unsigned long *args,
			       struct cpu_vcpu_sbi_return *out)
{
	int ret = 0;
	struct sbiret hret;

	switch (func_id) {
	case SBI_EXT_BASE_GET_SPEC_VERSION:
		out->value = (CPU_VCPU_SBI_VERSION_MAJOR <<
			      SBI_SPEC_VERSION_MAJOR_SHIFT) |
			     CPU_VCPU_SBI_VERSION_MINOR;
		break;
	case SBI_EXT_BASE_GET_IMP_ID:
		out->value = CPU_VCPU_SBI_IMPID;
		break;
	case SBI_EXT_BASE_GET_IMP_VERSION:
		out->value = VMM_VERSION_MAJOR << 24 |
			     VMM_VERSION_MINOR << 12 |
			     VMM_VERSION_RELEASE;
		break;
	case SBI_EXT_BASE_GET_MVENDORID:
	case SBI_EXT_BASE_GET_MARCHID:
	case SBI_EXT_BASE_GET_MIMPID:
		hret = sbi_ecall(SBI_EXT_BASE, func_id, 0, 0, 0, 0, 0, 0);
		ret = hret.error;
		out->value = hret.value;
		break;
	case SBI_EXT_BASE_PROBE_EXT:
		out->value = (cpu_vcpu_sbi_find_extension(vcpu, args[0])) ?
				1 : 0;
		break;
	default:
		ret = SBI_ERR_NOT_SUPPORTED;
	}

	return ret;
}

const struct cpu_vcpu_sbi_extension vcpu_sbi_base = {
	.name = "base",
	.extid_start = SBI_EXT_BASE,
	.extid_end = SBI_EXT_BASE,
	.handle = vcpu_sbi_base_ecall,
};
