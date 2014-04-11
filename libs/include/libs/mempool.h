/**
 * Copyright (c) 2013 Anup Patel.
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
 * @file mempool.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for memory pool APIs.
 */

#ifndef __MEMPOOL_H__
#define __MEMPOOL_H__

#include <vmm_types.h>
#include <libs/fifo.h>

/** MEMPOOL types */
enum mempool_type {
	MEMPOOL_TYPE_UNKNOWN = 0,
	MEMPOOL_TYPE_RAW,
	MEMPOOL_TYPE_RAM,
	MEMPOOL_TYPE_HEAP,
	MEMPOOL_MAX_TYPES
};

/** MEMPOOl representation 
 *
 *  A MEMPOOL is a memory allocator for fixed sized entities.
 *  For each MEMPOOL, we create a pool of entities on RAM pages,
 *  RAW/Device memory, or Heap.
 */
struct mempool {
	/* Type of MEMPOOL */
	enum mempool_type type;

	/* Size, Count and Base address of entities in MEMPOOL */
	u32 entity_size;
	u32 entity_count;
	virtual_addr_t entity_base;

	/* Internal FIFO to manage entities */
	struct fifo *f;

	/* Additional fields based on MEMPOOL Type */
	union {
		/* Additional fields for MEMPOOL_TYPE_RAW */
		struct {
			physical_addr_t phys;
			virtual_size_t size;
			u32 mem_flags;
		} raw;
		/* Additional fields for MEMPOOL_TYPE_RAM */
		struct {
			u32 page_count;
			u32 mem_flags;
		} ram;
	} d;
};

/** Create a MEMPOOL on RAW/Device memory */
struct mempool *mempool_raw_create(u32 entity_size,
				   physical_addr_t phys,
				   virtual_size_t size,
				   u32 mem_flags);

/** Create a MEMPOOL on RAM pages */
struct mempool *mempool_ram_create(u32 entity_size,
				   u32 page_count,
				   u32 mem_flags);

/** Create a MEMPOOL on Heap memory */
struct mempool *mempool_heap_create(u32 entity_size,
				    u32 entity_count);

/** Destroy a MEMPOOL */
int mempool_destroy(struct mempool *mp);

/** Check if given pointer is a valid MEMPOOL entity */
bool mempool_check_ptr(struct mempool *mp, void *entity);

/** Get the type of MEMPOOL */
enum mempool_type mempool_get_type(struct mempool *mp);

/** Get count of total entities in MEMPOOL */
u32 mempool_total_entities(struct mempool *mp);

/** Get count of free entities in MEMPOOL */
u32 mempool_free_entities(struct mempool *mp);

/** Alloc a new entity from MEMPOOL */
void *mempool_malloc(struct mempool *mp);

/** Alloc a new zeroed entity from MEMPOOL */
void *mempool_zalloc(struct mempool *mp);

/** Free a entity to MEMPOOL */
int mempool_free(struct mempool *mp, void *entity);

#endif /* __MEMPOOL_H__ */
