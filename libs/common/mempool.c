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
 * @file mempool.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of memory pool APIs
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_host_aspace.h>
#include <libs/mathlib.h>
#include <libs/stringlib.h>
#include <libs/mempool.h>

struct mempool *mempool_raw_create(u32 entity_size,
				   physical_addr_t phys,
				   virtual_size_t size,
				   u32 mem_flags)
{
	u32 e;
	virtual_addr_t va;
	struct mempool *mp;

	if (!entity_size || (size < entity_size)) {
		return NULL;
	}

	mp = vmm_zalloc(sizeof(struct mempool));
	if (!mp) {
		return NULL;
	}

	mp->type = MEMPOOL_TYPE_RAW;
	mp->entity_size = entity_size;
	mp->entity_count = udiv64(size, entity_size);

	mp->f = fifo_alloc(sizeof(virtual_addr_t), mp->entity_count);
	if (!mp->f) {
		vmm_free(mp);
		return NULL;
	}

	mp->entity_base = vmm_host_memmap(phys, size, mem_flags);
	if (!mp->entity_base) {
		fifo_free(mp->f);
		vmm_free(mp);
		return NULL;
	}
	mp->d.raw.phys = phys;
	mp->d.raw.size = size;
	mp->d.raw.mem_flags = mem_flags;

	for (e = 0; e < mp->entity_count; e++) {
		va = mp->entity_base + e * entity_size;
		fifo_enqueue(mp->f, &va, FALSE);
	}

	return mp;
}

struct mempool *mempool_ram_create(u32 entity_size,
				   u32 page_count,
				   u32 mem_flags)
{
	u32 e;
	virtual_addr_t va;
	struct mempool *mp;

	if (!entity_size ||
	    ((VMM_PAGE_SIZE * page_count) < entity_size)) {
		return NULL;
	}

	mp = vmm_zalloc(sizeof(struct mempool));
	if (!mp) {
		return NULL;
	}

	mp->type = MEMPOOL_TYPE_RAM;
	mp->entity_size = entity_size;
	mp->entity_count =
		udiv64((VMM_PAGE_SIZE * page_count), entity_size);

	mp->f = fifo_alloc(sizeof(virtual_addr_t), mp->entity_count);
	if (!mp->f) {
		vmm_free(mp);
		return NULL;
	}

	mp->entity_base = vmm_host_alloc_pages(page_count, mem_flags);
	if (!mp->entity_base) {
		fifo_free(mp->f);
		vmm_free(mp);
		return NULL;
	}
	mp->d.ram.page_count = page_count;
	mp->d.ram.mem_flags = mem_flags;

	for (e = 0; e < mp->entity_count; e++) {
		va = mp->entity_base + e * entity_size;
		fifo_enqueue(mp->f, &va, FALSE);
	}

	return mp;
}

struct mempool *mempool_heap_create(u32 entity_size,
				    u32 entity_count)
{
	u32 e;
	virtual_addr_t va;
	struct mempool *mp;

	if (!entity_size || !entity_count) {
		return NULL;
	}

	mp = vmm_zalloc(sizeof(struct mempool));
	if (!mp) {
		return NULL;
	}

	mp->type = MEMPOOL_TYPE_HEAP;
	mp->entity_size = entity_size;
	mp->entity_count = entity_count;

	mp->f = fifo_alloc(sizeof(virtual_addr_t), mp->entity_count);
	if (!mp->f) {
		vmm_free(mp);
		return NULL;
	}

	mp->entity_base =
		(virtual_addr_t)vmm_malloc(entity_size * entity_count);
	if (!mp->entity_base) {
		fifo_free(mp->f);
		vmm_free(mp);
		return NULL;
	}

	for (e = 0; e < mp->entity_count; e++) {
		va = mp->entity_base + e * entity_size;
		fifo_enqueue(mp->f, &va, FALSE);
	}

	return mp;
}

int mempool_destroy(struct mempool *mp)
{
	int rc = VMM_OK;

	if (!mp) {
		return VMM_EFAIL;
	}

	switch (mp->type) {
	case MEMPOOL_TYPE_RAW:
		rc = vmm_host_memunmap(mp->entity_base);
		break;
	case MEMPOOL_TYPE_RAM:
		rc = vmm_host_free_pages(mp->entity_base,
					 mp->d.ram.page_count);
		break;
	case MEMPOOL_TYPE_HEAP:
		vmm_free((void *)mp->entity_base);
		break;
	default:
		return VMM_EINVALID;
	};

	fifo_free(mp->f);
	vmm_free(mp);

	return rc;
}

bool mempool_check_ptr(struct mempool *mp, void *entity)
{
	virtual_addr_t entity_va;

	if (!mp || !entity) {
		return FALSE;
	}

	entity_va = (virtual_addr_t)entity;
	if ((entity_va < mp->entity_base) || 
	    ((mp->entity_base +
	     (mp->entity_count * mp->entity_size)) < entity_va)) {
		return FALSE;
	}

	return TRUE;
}

enum mempool_type mempool_get_type(struct mempool *mp)
{
	return (mp) ? mp->type : MEMPOOL_TYPE_UNKNOWN;
}

u32 mempool_total_entities(struct mempool *mp)
{
	return (mp) ? mp->entity_count : 0;
}

u32 mempool_free_entities(struct mempool *mp)
{
	return (mp) ? fifo_avail(mp->f) : 0;
}

void *mempool_malloc(struct mempool *mp)
{
	virtual_addr_t entity_va;

	if (!mp) {
		return NULL;
	}

	if (fifo_dequeue(mp->f, &entity_va)) {
		return (void *)entity_va;
	}

	return NULL;
}

void *mempool_zalloc(struct mempool *mp)
{
	void *ret = mempool_malloc(mp);

	if (mp && ret) {
		memset(ret, 0, mp->entity_size);
	}

	return ret;
}

int mempool_free(struct mempool *mp, void *entity)
{
	virtual_addr_t entity_va;

	if (!mp) {
		return VMM_EFAIL;
	}

	if (!mempool_check_ptr(mp, entity)) {
		return VMM_EINVALID;
	}

	entity_va = (virtual_addr_t)entity;
	if (!fifo_enqueue(mp->f, &entity_va, FALSE)) {
		return VMM_ENOSPC;
	}

	return VMM_OK;
}

