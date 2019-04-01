/**
 * Copyright (c) 2019 Anup Patel.
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
 * @file cpu_vcpu_csr.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief source for VCPU CSR read/write handling
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <cpu_vcpu_csr.h>

#include <riscv_csr.h>
#include <riscv_timex.h>

int cpu_vcpu_csr_read(struct vmm_vcpu *vcpu,
			unsigned long csr_num,
			unsigned long *csr_val)
{
	ulong cen = -1UL;
	struct riscv_guest_priv *priv = riscv_guest_priv(vcpu->guest);

	/* TODO: Emulate CSR_SCOUNTEREN */

	switch (csr_num) {
	case CSR_CYCLE:
		if (!((cen >> (CSR_CYCLE - CSR_CYCLE)) & 1))
			return -1;
		*csr_val = csr_read(CSR_CYCLE);
		break;
	case CSR_TIME:
		if (!((cen >> (CSR_TIME - CSR_CYCLE)) & 1))
			return -1;
		*csr_val = get_cycles64() - priv->time_offset;
		break;
	case CSR_INSTRET:
		if (!((cen >> (CSR_INSTRET - CSR_CYCLE)) & 1))
			return -1;
		*csr_val = csr_read(CSR_INSTRET);
		break;
#if !defined(CONFIG_64BIT)
	case CSR_CYCLEH:
		if (!((cen >> (CSR_CYCLE - CSR_CYCLE)) & 1))
			return -1;
		*csr_val = csr_read(CSR_CYCLEH);
		break;
	case CSR_TIMEH:
		if (!((cen >> (CSR_TIME - CSR_CYCLE)) & 1))
			return -1;
		*csr_val = (get_cycles64() - priv->time_offset) >> 32;
		break;
	case CSR_INSTRETH:
		if (!((cen >> (CSR_INSTRET - CSR_CYCLE)) & 1))
			return -1;
		*csr_val = csr_read(CSR_INSTRETH);
		break;
#endif
	default:
		vmm_printf("%s: vcpu=%s invalid csr_num=0x%lx\n",
			   __func__, (vcpu) ? vcpu->name : "(null)", csr_num);
		return VMM_ENOTSUPP;
	};

	return VMM_OK;
}

int cpu_vcpu_csr_write(struct vmm_vcpu *vcpu,
			unsigned long csr_num,
			unsigned long csr_val)
{
	/* TODO: Emulate CSR_SCOUNTEREN */

	switch (csr_num) {
	case CSR_CYCLE:
		csr_write(CSR_CYCLE, csr_val);
		break;
	case CSR_INSTRET:
		csr_write(CSR_INSTRET, csr_val);
		break;
#if !defined(CONFIG_64BIT)
	case CSR_CYCLEH:
		csr_write(CSR_CYCLEH, csr_val);
		break;
	case CSR_INSTRETH:
		csr_write(CSR_INSTRETH, csr_val);
		break;
#endif
	default:
		vmm_printf("%s: vcpu=%s invalid csr_num=0x%lx\n",
			   __func__, (vcpu) ? vcpu->name : "(null)", csr_num);
		return VMM_ENOTSUPP;
	};

	return VMM_OK;
}
