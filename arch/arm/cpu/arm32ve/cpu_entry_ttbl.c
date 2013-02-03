/**
 * Copyright (c) 2012 Anup Patel.
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
 * @file cpu_entry_ttbl.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Initial translation table setup at reset time
 */

#include <vmm_types.h>
#include <cpu_defines.h>
#include <cpu_inline_asm.h>

extern u8 def_ttbl[];
extern int def_ttbl_tree[];

/* Note: This function must be called with MMU disabled from 
 * primary CPU only.
 * Note: This function cannot refer to any global variable &
 * functions to ensure that it can execute from anywhere.
 */
#define to_load_pa(va)		((va) - exec_start + load_start)
void __attribute__ ((section (".entry"))) 
_setup_initial_ttbl(virtual_addr_t load_start,
		    virtual_addr_t load_end,
		    virtual_addr_t exec_start,
		    virtual_addr_t exec_end)
{
	int * ttbl_tree;
	u32 i, index, map_exec;
	u32 ttbl_count;
	u64 *ttbl, *nttbl;
	virtual_addr_t ttbl_base, page_addr;
	physical_addr_t pa;

	/* Initialize HMAIR0 and HMAIR1 for using caching 
	 * attributes via attrindex of each page
	 */
	write_hmair0(HMAIR0_INITVAL);
	write_hmair1(HMAIR0_INITVAL);

	ttbl = NULL;
	ttbl_base = to_load_pa((virtual_addr_t)&def_ttbl);
	nttbl = (u64 *)ttbl_base;
	ttbl_tree = (int *)to_load_pa((virtual_addr_t)&def_ttbl_tree);
	for (i = 0; i < TTBL_INITIAL_TABLE_COUNT; i++) {
		ttbl_tree[i] = -1;
	}
	ttbl_count = 0;

	/* Allocate level1 table */
	if (ttbl_count == TTBL_INITIAL_TABLE_COUNT) {
		while(1); /* No initial table available */
	}
	for (i = 0; i < TTBL_TABLE_ENTCNT; i++) {
		nttbl[i] = 0x0ULL;
	}
	ttbl_count++;
	ttbl = nttbl;
	nttbl += TTBL_TABLE_ENTCNT;

	map_exec = 0;
	page_addr = load_start;
	while (1) {
		if (!map_exec && load_end <= page_addr) {
			map_exec = 1;
			page_addr = exec_start;
		} else if (map_exec && exec_end <= page_addr) {
			break;
		}

		/* Setup level1 table */
		ttbl = (u64 *)to_load_pa((virtual_addr_t)&def_ttbl);
		index = (page_addr & TTBL_L1_INDEX_MASK) >> TTBL_L1_INDEX_SHIFT;
		if (ttbl[index] & TTBL_VALID_MASK) {
			/* Find level2 table */
			ttbl = (u64 *)(u32)(ttbl[index] & TTBL_OUTADDR_MASK);
		} else {
			/* Allocate new level2 table */
			if (ttbl_count == TTBL_INITIAL_TABLE_COUNT) {
				while(1); /* No initial table available */
			}
			for (i = 0; i < TTBL_TABLE_ENTCNT; i++) {
				nttbl[i] = 0x0ULL;
			}
			ttbl_tree[ttbl_count] = ((u32)ttbl - ttbl_base) >> 
							TTBL_TABLE_SIZE_SHIFT;
			ttbl_count++;
			ttbl[index] |= (((virtual_addr_t)nttbl) & TTBL_OUTADDR_MASK);
			ttbl[index] |= (TTBL_TABLE_MASK | TTBL_VALID_MASK);
			ttbl = nttbl;
			nttbl += TTBL_TABLE_ENTCNT;
		}

		/* Setup level2 table */
		index = (page_addr & TTBL_L2_INDEX_MASK) >> TTBL_L2_INDEX_SHIFT;
		if (ttbl[index] & TTBL_VALID_MASK) {
			/* Find level3 table */
			ttbl = (u64 *)(u32)(ttbl[index] & TTBL_OUTADDR_MASK);
		} else {
			/* Allocate new level3 table */
			if (ttbl_count == TTBL_INITIAL_TABLE_COUNT) {
				while(1); /* No initial table available */
			}
			for (i = 0; i < TTBL_TABLE_ENTCNT; i++) {
				nttbl[i] = 0x0ULL;
			}
			ttbl_tree[ttbl_count] = ((u32)ttbl - ttbl_base) >> 
							TTBL_TABLE_SIZE_SHIFT;
			ttbl_count++;
			ttbl[index] |= (((virtual_addr_t)nttbl) & TTBL_OUTADDR_MASK);
			ttbl[index] |= (TTBL_TABLE_MASK | TTBL_VALID_MASK);
			ttbl = nttbl;
			nttbl += TTBL_TABLE_ENTCNT;
		}

		/* Setup level3 table */
		index = (page_addr & TTBL_L3_INDEX_MASK) >> TTBL_L3_INDEX_SHIFT;
		if (!(ttbl[index] & TTBL_VALID_MASK)) {
			/* Update level3 table */
			if (map_exec) {
				ttbl[index] |= (to_load_pa(page_addr) & TTBL_OUTADDR_MASK);
			} else {
				ttbl[index] |= (page_addr & TTBL_OUTADDR_MASK);
			}
			ttbl[index] |= TTBL_STAGE1_LOWER_AF_MASK;
			ttbl[index] |= (TTBL_AP_SRW_U << TTBL_STAGE1_LOWER_AP_SHIFT);
			ttbl[index] |= (0x1 << TTBL_STAGE1_LOWER_NS_SHIFT) &
							TTBL_STAGE1_LOWER_NS_MASK;
			ttbl[index] |= (AINDEX_NORMAL_WB << TTBL_STAGE1_LOWER_AINDEX_SHIFT) & 
							TTBL_STAGE1_LOWER_AINDEX_MASK;
			ttbl[index] |= (TTBL_TABLE_MASK | TTBL_VALID_MASK);
		}

		/* Point to next page */
		page_addr += TTBL_L3_BLOCK_SIZE;
	}

	/* Setup Hypervisor Translation Control Register */
	i = read_htcr();
	i &= ~HTCR_T0SZ_MASK; /* Ensure T0SZ = 0 */
	i &= ~HTCR_ORGN0_MASK; /* Clear ORGN0 */
	i |= (0x3 << HTCR_ORGN0_SHIFT) & HTCR_ORGN0_MASK;
	i &= ~HTCR_IRGN0_MASK; /* Clear IRGN0 */
	i |= (0x3 << HTCR_IRGN0_SHIFT) & HTCR_IRGN0_MASK;
	write_htcr(i);

	/* Setup Hypervisor Translation Table Base Register */
	/* Note: if MMU is disabled then va = pa */
	pa = to_load_pa((virtual_addr_t)&def_ttbl);;
	pa &= HTTBR_BADDR_MASK;
	write_httbr(pa);

	/* Setup Hypervisor Virtual Translation Control Register */
	i = read_vtcr();
	i |= (0x1 << VTCR_SL0_SHIFT) & VTCR_SL0_MASK;
	i &= ~VTCR_ORGN0_MASK; /* Clear ORGN0 */
	i |= (0x3 << VTCR_ORGN0_SHIFT) & VTCR_ORGN0_MASK;
	i &= ~VTCR_IRGN0_MASK; /* Clear IRGN0 */
	i |= (0x3 << VTCR_IRGN0_SHIFT) & VTCR_IRGN0_MASK;
	write_vtcr(i);
}
