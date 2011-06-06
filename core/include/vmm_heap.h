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
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for heap managment in VMM
 */
#ifndef _VMM_HEAP_H__
#define _VMM_HEAP_H__

#include <vmm_types.h>

/** Allocate memory */
void *vmm_malloc(virtual_size_t size);

/** Free memory */
void vmm_free(void *pointer);

/** Initialization function for head managment */
int vmm_heap_init(void);

#endif
