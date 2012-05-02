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
 * @file vmm_host_vapool.c
 * @author Anup patel (anup@brainfault.org)
 * @brief Source file for virtual address pool management.
 */

#include <vmm_error.h>
#include <vmm_math.h>
#include <vmm_list.h>
#include <vmm_bitmap.h>
#include <vmm_string.h>
#include <vmm_host_aspace.h>
#include <vmm_host_vapool.h>

struct vmm_host_vapool_ctrl {
	u32 *vapool_bmap;
	u32 vapool_bmap_sz;
	u32 vapool_bmap_free;
	u32 vapool_page_count;
	virtual_addr_t vapool_start;
	virtual_size_t vapool_size;
};

static struct vmm_host_vapool_ctrl vpctrl;

int vmm_host_vapool_alloc(virtual_addr_t * va, virtual_size_t sz, bool aligned)
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

	if (vpctrl.vapool_bmap_free < bcnt) {
		return VMM_EFAIL;
	}

	found = 0;
	if (aligned && (sz > VMM_PAGE_SIZE)) {
		bpos = vmm_umod32(vpctrl.vapool_start, sz);
		if (bpos) {
			bpos = VMM_ROUNDUP2_PAGE_SIZE(sz) >> VMM_PAGE_SHIFT;
		}
		binc = bcnt;
	} else {
		bpos = 0;
		binc = 1;
	}
	for ( ; bpos < (vpctrl.vapool_size >> VMM_PAGE_SHIFT); bpos += binc) {
		bfree = 0;
		for (i = bpos; i < (bpos + bcnt); i++) {
			if (bitmap_isset(vpctrl.vapool_bmap, i)) {
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

	*va = vpctrl.vapool_start + bpos * VMM_PAGE_SIZE;
	for (i = bpos; i < (bpos + bcnt); i++) {
		bitmap_setbit(vpctrl.vapool_bmap, i);
		vpctrl.vapool_bmap_free--;
	}

	return VMM_OK;
}

int vmm_host_vapool_reserve(virtual_addr_t va, virtual_size_t sz)
{
	u32 i, bcnt, bpos, bfree;

	if ((va < vpctrl.vapool_start) ||
	    ((vpctrl.vapool_start + vpctrl.vapool_size) <= va)) {
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

	if (vpctrl.vapool_bmap_free < bcnt) {
		return VMM_EFAIL;
	}

	bpos = (va - vpctrl.vapool_start) >> VMM_PAGE_SHIFT;
	bfree = 0;
	for (i = bpos; i < (bpos + bcnt); i++) {
		if (bitmap_isset(vpctrl.vapool_bmap, i)) {
			break;
		}
		bfree++;
	}

	if (bfree != bcnt) {
		return VMM_EFAIL;
	}

	for (i = bpos; i < (bpos + bcnt); i++) {
		bitmap_setbit(vpctrl.vapool_bmap, i);
		vpctrl.vapool_bmap_free--;
	}

	return VMM_OK;
}

int vmm_host_vapool_free(virtual_addr_t va, virtual_size_t sz)
{
	u32 i, bcnt, bpos;

	if (va < vpctrl.vapool_start ||
	    (vpctrl.vapool_start + vpctrl.vapool_size) <= va) {
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

	bpos = (va - vpctrl.vapool_start) >> VMM_PAGE_SHIFT;

	for (i = bpos; i < (bpos + bcnt); i++) {
		bitmap_clearbit(vpctrl.vapool_bmap, i);
		vpctrl.vapool_bmap_free++;
	}

	return VMM_OK;
}

virtual_addr_t vmm_host_vapool_base(void)
{
	return vpctrl.vapool_start;
}

bool vmm_host_vapool_page_isfree(virtual_addr_t va)
{
	u32 bpos;

	if (va < vpctrl.vapool_start ||
	    (vpctrl.vapool_start + vpctrl.vapool_size) <= va) {
		return TRUE;
	}

	bpos = (va - vpctrl.vapool_start) >> VMM_PAGE_SHIFT;

	if (bitmap_isset(vpctrl.vapool_bmap, bpos)) {
		return FALSE;
	}

	return TRUE;
}

u32 vmm_host_vapool_free_page_count(void)
{
	return vpctrl.vapool_bmap_free;
}

u32 vmm_host_vapool_total_page_count(void)
{
	return vpctrl.vapool_page_count;
}

virtual_size_t vmm_host_vapool_size(void)
{
	return vpctrl.vapool_size;
}

virtual_size_t vmm_host_vapool_estimate_hksize(virtual_size_t size)
{
	return bitmap_estimate_size(size >> VMM_PAGE_SHIFT);
}

int __init vmm_host_vapool_init(virtual_addr_t base,
				virtual_size_t size, 
				virtual_addr_t hkbase, 
			 	virtual_addr_t resv_va, 
				virtual_size_t resv_sz)
{
	int ite, last, max;

	if ((hkbase < base) || ((base + size) <= hkbase)) {
		return VMM_EFAIL;
	}
	if ((hkbase < resv_va) || ((resv_va + resv_sz) <= hkbase)) {
		return VMM_EFAIL;
	}

	vmm_memset(&vpctrl, 0, sizeof(vpctrl));

	vpctrl.vapool_start = base;
	vpctrl.vapool_size = size;
	vpctrl.vapool_start &= ~VMM_PAGE_MASK;
	vpctrl.vapool_size &= ~VMM_PAGE_MASK;
	vpctrl.vapool_page_count = vpctrl.vapool_size >> VMM_PAGE_SHIFT;
	vpctrl.vapool_bmap = (u32 *)hkbase;
	vpctrl.vapool_bmap_sz = bitmap_estimate_size(vpctrl.vapool_page_count);
	vpctrl.vapool_bmap_free = vpctrl.vapool_page_count;

	bitmap_clearall(vpctrl.vapool_bmap, vpctrl.vapool_page_count);

	max = ((vpctrl.vapool_start + vpctrl.vapool_size) >> VMM_PAGE_SHIFT);
	ite = ((resv_va - vpctrl.vapool_start) >> VMM_PAGE_SHIFT);
	last = ite + (resv_sz >> VMM_PAGE_SHIFT);
	for ( ; (ite < last) && (ite < max); ite++) {
		bitmap_setbit(vpctrl.vapool_bmap, ite);
		vpctrl.vapool_bmap_free--;
	}

	return VMM_OK;
}

