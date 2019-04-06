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

int cpu_vcpu_csr_read(struct vmm_vcpu *vcpu,
			unsigned long csr_num,
			unsigned long *csr_val)
{
	/*
	 * We don't have any CSRs to emulate because runtime
	 * M-mode firmware (i.e. OpenSBI) takes care of it
	 */
	vmm_printf("%s: vcpu=%s invalid csr_num=0x%lx\n",
		   __func__, (vcpu) ? vcpu->name : "(null)", csr_num);

	return VMM_ENOTSUPP;
}

int cpu_vcpu_csr_write(struct vmm_vcpu *vcpu,
			unsigned long csr_num,
			unsigned long csr_val)
{
	/*
	 * We don't have any CSRs to emulate because runtime
	 * M-mode firmware (i.e. OpenSBI) takes care of it
	 */
	vmm_printf("%s: vcpu=%s invalid csr_num=0x%lx\n",
		   __func__, (vcpu) ? vcpu->name : "(null)", csr_num);

	return VMM_ENOTSUPP;
}
