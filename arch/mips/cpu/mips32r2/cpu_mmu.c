/**
 * Copyright (c) 2010-2011 Himanshu Chauhan.
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
 * @file cpu_mmu.c
 * @version 0.01
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief MMU handling functions.
 */

#include <arch_regs.h>
#include <vmm_error.h>
#include <vmm_types.h>
#include <vmm_stdio.h>
#include <vmm_scheduler.h>
#include <cpu_mmu.h>
#include <cpu_asm_macros.h>
#include <cpu_vcpu_mmu.h>

void set_current_asid(u32 cur_asid)
{
	mips32_entryhi_t ehi;
	ehi._entryhi = read_c0_entryhi();
	ehi._s_entryhi.asid = cur_asid;
	write_c0_entryhi(ehi._entryhi);
}

u32 do_tlbmiss(arch_regs_t *uregs)
{
	mips32_entryhi_t ehi;
	ehi._entryhi = read_c0_entryhi();
	virtual_addr_t badvaddr;
	pte_t *fpte;
	struct mips32_tlb_entry tlb_entry;

	/* FIXME: Hardcoded 1 is the ASID of VMM. Move it to a macro */
	if (!is_vmm_asid(ehi._s_entryhi.asid)) {
		return do_vcpu_tlbmiss(uregs);
	} else {
		badvaddr = read_c0_badvaddr();
		fpte = cpu_va2pte(badvaddr);

		if (fpte == NULL)
			vmm_panic("ARGHHH!!! Cannot handle page fault in VMM!\n");

		tlb_entry.entryhi._s_entryhi.asid = (0x01UL << 6);
		tlb_entry.entryhi._s_entryhi.reserved = 0;
		tlb_entry.entryhi._s_entryhi.vpn2 = (fpte->vaddr >> VPN2_SHIFT);
		tlb_entry.entryhi._s_entryhi.vpn2x = 0;
		tlb_entry.page_mask = PAGE_MASK;

		/* TLB Low entry. Mapping two physical addresses */
		/* FIXME: Take the flag settings from mem_flags. */
		tlb_entry.entrylo0._s_entrylo.global = 0; /* not global */
		tlb_entry.entrylo0._s_entrylo.valid = 1; /* valid */
		tlb_entry.entrylo0._s_entrylo.dirty = 1; /* writeable */
		tlb_entry.entrylo0._s_entrylo.cacheable = 0; /* Dev map, no cache */
		tlb_entry.entrylo0._s_entrylo.pfn = (fpte->paddr >> PAGE_SHIFT);

		/* We'll map to consecutive physical addresses */
		/* Needed? */
		fpte->paddr += PAGE_SIZE;
		tlb_entry.entrylo1._s_entrylo.global = 0; /* not global */
		tlb_entry.entrylo1._s_entrylo.valid = 0; /* valid */
		tlb_entry.entrylo1._s_entrylo.dirty = 1; /* writeable */
		tlb_entry.entrylo1._s_entrylo.cacheable = 0; /* Dev map, no cache */
		tlb_entry.entrylo1._s_entrylo.pfn = (fpte->paddr >> PAGE_SHIFT);

		mips_fill_tlb_entry(&tlb_entry, -1);

	}

	return 0;
}
