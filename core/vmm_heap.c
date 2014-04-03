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
 * @file vmm_heap.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief heap management using buddy allocator
 */

#include <vmm_error.h>
#include <vmm_cache.h>
#include <vmm_heap.h>
#include <vmm_stdio.h>
#include <vmm_host_aspace.h>
#include <libs/buddy.h>

struct vmm_heap_control {
	struct buddy_allocator ba;
	void *hk_start;
	unsigned long hk_size;
	void *mem_start;
	unsigned long mem_size;
	void *heap_start;
	unsigned long heap_size;
};

static struct vmm_heap_control heap;

#define HEAP_MIN_BIN		(VMM_CACHE_LINE_SHIFT)
#define HEAP_MAX_BIN		(VMM_PAGE_SHIFT)

void *vmm_malloc(virtual_size_t size)
{
	int rc;
	unsigned long addr;

	if (!size) {
		return NULL;
	}

	rc = buddy_mem_alloc(&heap.ba, size, &addr);
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
	BUG_ON(ptr < heap.mem_start);
	BUG_ON((heap.mem_start + heap.mem_size) <= ptr);

	rc = buddy_mem_free(&heap.ba, (unsigned long)ptr);
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
	return (virtual_addr_t)heap.heap_start;
}

virtual_size_t vmm_heap_size(void)
{
	return (virtual_size_t)heap.heap_size;
}

virtual_size_t vmm_heap_hksize(void)
{
	return heap.hk_size;
}

virtual_size_t vmm_heap_free_size(void)
{
	return buddy_bins_free_space(&heap.ba);
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
			    buddy_bins_area_count(&heap.ba, idx),
			    buddy_bins_block_count(&heap.ba, idx));
	}

	vmm_cprintf(cdev, "House-Keeping State\n");
	vmm_cprintf(cdev, "  Buddy Areas: %d free out of %d\n",
		    buddy_hk_area_free(&heap.ba),
		    buddy_hk_area_total(&heap.ba));

	return VMM_OK;
}

int __init vmm_heap_init(void)
{
	memset(&heap, 0, sizeof(heap));

	heap.heap_size = CONFIG_HEAP_SIZE_MB * 1024 * 1024;
	heap.heap_start = (void *)vmm_host_alloc_pages(
					VMM_SIZE_TO_PAGE(heap.heap_size),
					VMM_MEMORY_FLAGS_NORMAL);
	if (!heap.heap_start) {
		return VMM_ENOMEM;
	}

	heap.hk_start = heap.heap_start;
	heap.hk_size = (heap.heap_size) / 8; /* 12.5 percent for house-keeping */
	heap.mem_start = heap.heap_start + heap.hk_size;
	heap.mem_size = heap.heap_size - heap.hk_size;

	return buddy_allocator_init(&heap.ba,
			  heap.hk_start, heap.hk_size,
			  (unsigned long)heap.mem_start, heap.mem_size,
			  HEAP_MIN_BIN, HEAP_MAX_BIN);
}
