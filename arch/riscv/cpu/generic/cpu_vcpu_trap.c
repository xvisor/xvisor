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
 * @file cpu_vcpu_trap.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief source of VCPU trap handling
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_host_aspace.h>
#include <vmm_guest_aspace.h>
#include <libs/stringlib.h>
#include <cpu_mmu.h>
#include <cpu_vcpu_trap.h>

int cpu_vcpu_page_fault(struct vmm_vcpu *vcpu,
			arch_regs_t *regs,
			unsigned long cause,
			unsigned long fault_addr)
{
	int rc, rc1;
	u32 reg_flags = 0x0, pg_reg_flags = 0x0;
	struct cpu_page pg;
	physical_addr_t inaddr, outaddr;
	physical_size_t size, availsz;

	memset(&pg, 0, sizeof(pg));

	inaddr = fault_addr & PGTBL_L0_MAP_MASK;
	size = PGTBL_L0_BLOCK_SIZE;

	rc = vmm_guest_physical_map(vcpu->guest, inaddr, size,
				    &outaddr, &availsz, &reg_flags);
	if (rc) {
		vmm_printf("%s: guest_phys=0x%"PRIPADDR" size=0x%"PRIPSIZE
			   " map failed\n", __func__, inaddr, size);
		return rc;
	}

	if (availsz < PGTBL_L0_BLOCK_SIZE) {
		vmm_printf("%s: availsz=0x%"PRIPSIZE" insufficent for "
			   "guest_phys=0x%"PRIPADDR"\n",
			   __func__, availsz, inaddr);
		return VMM_ENOSPC;
	}

	pg.ia = inaddr;
	pg.sz = size;
	pg.oa = outaddr;
	pg_reg_flags = reg_flags;

	if (reg_flags & (VMM_REGION_ISRAM | VMM_REGION_ISROM)) {
		inaddr = fault_addr & PGTBL_L1_MAP_MASK;
		size = PGTBL_L1_BLOCK_SIZE;
		rc = vmm_guest_physical_map(vcpu->guest, inaddr, size,
				    &outaddr, &availsz, &reg_flags);
		if (!rc && (availsz >= PGTBL_L1_BLOCK_SIZE)) {
			pg.ia = inaddr;
			pg.sz = size;
			pg.oa = outaddr;
			pg_reg_flags = reg_flags;
		}
#ifdef CONFIG_64BIT
		inaddr = fault_addr & PGTBL_L2_MAP_MASK;
		size = PGTBL_L2_BLOCK_SIZE;
		rc = vmm_guest_physical_map(vcpu->guest, inaddr, size,
				    &outaddr, &availsz, &reg_flags);
		if (!rc && (availsz >= PGTBL_L2_BLOCK_SIZE)) {
			pg.ia = inaddr;
			pg.sz = size;
			pg.oa = outaddr;
			pg_reg_flags = reg_flags;
		}
#endif
	}

	pg.user = 1;
	if (pg_reg_flags & VMM_REGION_VIRTUAL) {
		pg.read = 0;
		pg.write = 0;
		pg.execute = 0;
	} else if (pg_reg_flags & VMM_REGION_READONLY) {
		pg.read = 1;
		pg.write = 0;
		pg.execute = 1;
	} else {
		pg.read = 1;
		pg.write = 1;
		pg.execute = 1;
	}
	pg.valid = 1;

	/* Try to map the page in Stage2 */
	rc = cpu_mmu_map_page(riscv_guest_priv(vcpu->guest)->pgtbl, &pg);
	if (rc) {
		/* On SMP Guest, two different VCPUs may try to map same
		 * Guest region in Stage2 at the same time. This may cause
		 * cpu_mmu_map_page() to fail for one of the Guest VCPUs.
		 *
		 * To take care of this situation, we recheck Stage2 mapping
		 * when cpu_mmu_map_page() fails.
		 */
		memset(&pg, 0, sizeof(pg));
		rc1 = cpu_mmu_get_page(riscv_guest_priv(vcpu->guest)->pgtbl,
				       fault_addr, &pg);
		if (rc1) {
			return rc1;
		}
		rc = VMM_OK;
	}

	return rc;
}

int cpu_vcpu_access_fault(struct vmm_vcpu *vcpu,
			  arch_regs_t *regs,
			  unsigned long cause,
			  unsigned long fault_addr)
{
	/* TODO: */
	return VMM_EFAIL;
}
