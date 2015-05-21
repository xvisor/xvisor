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
#include <arch_devtree.h>
#include <libs/stringlib.h>
#include <libs/mathlib.h>
#include <libs/bitmap.h>

struct vmm_host_ram_bank {
	physical_addr_t start;
	physical_size_t size;
	u32 frame_count;

	vmm_spinlock_t bmap_lock;
	unsigned long *bmap;
	u32 bmap_sz;
	u32 bmap_free;

	struct vmm_resource res;
};

struct vmm_host_ram_ctrl {
	u32 bank_count;
	struct vmm_host_ram_bank banks[CONFIG_MAX_RAM_BANK_COUNT];
};

static struct vmm_host_ram_ctrl rctrl;

physical_size_t vmm_host_ram_alloc(physical_addr_t *pa,
				   physical_size_t sz,
				   u32 align_order)
{
	irq_flags_t flags;
	u32 i, found, bn, binc, bcnt, bpos, bfree;
	struct vmm_host_ram_bank *bank;

	if ((sz == 0) ||
	    (align_order < VMM_PAGE_SHIFT) ||
	    (BITS_PER_LONG <= align_order)) {
		return 0;
	}

	sz = roundup2_order_size(sz, align_order);
	bcnt = VMM_SIZE_TO_PAGE(sz);

	for (bn = 0; bn < rctrl.bank_count; bn++) {
		bank = &rctrl.banks[bn];

		vmm_spin_lock_irqsave_lite(&bank->bmap_lock, flags);

		if (bank->bmap_free < bcnt) {
			vmm_spin_unlock_irqrestore_lite(&bank->bmap_lock, flags);
			continue;
		}

		found = 0;
		binc = order_size(align_order) >> VMM_PAGE_SHIFT;
		bpos = bank->start & order_mask(align_order);
		if (bpos) {
			bpos = VMM_SIZE_TO_PAGE(order_size(align_order) - bpos);
		}
		for (; bpos < (bank->size >> VMM_PAGE_SHIFT); bpos += binc) {
			bfree = 0;
			for (i = bpos; i < (bpos + bcnt); i++) {
				if (bitmap_isset(bank->bmap, i)) {
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
			vmm_spin_unlock_irqrestore_lite(&bank->bmap_lock, flags);
			continue;
		}

		*pa = bank->start + bpos * VMM_PAGE_SIZE;
		bitmap_set(bank->bmap, bpos, bcnt);
		bank->bmap_free -= bcnt;

		vmm_spin_unlock_irqrestore_lite(&bank->bmap_lock, flags);

		return sz;
	}

	return 0;
}

int vmm_host_ram_reserve(physical_addr_t pa, physical_size_t sz)
{
	int rc = VMM_EINVALID;
	u32 i, bn, bcnt, bpos, bfree;
	irq_flags_t flags;
	struct vmm_host_ram_bank *bank;

	for (bn = 0; bn < rctrl.bank_count; bn++) {
		bank = &rctrl.banks[bn];

		if ((pa < bank->start) ||
		    ((bank->start + bank->size) < (pa + sz))) {
			continue;
		}

		bpos = (pa - bank->start) >> VMM_PAGE_SHIFT;
		bcnt = VMM_SIZE_TO_PAGE(sz);

		vmm_spin_lock_irqsave_lite(&bank->bmap_lock, flags);

		if (bank->bmap_free < bcnt) {
			vmm_spin_unlock_irqrestore_lite(&bank->bmap_lock, flags);
			rc = VMM_ENOSPC;
			break;
		}

		bfree = 0;
		for (i = bpos; i < (bpos + bcnt); i++) {
			if (bitmap_isset(bank->bmap, i)) {
				break;
			}
			bfree++;
		}

		if (bfree != bcnt) {
			vmm_spin_unlock_irqrestore_lite(&bank->bmap_lock, flags);
			rc = VMM_ENOSPC;
			break;
		}

		bitmap_set(bank->bmap, bpos, bcnt);
		bank->bmap_free -= bcnt;

		vmm_spin_unlock_irqrestore_lite(&bank->bmap_lock, flags);

		rc = VMM_OK;
		break;
	}

	return rc;
}

int vmm_host_ram_free(physical_addr_t pa, physical_size_t sz)
{
	int rc = VMM_EINVALID;
	u32 bn, bcnt, bpos;
	irq_flags_t flags;
	struct vmm_host_ram_bank *bank;

	for (bn = 0; bn < rctrl.bank_count; bn++) {
		bank = &rctrl.banks[bn];

		if ((pa < bank->start) ||
		    ((bank->start + bank->size) < (pa + sz))) {
			continue;
		}

		bpos = (pa - bank->start) >> VMM_PAGE_SHIFT;
		bcnt = VMM_SIZE_TO_PAGE(sz);

		vmm_spin_lock_irqsave_lite(&bank->bmap_lock, flags);

		bitmap_clear(bank->bmap, bpos, bcnt);
		bank->bmap_free += bcnt;

		vmm_spin_unlock_irqrestore_lite(&bank->bmap_lock, flags);

		rc = VMM_OK;
		break;
	}

	return rc;
}

bool vmm_host_ram_frame_isfree(physical_addr_t pa)
{
	u32 bn, bpos;
	bool ret = FALSE;
	irq_flags_t flags;
	struct vmm_host_ram_bank *bank;

	for (bn = 0; bn < rctrl.bank_count; bn++) {
		bank = &rctrl.banks[bn];

		if ((pa < bank->start) ||
		    ((bank->start + bank->size) <= pa)) {
			continue;
		}

		bpos = (pa - bank->start) >> VMM_PAGE_SHIFT;

		vmm_spin_lock_irqsave_lite(&bank->bmap_lock, flags);

		if (!bitmap_isset(bank->bmap, bpos)) {
			ret = TRUE;
		}

		vmm_spin_unlock_irqrestore_lite(&bank->bmap_lock, flags);

		break;
	}

	return ret;
}

u32 vmm_host_ram_total_free_frames(void)
{
	u32 bn, ret = 0;
	irq_flags_t flags;
	struct vmm_host_ram_bank *bank;

	for (bn = 0; bn < rctrl.bank_count; bn++) {
		bank = &rctrl.banks[bn];

		vmm_spin_lock_irqsave_lite(&bank->bmap_lock, flags);
		ret += bank->bmap_free;
		vmm_spin_unlock_irqrestore_lite(&bank->bmap_lock, flags);
	}

	return ret;
}

u32 vmm_host_ram_total_frame_count(void)
{
	u32 bn, ret = 0;

	for (bn = 0; bn < rctrl.bank_count; bn++) {
		ret += rctrl.banks[bn].frame_count;
	}

	return ret;
}

physical_size_t vmm_host_ram_total_size(void)
{
	u32 bn;
	physical_size_t ret = 0;

	for (bn = 0; bn < rctrl.bank_count; bn++) {
		ret += rctrl.banks[bn].size;
	}

	return ret;
}

u32 vmm_host_ram_bank_count(void)
{
	return rctrl.bank_count;
}

physical_addr_t vmm_host_ram_bank_start(u32 bank)
{
	return (bank < rctrl.bank_count) ? rctrl.banks[bank].start : 0;
}

physical_size_t vmm_host_ram_bank_size(u32 bank)
{
	return (bank < rctrl.bank_count) ? rctrl.banks[bank].size : 0;
}

u32 vmm_host_ram_bank_frame_count(u32 bank)
{
	return (bank < rctrl.bank_count) ? rctrl.banks[bank].frame_count : 0;
}

u32 vmm_host_ram_bank_free_frames(u32 bank)
{
	u32 ret;
	irq_flags_t flags;
	struct vmm_host_ram_bank *bankp;

	if (bank >= rctrl.bank_count) {
		return 0;
	}

	bankp = &rctrl.banks[bank];

	vmm_spin_lock_irqsave_lite(&bankp->bmap_lock, flags);
	ret = bankp->bmap_free;
	vmm_spin_unlock_irqrestore_lite(&bankp->bmap_lock, flags);

	return ret;
}

virtual_size_t vmm_host_ram_estimate_hksize(void)
{
	int rc;
	u32 bn, count;
	virtual_size_t ret;
	physical_size_t size;

	if ((rc = arch_devtree_ram_bank_count(&count))) {
		return 0;
	}
	if (!count || (count > CONFIG_MAX_RAM_BANK_COUNT)) {
		return 0;
	}

	ret = 0;
	for (bn = 0; bn < count; bn++) {
		if ((rc = arch_devtree_ram_bank_size(bn, &size))) {
			return ret;
		}

		ret += bitmap_estimate_size(size >> VMM_PAGE_SHIFT);
	}

	return ret;
}

int __init vmm_host_ram_init(virtual_addr_t hkbase)
{
	int rc;
	u32 bn;
	struct vmm_host_ram_bank *bank;

	memset(&rctrl, 0, sizeof(rctrl));

	if ((rc = arch_devtree_ram_bank_count(&rctrl.bank_count))) {
		return rc;
	}
	if (!rctrl.bank_count) {
		return VMM_ENODEV;
	}
	if (rctrl.bank_count > CONFIG_MAX_RAM_BANK_COUNT) {
		return VMM_EINVALID;
	}

	for (bn = 0; bn < rctrl.bank_count; bn++) {
		bank = &rctrl.banks[bn];

		if ((rc = arch_devtree_ram_bank_start(bn, &bank->start))) {
			return rc;
		}
		if (bank->start & VMM_PAGE_MASK) {
			return VMM_EINVALID;
		}
		if ((rc = arch_devtree_ram_bank_size(bn, &bank->size))) {
			return rc;
		}
		if (bank->size & VMM_PAGE_MASK) {
			return VMM_EINVALID;
		}

		bank->frame_count = bank->size >> VMM_PAGE_SHIFT;

		INIT_SPIN_LOCK(&bank->bmap_lock);

		bank->bmap = (unsigned long *)hkbase;
		bank->bmap_sz = bitmap_estimate_size(bank->frame_count);
		bank->bmap_free = bank->frame_count;

		bitmap_zero(bank->bmap, bank->frame_count);

		bank->res.start = bank->start;
		bank->res.end = bank->start + bank->size - 1;
		bank->res.name = "System RAM";
		bank->res.flags = VMM_IORESOURCE_MEM | VMM_IORESOURCE_BUSY;
		rc = vmm_request_resource(&vmm_hostmem_resource, &bank->res);
		if (rc) {
			return rc;
		}

		hkbase += bank->bmap_sz;
	}

	return VMM_OK;
}

