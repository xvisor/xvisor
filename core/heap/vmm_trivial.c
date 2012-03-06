/**
 * Copyright (c) 2010 Anup Patel.
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
 * @file vmm_trivial.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief source file for trivial heap allocator
 */

#include <vmm_error.h>
#include <vmm_string.h>
#include <vmm_stdio.h>
#include <vmm_heap.h>
#include <vmm_host_aspace.h>

struct vmm_trivial_control {
	virtual_addr_t base;
	virtual_size_t size;
	virtual_addr_t curoff;
};

static struct vmm_trivial_control heap_ctrl;

void *vmm_malloc(virtual_size_t size)
{
	void *retval = NULL;
	if (size & 0x3)
		size = ((size >> 2) + 1) << 2;
	if ((heap_ctrl.curoff + size) < heap_ctrl.size) {
		retval = (void *)(heap_ctrl.base + heap_ctrl.curoff);
		heap_ctrl.curoff += size;
		vmm_memset(retval, 0, size);
	}
	return retval;
}

void vmm_free(void *pointer)
{
	/* Nothing to be done for freeing */
}

int vmm_heap_allocator_name(char * name, int name_sz)
{
	if (!name && name_sz > 0) {
		return VMM_EFAIL;
	}

	vmm_strncpy(name, "Trivial", name_sz);

	return VMM_OK;
}

virtual_addr_t vmm_heap_start_va(void)
{
	return (virtual_addr_t)heap_ctrl.base;
}

virtual_size_t vmm_heap_size(void)
{
	return (virtual_size_t)heap_ctrl.size;
}

virtual_size_t vmm_heap_hksize(void)
{
	return (virtual_size_t)0;
}

int vmm_heap_print_state(struct vmm_chardev *cdev)
{
	vmm_cprintf(cdev, "Heap State\n");
	vmm_cprintf(cdev, "  Used Space  : %d KiB\n", 
				heap_ctrl.curoff / 1024);
	vmm_cprintf(cdev, "  Free Space  : %d KiB\n", 
				(heap_ctrl.size - heap_ctrl.curoff) / 1024);
	vmm_cprintf(cdev, "  Total Space : %d KiB\n", 
				heap_ctrl.size / 1024);
	return VMM_OK;
}

int __init vmm_heap_init(void)
{
	u32 heap_size = 0, heap_page_count = 0, heap_mem_flags;
	virtual_addr_t heap_start = 0x0;

	heap_size = CONFIG_HEAP_SIZE * 1024;
	heap_page_count =  VMM_ROUNDUP2_PAGE_SIZE(heap_size) / VMM_PAGE_SIZE;
	heap_mem_flags = 0;
	heap_mem_flags |= VMM_MEMORY_READABLE; 
	heap_mem_flags |= VMM_MEMORY_WRITEABLE; 
	heap_mem_flags |= VMM_MEMORY_CACHEABLE;
	heap_mem_flags |= VMM_MEMORY_BUFFERABLE;
	heap_start = vmm_host_alloc_pages(heap_page_count, heap_mem_flags);
	if (!heap_start) {
		return VMM_EFAIL;
	}

	/* Clear the memory of heap control structure */
	vmm_memset(&heap_ctrl, 0, sizeof(heap_ctrl));

	/* Load values in heap control structure */
	heap_ctrl.base = heap_start;
	heap_ctrl.size = heap_size;
	heap_ctrl.curoff = 0;

	return VMM_OK;
}
