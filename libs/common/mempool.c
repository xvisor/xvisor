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
#include <libs/stringlib.h>
#include <libs/mempool.h>

struct mempool *mempool_create(u32 buf_size, u32 buf_count)
{
	u32 b;
	virtual_addr_t va;
	struct mempool *mp;

	mp = vmm_zalloc(sizeof(struct mempool));
	if (!mp) {
		return NULL;
	}

	mp->f = fifo_alloc(sizeof(virtual_addr_t), buf_count);
	if (!mp->f) {
		vmm_free(mp);
		return NULL;
	}

	mp->buf_count = buf_count;
	mp->buf_size = buf_size;

	mp->page_count = VMM_SIZE_TO_PAGE(buf_size * buf_count);
	mp->page_base = vmm_host_alloc_pages(mp->page_count, 
					     VMM_MEMORY_FLAGS_NORMAL);
	if (!mp->page_base) {
		fifo_free(mp->f);
		vmm_free(mp);
		return NULL;
	}

	for (b = 0; b < mp->buf_count; b++) {
		va = mp->page_base + b * buf_size;
		fifo_enqueue(mp->f, &va, FALSE);
	}

	return mp;
}

int mempool_destroy(struct mempool *mp)
{
	if (!mp) {
		return VMM_EFAIL;
	}

	vmm_host_free_pages(mp->page_base, mp->page_count);
	fifo_free(mp->f);
	vmm_free(mp);

	return VMM_OK;
}

void *mempool_malloc(struct mempool *mp)
{
	virtual_addr_t buf_va;

	if (!mp) {
		return NULL;
	}

	if (fifo_dequeue(mp->f, &buf_va)) {
		return (void *)buf_va;
	}

	return NULL;
}

void *mempool_zalloc(struct mempool *mp)
{
	void *ret = mempool_malloc(mp);

	if (mp && ret) {
		memset(ret, 0, mp->buf_size);
	}

	return ret;
}

int mempool_free(struct mempool *mp, void *buf)
{
	virtual_addr_t buf_va;

	if (!mp) {
		return VMM_EFAIL;
	}

	buf_va = (virtual_addr_t)buf;

	if ((buf_va < mp->page_base) || 
	    ((mp->page_base + (mp->page_count * VMM_PAGE_SIZE)) < buf_va)) {
		return VMM_EINVALID;
	}

	if (!fifo_enqueue(mp->f, &buf_va, FALSE)) {
		return VMM_EFAIL;
	}

	return VMM_OK;
}

