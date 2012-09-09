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
#include <vmm_spinlocks.h>
#include <vmm_host_aspace.h>
#include <vmm_host_vapool.h>
#include <stringlib.h>
#include <mathlib.h>
#include <bitmap.h>

struct vmm_host_vapool_ctrl {
	vmm_spinlock_t vapool_bmap_lock;
	unsigned long *vapool_bmap;
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
	irq_flags_t flags;

	bcnt = VMM_SIZE_TO_PAGE(sz);

	vmm_spin_lock_irqsave(&vpctrl.vapool_bmap_lock, flags);

	if (vpctrl.vapool_bmap_free < bcnt) {
		vmm_spin_unlock_irqrestore(&vpctrl.vapool_bmap_lock, flags);
		return VMM_EFAIL;
	}

	found = 0;
	if (aligned && (sz > VMM_PAGE_SIZE)) {
		bpos = umod32(vpctrl.vapool_start, sz);
		if (bpos) {
			bpos = VMM_SIZE_TO_PAGE(sz);
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
		vmm_spin_unlock_irqrestore(&vpctrl.vapool_bmap_lock, flags);
		return VMM_EFAIL;
	}

	*va = vpctrl.vapool_start + bpos * VMM_PAGE_SIZE;
	bitmap_set(vpctrl.vapool_bmap, bpos, bcnt);
	vpctrl.vapool_bmap_free -= bcnt;

	vmm_spin_unlock_irqrestore(&vpctrl.vapool_bmap_lock, flags);

	return VMM_OK;
}

int vmm_host_vapool_reserve(virtual_addr_t va, virtual_size_t sz)
{
	u32 i, bcnt, bpos, bfree;
	irq_flags_t flags;

	if ((va < vpctrl.vapool_start) ||
	    ((vpctrl.vapool_start + vpctrl.vapool_size) <= va)) {
		return VMM_EFAIL;
	}

	bcnt = VMM_SIZE_TO_PAGE(sz);

	vmm_spin_lock_irqsave(&vpctrl.vapool_bmap_lock, flags);

	if (vpctrl.vapool_bmap_free < bcnt) {
		vmm_spin_unlock_irqrestore(&vpctrl.vapool_bmap_lock, flags);
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
		vmm_spin_unlock_irqrestore(&vpctrl.vapool_bmap_lock, flags);
		return VMM_EFAIL;
	}

	bitmap_set(vpctrl.vapool_bmap, bpos, bcnt);
	vpctrl.vapool_bmap_free -= bcnt;

	vmm_spin_unlock_irqrestore(&vpctrl.vapool_bmap_lock, flags);

	return VMM_OK;
}

int vmm_host_vapool_free(virtual_addr_t va, virtual_size_t sz)
{
	u32 bcnt, bpos;
	irq_flags_t flags;

	if (va < vpctrl.vapool_start ||
	    (vpctrl.vapool_start + vpctrl.vapool_size) <= va) {
		return VMM_EFAIL;
	}

	bcnt = VMM_SIZE_TO_PAGE(sz);
	bpos = (va - vpctrl.vapool_start) >> VMM_PAGE_SHIFT;

	vmm_spin_lock_irqsave(&vpctrl.vapool_bmap_lock, flags);

	bitmap_clear(vpctrl.vapool_bmap, bpos, bcnt);
	vpctrl.vapool_bmap_free += bcnt;

	vmm_spin_unlock_irqrestore(&vpctrl.vapool_bmap_lock, flags);

	return VMM_OK;
}

virtual_addr_t vmm_host_vapool_base(void)
{
	return vpctrl.vapool_start;
}

bool vmm_host_vapool_page_isfree(virtual_addr_t va)
{
	u32 bpos;
	bool ret = TRUE;
	irq_flags_t flags;

	if (va < vpctrl.vapool_start ||
	    (vpctrl.vapool_start + vpctrl.vapool_size) <= va) {
		return ret;
	}

	bpos = (va - vpctrl.vapool_start) >> VMM_PAGE_SHIFT;

	vmm_spin_lock_irqsave(&vpctrl.vapool_bmap_lock, flags);

	if (bitmap_isset(vpctrl.vapool_bmap, bpos)) {
		ret = FALSE;
	}

	vmm_spin_unlock_irqrestore(&vpctrl.vapool_bmap_lock, flags);

	return ret;
}

u32 vmm_host_vapool_free_page_count(void)
{
	u32 ret;
	irq_flags_t flags;

	vmm_spin_lock_irqsave(&vpctrl.vapool_bmap_lock, flags);
	ret = vpctrl.vapool_bmap_free;
	vmm_spin_unlock_irqrestore(&vpctrl.vapool_bmap_lock, flags);

	return ret;
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
	int start, last, max;

	if ((hkbase < base) || ((base + size) <= hkbase)) {
		return VMM_EFAIL;
	}
	if ((hkbase < resv_va) || ((resv_va + resv_sz) <= hkbase)) {
		return VMM_EFAIL;
	}

	memset(&vpctrl, 0, sizeof(vpctrl));

	INIT_SPIN_LOCK(&vpctrl.vapool_bmap_lock);

	vpctrl.vapool_start = base;
	vpctrl.vapool_size = size;
	vpctrl.vapool_start &= ~VMM_PAGE_MASK;
	vpctrl.vapool_size &= ~VMM_PAGE_MASK;
	vpctrl.vapool_page_count = vpctrl.vapool_size >> VMM_PAGE_SHIFT;
	vpctrl.vapool_bmap = (unsigned long *)hkbase;
	vpctrl.vapool_bmap_sz = bitmap_estimate_size(vpctrl.vapool_page_count);
	vpctrl.vapool_bmap_free = vpctrl.vapool_page_count;

	bitmap_zero(vpctrl.vapool_bmap, vpctrl.vapool_page_count);

	max = ((vpctrl.vapool_start + vpctrl.vapool_size) >> VMM_PAGE_SHIFT);
	start = ((resv_va - vpctrl.vapool_start) >> VMM_PAGE_SHIFT);
	last = start + (resv_sz >> VMM_PAGE_SHIFT);
	last = (last < max) ? last : max;
	bitmap_set(vpctrl.vapool_bmap, start, last - start);
	vpctrl.vapool_bmap_free -= last - start;

	return VMM_OK;
}

