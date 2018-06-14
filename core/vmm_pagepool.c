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
 * @file vmm_pagepool.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief source file for page pool subsystem
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_host_aspace.h>
#include <vmm_heap.h>
#include <vmm_pagepool.h>

static u32 pagepool_type2flags(enum vmm_pagepool_type page_type)
{
	switch (page_type) {
	case VMM_PAGEPOOL_NORMAL:
		return VMM_MEMORY_FLAGS_NORMAL;
	case VMM_PAGEPOOL_NORMAL_NOCACHE:
		return VMM_MEMORY_FLAGS_NORMAL_NOCACHE;
	case VMM_PAGEPOOL_NORMAL_WT:
		return VMM_MEMORY_FLAGS_NORMAL_WT;
	case VMM_PAGEPOOL_DMA_COHERENT:
		return VMM_MEMORY_FLAGS_DMA_COHERENT;
	case VMM_PAGEPOOL_DMA_NONCOHERENT:
		return VMM_MEMORY_FLAGS_DMA_NONCOHERENT;
	case VMM_PAGEPOOL_IO:
		return VMM_MEMORY_FLAGS_IO;
	default:
		break;
	};

	return VMM_MEMORY_FLAGS_NORMAL;
}

virtual_addr_t vmm_pagepool_alloc(enum vmm_pagepool_type page_type,
				  u32 page_count)
{
	/* TODO: */
	return vmm_host_alloc_pages(page_count,
				    pagepool_type2flags(page_type));
}

int vmm_pagepool_free(enum vmm_pagepool_type page_type,
		      virtual_addr_t page_va, u32 page_count)
{
	/* TODO: */
	return vmm_host_free_pages(page_va, page_count);
}

int __init vmm_pagepool_init(void)
{
	/* TODO: */
	return VMM_OK;
}
