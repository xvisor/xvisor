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
 * @file vmm_heap.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for heap managment interface
 */
#ifndef _VMM_HEAP_H__
#define _VMM_HEAP_H__

#include <vmm_types.h>
#include <vmm_chardev.h>

/** Allocate memory */
void *vmm_malloc(virtual_size_t size);

/** Free memory */
void vmm_free(void *pointer);

/** Retrive name of heap allocator */
int vmm_heap_allocator_name(char * name, int name_sz);

/** Starting virtual address of heap */
virtual_addr_t vmm_heap_start_va(void);

/** Total size of heap (house-keeping + allocation) */
virtual_size_t vmm_heap_size(void);

/** Size of heap house-keeping */
virtual_size_t vmm_heap_hksize(void);

/** Print heap state */
int vmm_heap_print_state(struct vmm_chardev *cdev);

/** Initialization function for head managment */
int vmm_heap_init(void);

#endif
