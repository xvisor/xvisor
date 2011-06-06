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
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief source file for trivial allocator in VMM
 */

#include <vmm_error.h>
#include <vmm_string.h>
#include <vmm_sections.h>
#include <vmm_heap.h>
#include <mm/vmm_trivial.h>

vmm_trivial_control_t heap_ctrl;

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

int vmm_heap_init(void)
{
	/* Clear the memory of heap control structure */
	vmm_memset(&heap_ctrl, 0, sizeof(heap_ctrl));

	/* Load values in heap control structure */
	heap_ctrl.base = vmm_heap_start();
	heap_ctrl.size = vmm_heap_size();
	heap_ctrl.curoff = 0;

	return VMM_OK;
}
