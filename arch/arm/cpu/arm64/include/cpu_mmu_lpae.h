/**
 * Copyright (c) 2013 Sukanto Ghosh.
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
 * @file cpu_mmu_lpae.h
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief MMU interface of LPAE enabled ARM processor
 */
#ifndef _CPU_MMU_LPAE_H__
#define _CPU_MMU_LPAE_H__

#include <cpu_inline_asm.h>
#include <arch_barrier.h>

#define TTBL_FIRST_LEVEL		1
#define TTBL_LAST_LEVEL			3

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
	dsb();
	isb();
}

static inline void cpu_mmu_clean_invalidate(void *va)
{
	asm volatile("dc civac, %0\t\n"
		     "dsb sy\t\n"
		     "isb\t\n"
		     : : "r" ((unsigned long)va));
}

#endif /* _CPU_MMU_LPAE_H */
