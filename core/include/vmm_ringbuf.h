/**
 * Copyright (c) 2010 Himanshu Chauhan.
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
 * @file vmm_ringbuf.h
 * @version 0.01
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief header file for generic ring buffer.
 */

#ifndef __VMM_RINGBUF_H__
#define __VMM_RINGBUF_H__

#include <vmm_types.h>
#include <vmm_spinlocks.h>

struct vmm_ringbuf {
	vmm_spinlock_t lock;
	void *keys;
	u32 key_size;
	u32 key_count;
	u32 read_pos;
	u32 write_pos;
	u32 avail_count;
};

struct vmm_ringbuf *vmm_ringbuf_alloc(u32 key_size, u32 key_count);
bool vmm_ringbuf_isempty(struct vmm_ringbuf *rb);
bool vmm_ringbuf_isfull(struct vmm_ringbuf *rb);
bool vmm_ringbuf_enqueue(struct vmm_ringbuf *rb, void *srckey, bool overwrite);
bool vmm_ringbuf_dequeue(struct vmm_ringbuf *rb, void *dstkey);
bool vmm_ringbuf_getkey(struct vmm_ringbuf *rb, u32 index, void *dstkey);
u32 vmm_ringbuf_avail(struct vmm_ringbuf *rb);
int vmm_ringbuf_free(struct vmm_ringbuf *rb);

#endif /* __VMM_RINGBUF_H__ */
