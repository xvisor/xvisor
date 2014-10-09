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
#include <vmm_stdio.h>
#include <vmm_spinlocks.h>
#include <vmm_host_aspace.h>
#include <vmm_host_vapool.h>
#include <libs/stringlib.h>
#include <libs/buddy.h>

#define VAPOOL_MIN_BIN		(VMM_PAGE_SHIFT)
#define VAPOOL_MAX_BIN		(20)

struct vmm_host_vapool_ctrl {
	virtual_addr_t vapool_start;
	virtual_size_t vapool_size;
	u32 vapool_page_count;
	struct buddy_allocator ba;
};

static struct vmm_host_vapool_ctrl vpctrl;

int vmm_host_vapool_alloc(virtual_addr_t *va, virtual_size_t sz)
{
	int rc;
	unsigned long addr;

	if (!va) {
		return VMM_EINVALID;
	}

	rc = buddy_mem_alloc(&vpctrl.ba, sz, &addr);
	if (!rc) {
		*va = addr;
	}

	return rc;
}

int vmm_host_vapool_reserve(virtual_addr_t va, virtual_size_t sz)
{
	if ((va < vpctrl.vapool_start) ||
	    ((vpctrl.vapool_start + vpctrl.vapool_size) < (va + sz))) {
		return VMM_EFAIL;
	}

	return buddy_mem_reserve(&vpctrl.ba, va, sz);
}

int vmm_host_vapool_find(virtual_addr_t va,
			 virtual_addr_t *alloc_va,
			 virtual_size_t *alloc_sz)
{
	int rc;
	unsigned long ava, asz;

	rc = buddy_mem_find(&vpctrl.ba, va, &ava, NULL, &asz);
	if (rc) {
		return rc;
	}

	if (alloc_va) {
		*alloc_va = ava;
	}
	if (alloc_sz) {
		*alloc_sz = asz;
	}

	return VMM_OK;
}

int vmm_host_vapool_free(virtual_addr_t va, virtual_size_t sz)
{
	if ((va < vpctrl.vapool_start) ||
	    ((vpctrl.vapool_start + vpctrl.vapool_size) < (va + sz))) {
		return VMM_EFAIL;
	}

	return buddy_mem_partial_free(&vpctrl.ba, va, sz);
}

bool vmm_host_vapool_page_isfree(virtual_addr_t va)
{
	bool ret = FALSE;

	if ((va < vpctrl.vapool_start) ||
	    ((vpctrl.vapool_start + vpctrl.vapool_size) <= va)) {
		return ret;
	}

	if (buddy_mem_find(&vpctrl.ba, va, NULL, NULL, NULL) != VMM_OK) {
		ret = TRUE;
	}

	return ret;
}

u32 vmm_host_vapool_free_page_count(void)
{
	return buddy_bins_free_space(&vpctrl.ba) >> VMM_PAGE_SHIFT;
}

u32 vmm_host_vapool_total_page_count(void)
{
	return vpctrl.vapool_page_count;
}

virtual_addr_t vmm_host_vapool_base(void)
{
	return vpctrl.vapool_start;
}

virtual_size_t vmm_host_vapool_size(void)
{
	return vpctrl.vapool_size;
}

bool vmm_host_vapool_isvalid(virtual_addr_t addr)
{
	if ((vpctrl.vapool_start <= addr) &&
	    (addr < (vpctrl.vapool_start + vpctrl.vapool_size))) {
		return TRUE;
	}
	return FALSE;
}

virtual_size_t vmm_host_vapool_estimate_hksize(virtual_size_t size)
{
	/* VAPOOL House-Keeping Size = (Total VAPOOL Size / 256); 
	 * 12MB VAPOOL   => 48KB House-Keeping
	 * 16MB VAPOOL   => 64KB House-Keeping
	 * 32MB VAPOOL   => 128KB House-Keeping
	 * 64MB VAPOOL   => 256KB House-Keeping
	 * 128MB VAPOOL  => 512KB House-Keeping
	 * 256MB VAPOOL  => 1024KB House-Keeping
	 * 512MB VAPOOL  => 2048KB House-Keeping
	 * 1024MB VAPOOL => 4096KB House-Keeping
	 * ..... and so on .....
	 */
	return size >> 8;
}

int vmm_host_vapool_print_state(struct vmm_chardev *cdev)
{
	unsigned long idx;

	vmm_cprintf(cdev, "VAPOOL State\n");

	for (idx = VAPOOL_MIN_BIN; idx <= VAPOOL_MAX_BIN; idx++) {
		if (idx < 10) {
			vmm_cprintf(cdev, "  [BLOCK %4dB]: ", 1<<idx);
		} else if (idx < 20) {
			vmm_cprintf(cdev, "  [BLOCK %4dK]: ", 1<<(idx-10));
		} else {
			vmm_cprintf(cdev, "  [BLOCK %4dM]: ", 1<<(idx-20));
		}
		vmm_cprintf(cdev, "%5d area(s), %5d free block(s)\n",
			    buddy_bins_area_count(&vpctrl.ba, idx),
			    buddy_bins_block_count(&vpctrl.ba, idx));
	}

	vmm_cprintf(cdev, "VAPOOL House-Keeping State\n");
	vmm_cprintf(cdev, "  Buddy Areas: %d free out of %d\n",
		    buddy_hk_area_free(&vpctrl.ba),
		    buddy_hk_area_total(&vpctrl.ba));

	return VMM_OK;
}

int __init vmm_host_vapool_init(virtual_addr_t base,
				virtual_size_t size, 
				virtual_addr_t hkbase)
{
	int rc;

	if ((hkbase < base) || ((base + size) <= hkbase)) {
		return VMM_EFAIL;
	}

	vpctrl.vapool_start = base;
	vpctrl.vapool_size = size;
	vpctrl.vapool_start &= ~VMM_PAGE_MASK;
	vpctrl.vapool_size &= ~VMM_PAGE_MASK;
	vpctrl.vapool_page_count = vpctrl.vapool_size >> VMM_PAGE_SHIFT;

	rc = buddy_allocator_init(&vpctrl.ba, (void *)hkbase,
				  vmm_host_vapool_estimate_hksize(size),
				  base, size, VAPOOL_MIN_BIN, VAPOOL_MAX_BIN);
	if (rc) {
		return rc;
	}

	return VMM_OK;
}

