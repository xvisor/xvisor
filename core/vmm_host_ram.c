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
 * @file vmm_host_ram.c
 * @author Anup patel (anup@brainfault.org)
 * @brief Source file for RAM management.
 */

#include <arch_cpu.h>
#include <arch_board.h>
#include <vmm_error.h>
#include <vmm_math.h>
#include <vmm_list.h>
#include <vmm_string.h>
#include <vmm_stdio.h>
#include <vmm_host_aspace.h>

struct vmm_host_ram_ctrl {
	u32 *ram_bmap;
	u32 ram_bmap_len;
	u32 ram_bmap_free;
	physical_addr_t ram_start;
	physical_size_t ram_size;
};

static struct vmm_host_ram_ctrl rctrl;

int vmm_host_ram_alloc(physical_addr_t * pa, physical_size_t sz, bool aligned)
{
	u32 i, found, binc, bcnt, bpos, bfree;

	bcnt = 0;
	while (sz > 0) {
		bcnt++;
		if (sz > VMM_PAGE_SIZE) {
			sz -= VMM_PAGE_SIZE;
		} else {
			sz = 0;
		}
	}

	if (rctrl.ram_bmap_free < bcnt) {
		return VMM_EFAIL;
	}

	found = 0;
	if (aligned && (sz > VMM_PAGE_SIZE)) {
		bpos = vmm_umod32(rctrl.ram_start, sz);
		if (bpos) {
			bpos = VMM_ROUNDUP2_PAGE_SIZE(sz) >> VMM_PAGE_SHIFT;
		}
		binc = bcnt;
	} else {
		bpos = 0;
		binc = 1;
	}
	for ( ; bpos < (rctrl.ram_size >> VMM_PAGE_SHIFT); bpos += binc) {
		bfree = 0;
		for (i = bpos; i < (bpos + bcnt); i++) {
			if (rctrl.ram_bmap[i >> 5] & 
			    (0x1 << (31 - (i & 0x1F)))) {
				break;
			}
			bfree++;
		}
		if (bfree == bcnt) {
			found = 1;
			break;
		}
	}
	if (!found) {
		return VMM_EFAIL;
	}

	*pa = rctrl.ram_start + bpos * VMM_PAGE_SIZE;
	for (i = bpos; i < (bpos + bcnt); i++) {
		rctrl.ram_bmap[i >> 5] |= (0x1 << (31 - (i & 0x1F)));
		rctrl.ram_bmap_free--;
	}

	return VMM_OK;
}

int vmm_host_ram_reserve(physical_addr_t pa, physical_size_t sz)
{
	u32 i, bcnt, bpos, bfree;

	if ((pa < rctrl.ram_start) ||
	    ((rctrl.ram_start + rctrl.ram_size) <= pa)) {
		return VMM_EFAIL;
	}

	bcnt = 0;
	while (sz > 0) {
		bcnt++;
		if (sz > VMM_PAGE_SIZE) {
			sz -= VMM_PAGE_SIZE;
		} else {
			sz = 0;
		}
	}

	if (rctrl.ram_bmap_free < bcnt) {
		return VMM_EFAIL;
	}

	bpos = (pa - rctrl.ram_start) >> VMM_PAGE_SHIFT;
	bfree = 0;
	for (i = bpos; i < (bpos + bcnt); i++) {
		if (rctrl.ram_bmap[i >> 5] & 
		    (0x1 << (31 - (i & 0x1F)))) {
			break;
		}
		bfree++;
	}

	if (bfree != bcnt) {
		return VMM_EFAIL;
	}

	for (i = bpos; i < (bpos + bcnt); i++) {
		rctrl.ram_bmap[i >> 5] |= (0x1 << (31 - (i & 0x1F)));
		rctrl.ram_bmap_free--;
	}

	return VMM_OK;
}

int vmm_host_ram_free(physical_addr_t pa, physical_size_t sz)
{
	u32 i, bcnt, bpos;

	if (pa < rctrl.ram_start ||
	    (rctrl.ram_start + rctrl.ram_size) <= pa) {
		return VMM_EFAIL;
	}

	bcnt = 0;
	while (sz > 0) {
		bcnt++;
		if (sz > VMM_PAGE_SIZE) {
			sz -= VMM_PAGE_SIZE;
		} else {
			sz = 0;
		}
	}

	bpos = (pa - rctrl.ram_start) >> VMM_PAGE_SHIFT;

	for (i = bpos; i < (bpos + bcnt); i++) {
		rctrl.ram_bmap[i >> 5] &= ~(0x1 << (31 - (i & 0x1F)));
		rctrl.ram_bmap_free++;
	}

	return VMM_OK;
}

physical_addr_t vmm_host_ram_base(void)
{
	return rctrl.ram_start;
}

bool vmm_host_ram_frame_isfree(physical_addr_t pa)
{
	u32 bpos;

	if (pa < rctrl.ram_start ||
	    (rctrl.ram_start + rctrl.ram_size) <= pa) {
		return TRUE;
	}

	bpos = (pa - rctrl.ram_start) >> VMM_PAGE_SHIFT;

	if (rctrl.ram_bmap[bpos >> 5] & (0x1 << (31 - (bpos & 0x1F)))) {
		return FALSE;
	}

	return TRUE;
}

u32 vmm_host_ram_free_frame_count(void)
{
	return rctrl.ram_bmap_free;
}

u32 vmm_host_ram_total_frame_count(void)
{
	return rctrl.ram_size >> VMM_PAGE_SHIFT;
}

physical_size_t vmm_host_ram_size(void)
{
	return rctrl.ram_size;
}

virtual_size_t vmm_host_ram_estimate_hksize(physical_size_t ram_size)
{
	return ((ram_size >> (VMM_PAGE_SHIFT + 5)) + 1) * sizeof(u32);
}

int __init vmm_host_ram_init(physical_addr_t base, 
			     physical_size_t size,
			     virtual_addr_t hkbase, 
			     physical_addr_t resv_pa, 
			     virtual_size_t resv_sz)
{
	int ite, last, max;

	vmm_memset(&rctrl, 0, sizeof(rctrl));

	rctrl.ram_start = base;
	rctrl.ram_size = size;
	rctrl.ram_start &= ~VMM_PAGE_MASK;
	rctrl.ram_size &= ~VMM_PAGE_MASK;
	rctrl.ram_bmap = (u32 *)hkbase;
	rctrl.ram_bmap_len = rctrl.ram_size >> (VMM_PAGE_SHIFT + 5);
	rctrl.ram_bmap_len += 1;
	rctrl.ram_bmap_free = rctrl.ram_size >> VMM_PAGE_SHIFT;

	vmm_memset(rctrl.ram_bmap, 0, sizeof(u32) * rctrl.ram_bmap_len);

	max = ((rctrl.ram_start + rctrl.ram_size) >> VMM_PAGE_SHIFT);
	ite = ((resv_pa - rctrl.ram_start) >> VMM_PAGE_SHIFT);
	last = ite + (resv_sz >> VMM_PAGE_SHIFT);
	for ( ; (ite < last) && (ite < max); ite++) {
		rctrl.ram_bmap[ite >> 5] |= (0x1 << (31 - (ite & 0x1F)));
		rctrl.ram_bmap_free--;
	}

	return VMM_OK;
}

