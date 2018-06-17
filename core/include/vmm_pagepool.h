/**
 * Copyright (c) 2018 Anup Patel.
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
 * @file vmm_pagepool.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for page pool subsystem
 *
 * This subsystem provides managed page allocations so that
 * we can track page allocations and also use hugepages for
 * all page allocations.
 */
#ifndef _VMM_PAGEPOOL_H__
#define _VMM_PAGEPOOL_H__

#include <vmm_types.h>

enum vmm_pagepool_type {
	VMM_PAGEPOOL_NORMAL=0,
	VMM_PAGEPOOL_NORMAL_NOCACHE,
	VMM_PAGEPOOL_NORMAL_WT,
	VMM_PAGEPOOL_DMA_COHERENT,
	VMM_PAGEPOOL_DMA_NONCOHERENT,
	VMM_PAGEPOOL_IO,
	VMM_PAGEPOOL_MAX
};

/** Get name of page pool type */
const char *vmm_pagepool_name(enum vmm_pagepool_type page_type);

/** Get total space in given page pool type */
virtual_size_t vmm_pagepool_space(enum vmm_pagepool_type page_type);

/** Get number of entries in given page pool type */
u32 vmm_pagepool_entry_count(enum vmm_pagepool_type page_type);

/** Get number of hugepages in given page pool type */
u32 vmm_pagepool_hugepage_count(enum vmm_pagepool_type page_type);

/** Get total number of pages in given page pool type */
u32 vmm_pagepool_page_count(enum vmm_pagepool_type page_type);

/** Get number of availabe pages in given page pool type */
u32 vmm_pagepool_page_avail_count(enum vmm_pagepool_type page_type);

/** Allocate pages from page pool */
virtual_addr_t vmm_pagepool_alloc(enum vmm_pagepool_type page_type,
				  u32 page_count);

/** Free pages back to page pool */
int vmm_pagepool_free(enum vmm_pagepool_type page_type,
		      virtual_addr_t page_va, u32 page_count);

/** Initialization page pool subsystem */
int vmm_pagepool_init(void);

#endif
