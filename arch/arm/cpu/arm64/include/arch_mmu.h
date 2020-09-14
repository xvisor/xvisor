/**
 * Copyright (c) 2020 Anup Patel.
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
 * @file arch_mmu.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Arch MMU interface header
 */

#ifndef __ARCH_MMU_H__
#define __ARCH_MMU_H__

#include <cpu_inline_asm.h>
#include <cpu_cache.h>
#include <arch_barrier.h>

#define cpu_invalid_ipa_guest_tlb(ipa)		inv_tlb_guest_allis()
#define cpu_invalid_va_hypervisor_tlb(va)	inv_tlb_hyp_vais((va))
#define cpu_invalid_all_tlbs()			inv_tlb_hyp_all()

#define cpu_stage2_ttbl_pa()						\
	(mrs(vttbr_el2) & VTTBR_BADDR_MASK)
#define cpu_stage2_vmid()						\
	((mrs(vttbr_el2) & VTTBR_VMID_MASK) >> VTTBR_VMID_SHIFT)
#define cpu_stage2_update(ttbl_pa, vmid)				\
	do {								\
	u64 vttbr = 0x0;						\
	vttbr |= ((u64)(vmid) << VTTBR_VMID_SHIFT) & VTTBR_VMID_MASK;	\
	vttbr |= (ttbl_pa)  & VTTBR_BADDR_MASK;				\
	msr(vttbr_el2, vttbr);						\
	} while(0);

static inline void cpu_mmu_sync_tte(u64 *tte)
{
	dsb(ishst);
}

static inline void cpu_mmu_clean_invalidate(void *va)
{
	asm volatile("dc civac, %0\t\n"
		     "dsb sy\t\n"
		     "isb\t\n"
		     : : "r" ((unsigned long)va));
}

static inline void cpu_mmu_invalidate_range(virtual_addr_t start,
					    virtual_addr_t size)
{
	invalidate_dcache_mva_range(start, start + size);
}

struct cpu_mmu_at_test_result {
	bool fault;
	bool fault_s2;
	bool fault_translation;
	bool fault_access;
	bool fault_permission;
	bool fault_unknown;
	physical_addr_t addr;
};

static inline void cpu_mmu_at_test_exec(physical_addr_t s2_tbl_pa,
					bool s1_avail,
					physical_addr_t s1_tbl_pa,
					virtual_addr_t addr, bool write,
					struct cpu_mmu_at_test_result *tp)
{
	u64 par;
	u64 t_vttbr_el2, t_hcr_el2;
	u64 t_par_el1, t_mair_el1, t_tcr_el1, t_ttbr0_el1, t_sctlr_el1;
	extern u64 __mair_set, __tcr_set, v8_crval[2];

	tp->fault = FALSE;
	tp->fault_s2 = FALSE;
	tp->fault_translation = FALSE;
	tp->fault_access = FALSE;
	tp->fault_permission = FALSE;
	tp->fault_unknown = FALSE;
	tp->addr = 0x0;

	t_mair_el1 = mrs(mair_el1);
	t_tcr_el1 = mrs(tcr_el1);
	t_ttbr0_el1 = mrs(ttbr0_el1);
	t_sctlr_el1 = mrs(sctlr_el1);
	t_vttbr_el2 = mrs(vttbr_el2);
	t_hcr_el2 = mrs(hcr_el2);
	t_par_el1 = mrs(par_el1);

	cpu_stage2_update(s2_tbl_pa, 0);
	if (s1_avail) {
		msr(mair_el1, __mair_set);
		msr(tcr_el1, (__tcr_set & (TCR_T0SZ_MASK | TCR_IRGN0_MASK |
					   TCR_ORGN0_MASK | TCR_SH0_MASK |
					   TCR_TG0_MASK)));
		msr(ttbr0_el1, s1_tbl_pa);
		msr(sctlr_el1, ((t_sctlr_el1 & ~v8_crval[0]) | v8_crval[1]));
	} else {
		msr(mair_el1, 0x0);
		msr(tcr_el1, 0x0);
		msr(ttbr0_el1, 0x0);
		msr(sctlr_el1, 0x0);
	}
	msr(hcr_el2, HCR_DEFAULT_BITS | HCR_RW_MASK);

	msr(par_el1, 0x0);
	if (write) {
		va2pa_at(VA2PA_STAGE12, VA2PA_EL1, VA2PA_WR, addr);
	} else {
		va2pa_at(VA2PA_STAGE12, VA2PA_EL1, VA2PA_RD, addr);
	}
	par = mrs(par_el1);

	tp->fault = (par & 0x01) ? TRUE : FALSE;
	if (tp->fault) {
		tp->fault_s2 = (par & 0x200) ? TRUE : FALSE;

		switch ((par >> 1) & 0x3f) {
		case 0b000100:
		case 0b000101:
		case 0b000110:
		case 0b000111:
			tp->fault_translation = TRUE;
			break;
		case 0b001001:
		case 0b001010:
		case 0b001011:
			tp->fault_access = TRUE;
			break;
		case 0b001101:
		case 0b001110:
		case 0b001111:
			tp->fault_permission = TRUE;
			break;
		default:
			tp->fault_unknown = TRUE;
			break;
		};
	} else {
		tp->addr = par & 0x000FFFFFFFFFF000ULL;
		tp->addr |= (addr & 0xFFFULL);
	}

	msr(par_el1, t_par_el1);
	msr(hcr_el2, t_hcr_el2);
	msr(mair_el1, t_mair_el1);
	msr(tcr_el1, t_tcr_el1);
	msr(ttbr0_el1, t_ttbr0_el1);
	msr(sctlr_el1, t_sctlr_el1);
	msr(vttbr_el2, t_vttbr_el2);

	/*
	 * We polluted TLB by running AT instructions so let's cleanup by
	 * invalidating all Guest and Host TLB entries.
	 */
	inv_tlb_hyp_all();
	inv_tlb_guest_allis();
}

#include <mmu_lpae.h>

#endif /* __ARCH_MMU_H__ */
