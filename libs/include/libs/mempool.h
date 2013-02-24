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

/** MEMPOOl representation */
struct mempool {
	struct fifo *f;

	u32 buf_size;
	u32 buf_count;

	u32 page_count;
	virtual_addr_t page_base;
};

/** Create a new MEMPOOL */
struct mempool *mempool_create(u32 buf_size, u32 buf_count);

/** Destroy a MEMPOOL */
int mempool_destroy(struct mempool *mp);

/** Alloc a new buffer from MEMPOOL */
void *mempool_malloc(struct mempool *mp);

/** Alloc a new zeroed buffer from MEMPOOL */
void *mempool_zalloc(struct mempool *mp);

/** Free a buffer to MEMPOOL */
int mempool_free(struct mempool *mp, void *buf);

#endif /* __MEMPOOL_H__ */
