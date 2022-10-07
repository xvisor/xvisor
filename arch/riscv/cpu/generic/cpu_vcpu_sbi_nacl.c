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
 * @file cpu_vcpu_sbi_nacl.c
 * @author Anup Patel (apatel@ventanamicro.com)
 * @brief source of SBI nested accleration extension
 */

#include <vmm_error.h>
#include <vmm_macros.h>
#include <vmm_manager.h>
#include <vmm_scheduler.h>

#include <cpu_hwcap.h>
#include <cpu_vcpu_nested.h>
#include <cpu_vcpu_sbi.h>
#include <cpu_vcpu_trap.h>
#include <riscv_sbi.h>

static int vcpu_sbi_nacl_ecall(struct vmm_vcpu *vcpu, unsigned long ext_id,
			       unsigned long func_id, unsigned long *args,
			       struct cpu_vcpu_sbi_return *out)
{
	int ret = VMM_OK;
	arch_regs_t *regs = vmm_scheduler_irq_regs();

	switch (func_id) {
	case SBI_EXT_NACL_PROBE_FEATURE:
		switch (args[0]) {
		case SBI_NACL_FEAT_SYNC_CSR:
		case SBI_NACL_FEAT_SYNC_HFENCE:
		case SBI_NACL_FEAT_SYNC_SRET:
		case SBI_NACL_FEAT_AUTOSWAP_CSR:
			out->value = 1;
			break;
		default:
			out->value = 0;
			break;
		}
		break;
	case SBI_EXT_NACL_SET_SHMEM:
		if (args[0] != -1UL && args[1]) {
			ret = VMM_ERANGE;
			break;
		}
		ret = cpu_vcpu_nested_setup_shmem(vcpu, regs, args[0]);
		break;
	case SBI_EXT_NACL_SYNC_CSR:
		ret = cpu_vcpu_nested_sync_csr(vcpu, regs, args[0]);
		break;
	case SBI_EXT_NACL_SYNC_HFENCE:
		ret = cpu_vcpu_nested_sync_hfence(vcpu, regs, args[0]);
		break;
	case SBI_EXT_NACL_SYNC_SRET:
		cpu_vcpu_nested_prep_sret(vcpu, regs);
		/*
		 * Ignore the SRET return value because over here:
		 * 1) nested virt is OFF
		 * 2) hstatus.SPVP == 1
		 */
		cpu_vcpu_sret_insn(vcpu, regs, 0);
		out->regs_updated = TRUE;
		break;
	default:
		return SBI_ERR_NOT_SUPPORTED;
	}

	return cpu_vcpu_sbi_xlate_error(ret);
}

static unsigned long vcpu_sbi_nacl_probe(struct vmm_vcpu *vcpu)
{
	return riscv_isa_extension_available(riscv_priv(vcpu)->isa, h) ? 1 : 0;
}

const struct cpu_vcpu_sbi_extension vcpu_sbi_nacl = {
	.name = "nacl",
	.extid_start = SBI_EXT_NACL,
	.extid_end = SBI_EXT_NACL,
	.handle = vcpu_sbi_nacl_ecall,
	.probe = vcpu_sbi_nacl_probe,
};
