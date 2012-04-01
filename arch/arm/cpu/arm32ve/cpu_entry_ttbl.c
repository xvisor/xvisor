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
extern u32 def_ttbl_use_count;

/* Note: This function must be called with MMU disabled from 
 * primary CPU only.
 * Note: This function cannot refer to any global variable &
 * functions to ensure that it can execute from anywhere.
 */
#define to_load_va(va)		((va) - exec_start + load_start)
void __attribute__ ((section (".entry"))) 
_setup_initial_ttbl(virtual_addr_t load_start,
		    virtual_addr_t load_end,
		    virtual_addr_t exec_start,
		    virtual_addr_t exec_end)
{
	u32 i, index, tmp, map_exec;
	u32 *ttbl_count;
	u64 *ttbl, *nttbl;
	physical_addr_t pa;

	ttbl = (u64 *)to_load_va((virtual_addr_t)&def_ttbl);
	nttbl = ttbl + TTBL_TABLE_ENTCNT;
	ttbl_count = (u32 *)to_load_va((virtual_addr_t)&def_ttbl_use_count);
	*ttbl_count = 0;

	for (i = 0; i < TTBL_TABLE_ENTCNT; i++) {
		ttbl[i] = 0x0ULL;
	}
	(*ttbl_count)++;

	map_exec = 0;
	tmp = load_start;
	while (1) {
		if (!map_exec && load_end <= tmp) {
			map_exec = 1;
			tmp = exec_start;
		} else if (map_exec && exec_end <= tmp) {
			break;
		}

		/* Setup level1 table */
		ttbl = (u64 *)to_load_va((virtual_addr_t)&def_ttbl);
		index = (tmp & TTBL_L1_INDEX_MASK) >> TTBL_L1_INDEX_SHIFT;
		if (ttbl[index] & TTBL_VALID_MASK) {
			ttbl = (u64 *)(u32)(ttbl[index] & TTBL_OUTADDR_MASK);
		} else {
			for (i = 0; i < TTBL_TABLE_ENTCNT; i++) {
				nttbl[i] = 0x0ULL;
			}
			ttbl[index] |= (((virtual_addr_t)nttbl) & TTBL_OUTADDR_MASK);
			ttbl[index] |= (TTBL_TABLE_MASK | TTBL_VALID_MASK);
			ttbl = nttbl;
			nttbl += TTBL_TABLE_ENTCNT;
			(*ttbl_count)++;
		}

		/* Setup level2 table */
		index = (tmp & TTBL_L2_INDEX_MASK) >> TTBL_L2_INDEX_SHIFT;
		if (ttbl[index] & TTBL_VALID_MASK) {
			ttbl = (u64 *)(u32)(ttbl[index] & TTBL_OUTADDR_MASK);
		} else {
			for (i = 0; i < TTBL_TABLE_ENTCNT; i++) {
				nttbl[i] = 0x0ULL;
			}
			ttbl[index] |= (((virtual_addr_t)nttbl) & TTBL_OUTADDR_MASK);
			ttbl[index] |= (TTBL_TABLE_MASK | TTBL_VALID_MASK);
			ttbl = nttbl;
			nttbl += TTBL_TABLE_ENTCNT;
			(*ttbl_count)++;
		}

		/* Setup level3 table */
		index = (tmp & TTBL_L3_INDEX_MASK) >> TTBL_L3_INDEX_SHIFT;
		if (!(ttbl[index] & TTBL_VALID_MASK)) {
			if (map_exec) {
				ttbl[index] |= (to_load_va(tmp) & TTBL_OUTADDR_MASK);
			} else {
				ttbl[index] |= (tmp & TTBL_OUTADDR_MASK);
			}
			ttbl[index] |= TTBL_STAGE1_LOWER_AF_MASK;
			ttbl[index] |= (TTBL_AP_SRW_U << TTBL_STAGE1_LOWER_AP_SHIFT);
			ttbl[index] |= (TTBL_TABLE_MASK | TTBL_VALID_MASK);
		}

		/* Point to next page */
		tmp += TTBL_L3_BLOCK_SIZE;
	}

	/* Setup Hypervisor Translation Control Register */
	tmp = read_htcr();
	tmp &= ~HTCR_T0SZ_MASK; /* Ensure T0SZ = 0 */
	write_htcr(tmp);

	/* Setup Hypervisor Translation Table Base Register */
	/* Note: if MMU is disabled then va = pa */
	pa = to_load_va((virtual_addr_t)&def_ttbl);;
	pa &= HTTBR_BADDR_MASK;
	write_httbr(pa);
}
