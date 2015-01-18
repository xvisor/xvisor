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

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_spinlocks.h>
#include <vmm_resource.h>
#include <vmm_host_aspace.h>
#include <vmm_host_ram.h>
#include <libs/stringlib.h>
#include <libs/mathlib.h>
#include <libs/bitmap.h>

struct vmm_host_ram_ctrl {
	vmm_spinlock_t ram_bmap_lock;
	unsigned long *ram_bmap;
	u32 ram_bmap_sz;
	u32 ram_bmap_free;
	u32 ram_frame_count;
	physical_addr_t ram_start;
	physical_size_t ram_size;
	struct vmm_resource ram_res;
};

static struct vmm_host_ram_ctrl rctrl;

physical_size_t vmm_host_ram_alloc(physical_addr_t *pa,
				   physical_size_t sz,
				   u32 align_order)
{
	irq_flags_t flags;
	u32 i, found, binc, bcnt, bpos, bfree;

	if ((sz == 0) ||
	    (align_order < VMM_PAGE_SHIFT) ||
	    (BITS_PER_LONG <= align_order)) {
		return 0;
	}

	sz = roundup2_order_size(sz, align_order);
	bcnt = VMM_SIZE_TO_PAGE(sz);

	vmm_spin_lock_irqsave_lite(&rctrl.ram_bmap_lock, flags);

	if (rctrl.ram_bmap_free < bcnt) {
		vmm_spin_unlock_irqrestore_lite(&rctrl.ram_bmap_lock, flags);
		return 0;
	}

	found = 0;
	binc = order_size(align_order) >> VMM_PAGE_SHIFT;
	bpos = rctrl.ram_start & order_mask(align_order);
	if (bpos) {
		bpos = VMM_SIZE_TO_PAGE(order_size(align_order) - bpos);
	}
	for (; bpos < (rctrl.ram_size >> VMM_PAGE_SHIFT); bpos += binc) {
		bfree = 0;
		for (i = bpos; i < (bpos + bcnt); i++) {
			if (bitmap_isset(rctrl.ram_bmap, i)) {
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
		vmm_spin_unlock_irqrestore_lite(&rctrl.ram_bmap_lock, flags);
		return 0;
	}

	*pa = rctrl.ram_start + bpos * VMM_PAGE_SIZE;
	bitmap_set(rctrl.ram_bmap, bpos, bcnt);
	rctrl.ram_bmap_free -= bcnt;

	vmm_spin_unlock_irqrestore_lite(&rctrl.ram_bmap_lock, flags);

	return sz;
}

int vmm_host_ram_reserve(physical_addr_t pa, physical_size_t sz)
{
	u32 i, bcnt, bpos, bfree;
	irq_flags_t flags;

	if ((pa < rctrl.ram_start) ||
	    ((rctrl.ram_start + rctrl.ram_size) < (pa + sz))) {
		return VMM_EFAIL;
	}

	bcnt = VMM_SIZE_TO_PAGE(sz);

	vmm_spin_lock_irqsave_lite(&rctrl.ram_bmap_lock, flags);

	if (rctrl.ram_bmap_free < bcnt) {
		vmm_spin_unlock_irqrestore_lite(&rctrl.ram_bmap_lock, flags);
		return VMM_EFAIL;
	}

	bpos = (pa - rctrl.ram_start) >> VMM_PAGE_SHIFT;
	bfree = 0;
	for (i = bpos; i < (bpos + bcnt); i++) {
		if (bitmap_isset(rctrl.ram_bmap, i)) {
			break;
		}
		bfree++;
	}

	if (bfree != bcnt) {
		vmm_spin_unlock_irqrestore_lite(&rctrl.ram_bmap_lock, flags);
		return VMM_EFAIL;
	}

	bitmap_set(rctrl.ram_bmap, bpos, bcnt);
	rctrl.ram_bmap_free -= bcnt;

	vmm_spin_unlock_irqrestore_lite(&rctrl.ram_bmap_lock, flags);

	return VMM_OK;
}

int vmm_host_ram_free(physical_addr_t pa, physical_size_t sz)
{
	u32 bcnt, bpos;
	irq_flags_t flags;

	if ((pa < rctrl.ram_start) ||
	    ((rctrl.ram_start + rctrl.ram_size) < (pa + sz))) {
		return VMM_EFAIL;
	}

	bcnt = VMM_SIZE_TO_PAGE(sz);
	bpos = (pa - rctrl.ram_start) >> VMM_PAGE_SHIFT;

	vmm_spin_lock_irqsave_lite(&rctrl.ram_bmap_lock, flags);

	bitmap_clear(rctrl.ram_bmap, bpos, bcnt);
	rctrl.ram_bmap_free += bcnt;

	vmm_spin_unlock_irqrestore_lite(&rctrl.ram_bmap_lock, flags);

	return VMM_OK;
}

physical_addr_t vmm_host_ram_base(void)
{
	return rctrl.ram_start;
}

bool vmm_host_ram_frame_isfree(physical_addr_t pa)
{
	u32 bpos;
	bool ret = FALSE;
	irq_flags_t flags;

	if ((pa < rctrl.ram_start) ||
	    ((rctrl.ram_start + rctrl.ram_size) <= pa)) {
		return ret;
	}

	bpos = (pa - rctrl.ram_start) >> VMM_PAGE_SHIFT;

	vmm_spin_lock_irqsave_lite(&rctrl.ram_bmap_lock, flags);

	if (!bitmap_isset(rctrl.ram_bmap, bpos)) {
		ret = TRUE;
	}

	vmm_spin_unlock_irqrestore_lite(&rctrl.ram_bmap_lock, flags);

	return ret;
}

u32 vmm_host_ram_free_frame_count(void)
{
	u32 ret;
	irq_flags_t flags;

	vmm_spin_lock_irqsave_lite(&rctrl.ram_bmap_lock, flags);
	ret = rctrl.ram_bmap_free;
	vmm_spin_unlock_irqrestore_lite(&rctrl.ram_bmap_lock, flags);

	return ret;
}

u32 vmm_host_ram_total_frame_count(void)
{
	return rctrl.ram_frame_count;
}

physical_size_t vmm_host_ram_size(void)
{
	return rctrl.ram_size;
}

virtual_size_t vmm_host_ram_estimate_hksize(physical_size_t ram_size)
{
	return bitmap_estimate_size(ram_size >> VMM_PAGE_SHIFT);
}

int __init vmm_host_ram_init(physical_addr_t base, 
			     physical_size_t size,
			     virtual_addr_t hkbase)
{
	int rc;

	memset(&rctrl, 0, sizeof(rctrl));

	INIT_SPIN_LOCK(&rctrl.ram_bmap_lock);

	rctrl.ram_start = base;
	rctrl.ram_size = size;
	rctrl.ram_start &= ~VMM_PAGE_MASK;
	rctrl.ram_size &= ~VMM_PAGE_MASK;
	rctrl.ram_frame_count = rctrl.ram_size >> VMM_PAGE_SHIFT;
	rctrl.ram_bmap = (unsigned long *)hkbase;
	rctrl.ram_bmap_sz = bitmap_estimate_size(rctrl.ram_frame_count);
	rctrl.ram_bmap_free = rctrl.ram_frame_count;

	bitmap_zero(rctrl.ram_bmap, rctrl.ram_frame_count);

	rctrl.ram_res.start = base;
	rctrl.ram_res.end = base + size - 1;
	rctrl.ram_res.name = "System RAM";
	rctrl.ram_res.flags = 0;
	rc = vmm_request_resource(&vmm_hostmem_resource, &rctrl.ram_res);
	if (rc) {
		return rc;
	}

	return VMM_OK;
}

