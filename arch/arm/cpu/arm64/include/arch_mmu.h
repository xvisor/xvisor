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

#include <mmu_lpae.h>

#endif /* __ARCH_MMU_H__ */
