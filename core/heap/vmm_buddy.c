/**
 * Copyright (c) 2010 Himanshu Chauhan.
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
 * @file vmm_buddy.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief source file for buddy heap allocator
 */

#include <vmm_error.h>
#include <vmm_cache.h>
#include <vmm_heap.h>
#include <vmm_stdio.h>
#include <vmm_host_aspace.h>
#include <libs/buddy.h>

struct vmm_buddy_heap {
	struct buddy_allocator ba;
	void *hk_start;
	unsigned long hk_size;
	void *mem_start;
	unsigned long mem_size;
	void *heap_start;
	unsigned long heap_size;
};

static struct vmm_buddy_heap bheap;

#define HEAP_MIN_BIN		(VMM_CACHE_LINE_SHIFT)
#define HEAP_MAX_BIN		(VMM_PAGE_SHIFT)

void *vmm_malloc(virtual_size_t size)
{
	int rc;
	unsigned long addr;

	if (!size) {
		return NULL;
	}

	rc = buddy_mem_alloc(&bheap.ba, size, &addr);
	if (rc) {
		vmm_printf("%s: Failed to alloc size=%d (error %d)\n",
			   __func__, size, rc);
		return NULL;
	}

	return (void *)addr;
}

void *vmm_zalloc(virtual_size_t size)
{
	void *ret = vmm_malloc(size);

	if (ret) {
		memset(ret, 0, size);
	}

	return ret;
}

void vmm_free(void *ptr)
{
	int rc;

	BUG_ON(!ptr);
	BUG_ON(ptr < bheap.mem_start);
	BUG_ON((bheap.mem_start + bheap.mem_size) <= ptr);

	rc = buddy_mem_free(&bheap.ba, (unsigned long)ptr);
	if (rc) {
		vmm_printf("%s: Failed to free ptr=%p (error %d)\n",
			   __func__, ptr, rc);
	}
}

int vmm_heap_allocator_name(char *name, int name_sz)
{
	if (!name || name_sz <= 0) {
		return VMM_EFAIL;
	}

	if (strlcpy(name, "Buddy System", name_sz) >= name_sz) {
		return VMM_EOVERFLOW;
	}

	return VMM_OK;
}

virtual_addr_t vmm_heap_start_va(void)
{
	return (virtual_addr_t)bheap.heap_start;
}

virtual_size_t vmm_heap_size(void)
{
	return (virtual_size_t)bheap.heap_size;
}

virtual_size_t vmm_heap_hksize(void)
{
	return bheap.hk_size;
}

virtual_size_t vmm_heap_free_size(void)
{
	return buddy_bins_free_space(&bheap.ba);
}

int vmm_heap_print_state(struct vmm_chardev *cdev)
{
	unsigned long idx;

	vmm_cprintf(cdev, "Heap State\n");

	for (idx = HEAP_MIN_BIN; idx <= HEAP_MAX_BIN; idx++) {
		if (idx < 10) {
			vmm_cprintf(cdev, "  [BLOCK %4dB]: ", 1<<idx);
		} else if (idx < 20) {
			vmm_cprintf(cdev, "  [BLOCK %4dK]: ", 1<<(idx-10));
		} else {
			vmm_cprintf(cdev, "  [BLOCK %4dM]: ", 1<<(idx-20));
		}
		vmm_cprintf(cdev, "%5d area(s), %5d free block(s)\n",
			    buddy_bins_area_count(&bheap.ba, idx),
			    buddy_bins_block_count(&bheap.ba, idx));
	}

	vmm_cprintf(cdev, "House-Keeping State\n");
	vmm_cprintf(cdev, "  Buddy Areas: %d free out of %d\n",
		    buddy_hk_area_free(&bheap.ba),
		    buddy_hk_area_total(&bheap.ba));

	return VMM_OK;
}

int __init vmm_heap_init(void)
{
	memset(&bheap, 0, sizeof(bheap));

	bheap.heap_size = CONFIG_HEAP_SIZE_MB * 1024 * 1024;
	bheap.heap_start = (void *)vmm_host_alloc_pages(
					VMM_SIZE_TO_PAGE(bheap.heap_size),
					VMM_MEMORY_FLAGS_NORMAL);
	if (!bheap.heap_start) {
		return VMM_ENOMEM;
	}

	bheap.hk_start = bheap.heap_start;
	bheap.hk_size = (bheap.heap_size) / 8; /* 12.5 percent for house-keeping */
	bheap.mem_start = bheap.heap_start + bheap.hk_size;
	bheap.mem_size = bheap.heap_size - bheap.hk_size;

	return buddy_allocator_init(&bheap.ba,
			  bheap.hk_start, bheap.hk_size,
			  (unsigned long)bheap.mem_start, bheap.mem_size,
			  HEAP_MIN_BIN, HEAP_MAX_BIN);
}
