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
 * @file vmm_host_vapool.h
 * @author Anup patel (anup@brainfault.org)
 * @brief Header file for virtual address pool management.
 */

#ifndef __VMM_HOST_VAPOOL_H_
#define __VMM_HOST_VAPOOL_H_

#include <vmm_types.h>

/** Allocate virtual space from virtual address pool */
int vmm_host_vapool_alloc(virtual_addr_t * va, 
			  virtual_size_t sz, 
			  bool aligned);

/** Reserve a portion of virtual space forcefully */
int vmm_host_vapool_reserve(virtual_addr_t va, virtual_size_t sz);

/** Free virtual space to virtual address pool */
int vmm_host_vapool_free(virtual_addr_t va, virtual_size_t sz);

/** Base address of virtual address pool */
virtual_addr_t vmm_host_vapool_base(void);

/** Check if a virtual address is free */
bool vmm_host_vapool_page_isfree(virtual_addr_t va);

/** Free page count of virtual address pool */
u32 vmm_host_vapool_free_page_count(void);

/** Total page count of virtual address pool */
u32 vmm_host_vapool_total_page_count(void);

/** Total size of virtual address pool */
virtual_size_t vmm_host_vapool_size(void);

/** Estimate house-keeping size of virtual address pool */
virtual_size_t vmm_host_vapool_estimate_hksize(virtual_size_t size);

/* Initialize virtual address pool managment */
int vmm_host_vapool_init(virtual_addr_t base,
			 virtual_size_t size, 
			 virtual_addr_t hkbase, 
			 virtual_addr_t resv_va, 
			 virtual_size_t resv_sz);

#endif /* __VMM_HOST_VAPOOL_H_ */
