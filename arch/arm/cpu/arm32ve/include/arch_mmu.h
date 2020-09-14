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

#define cpu_invalid_ipa_guest_tlb(ipa)			\
	do {						\
		inv_tlb_guest_allis();			\
		dsb(ish);				\
		isb();					\
	} while (0)

#define cpu_invalid_va_hypervisor_tlb(va)		\
	do {						\
		inv_tlb_hyp_mvais((va));		\
		dsb(ish);				\
		isb();					\
	} while (0)

#define cpu_invalid_all_tlbs()				\
	do {						\
		inv_utlb_all();				\
		dsb(ish);				\
		isb();					\
	} while (0)

#define cpu_stage2_ttbl_pa()				\
		(read_vttbr() & VTTBR_BADDR_MASK)
#define cpu_stage2_vmid()				\
		((read_vttbr() & VTTBR_VMID_MASK) >> VTTBR_VMID_SHIFT)
#define cpu_stage2_update(ttbl_pa, vmid)		\
	do {						\
		u64 vttbr = 0x0;			\
		vttbr |= ((u64)(vmid) << VTTBR_VMID_SHIFT) & VTTBR_VMID_MASK; \
		vttbr |= (ttbl_pa)  & VTTBR_BADDR_MASK;	\
		write_vttbr(vttbr);			\
	} while(0);

static inline void cpu_mmu_sync_tte(u64 *tte)
{
	dsb(ishst);
}

static inline void cpu_mmu_clean_invalidate(void *va)
{
	asm volatile("mcr     p15, 0, %0, c7, c14, 1\t\n"
		     "dsb\t\n"
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
	/* TODO: */
	tp->fault = FALSE;
	tp->fault_s2 = FALSE;
	tp->fault_translation = FALSE;
	tp->fault_access = FALSE;
	tp->fault_permission = FALSE;
	tp->fault_unknown = FALSE;
	tp->addr = 0x0;
}

#include <mmu_lpae.h>

#endif /* __ARCH_MMU_H__ */
