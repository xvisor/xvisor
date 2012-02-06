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
 * @file vmm_ringbuf.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief source file for generic ring buffer.
 */

#include <vmm_types.h>
#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_heap.h>
#include <vmm_string.h>
#include <vmm_ringbuf.h>

struct vmm_ringbuf *vmm_ringbuf_alloc(u32 key_size, u32 key_count)
{
	struct vmm_ringbuf *rb;

	rb = vmm_malloc(sizeof(struct vmm_ringbuf));
	if (!rb) {
		return NULL;
	}

	INIT_SPIN_LOCK(&rb->lock);
	rb->keys = vmm_malloc(key_size * key_count);
	if (!rb->keys) {
		goto rb_init_fail;
	}
	rb->key_size = key_size;
	rb->key_count = key_count;
	rb->read_pos = 0;
	rb->write_pos = 0;
	rb->avail_count = 0;

	return rb;

rb_init_fail:
	vmm_free(rb);
	return NULL;
}

bool vmm_ringbuf_isempty(struct vmm_ringbuf *rb)
{
	bool isempty;

	if (!rb) {
		return TRUE;
	}

	vmm_spin_lock(&rb->lock);

	isempty = (rb->read_pos == rb->write_pos);

	vmm_spin_unlock(&rb->lock);

	return isempty;
}

bool vmm_ringbuf_isfull(struct vmm_ringbuf *rb)
{
	u32 write_pos_mod;
	bool isfull;

	if (!rb) {
		return FALSE;
	}

	vmm_spin_lock(&rb->lock);

	write_pos_mod = (rb->write_pos + 1);
	if (rb->key_count <= write_pos_mod) {
		write_pos_mod -= rb->key_count;
	}
	isfull = (rb->read_pos == write_pos_mod);

	vmm_spin_unlock(&rb->lock);

	return isfull;
}

bool vmm_ringbuf_enqueue(struct vmm_ringbuf *rb, void *srckey, bool overwrite)
{
	u32 read_pos_mod, write_pos_mod;
	bool isfull, update;

	if (!rb || !srckey) {
		return FALSE;
	}

	vmm_spin_lock(&rb->lock);

	read_pos_mod = (rb->read_pos + 1);
	if (rb->key_count <= read_pos_mod) {
		read_pos_mod -= rb->key_count;
	}
	write_pos_mod = (rb->write_pos + 1);
	if (rb->key_count <= write_pos_mod) {
		write_pos_mod -= rb->key_count;
	}
	isfull = (rb->read_pos == write_pos_mod);
	update = FALSE;
	if (overwrite) {
		if (isfull) {
			rb->read_pos = read_pos_mod;
			rb->avail_count--;
		}
		update = TRUE;
	} else {
		if (!isfull) {
			update = TRUE;
		}
	}
	if(update) {
		switch(rb->key_size) {
		case 1:
			*((u8 *)(rb->keys + (rb->write_pos * rb->key_size)))
			= *((u8 *)srckey);
			break;
		case 2:
			*((u16 *)(rb->keys + (rb->write_pos * rb->key_size)))
			= *((u16 *)srckey);
			break;
		case 4:
			*((u32 *)(rb->keys + (rb->write_pos * rb->key_size)))
			= *((u32 *)srckey);
			break;
		default:
			vmm_memcpy(rb->keys + (rb->write_pos * rb->key_size), 
				   srckey, 
				   rb->key_size);
			break;
		};
		rb->write_pos = write_pos_mod;
		rb->avail_count++;
	}

	vmm_spin_unlock(&rb->lock);

	return update;
}

bool vmm_ringbuf_dequeue(struct vmm_ringbuf *rb, void *dstkey)
{
	u32 read_pos_mod;
	bool isempty;

	if (!rb || !dstkey) {
		return FALSE;
	}

	vmm_spin_lock(&rb->lock);

	isempty = (rb->read_pos == rb->write_pos);

	if (!isempty) {
		switch(rb->key_size) {
		case 1:
			*((u8 *)dstkey) =
			*((u8 *)(rb->keys + (rb->read_pos * rb->key_size)));
			break;
		case 2:
			*((u16 *)dstkey) =
			*((u16 *)(rb->keys + (rb->read_pos * rb->key_size)));
			break;
		case 4:
			*((u32 *)dstkey) = 
			*((u32 *)(rb->keys + (rb->read_pos * rb->key_size)));
			break;
		default:
			vmm_memcpy(dstkey, 
				   rb->keys + (rb->read_pos * rb->key_size),
				   rb->key_size);
			break;
		};
		read_pos_mod = (rb->read_pos + 1);
		if (rb->key_count <= read_pos_mod) {
			read_pos_mod -= rb->key_count;
		}
		rb->read_pos = read_pos_mod;
		rb->avail_count--;
	}

	vmm_spin_unlock(&rb->lock);

	return !isempty;
}

bool vmm_ringbuf_getkey(struct vmm_ringbuf *rb, u32 index, void *dstkey)
{
	if (!rb || !dstkey) {
		return FALSE;
	}

	if (rb->key_count <= index) {
		return FALSE;
	}
	
	vmm_spin_lock(&rb->lock);

	index = (rb->read_pos + index);
	if (rb->key_count <= index) {
		index -= rb->key_count;
	}
	switch(rb->key_size) {
	case 1:
		*((u8 *)dstkey) =
		*((u8 *)(rb->keys + (index * rb->key_size)));
		break;
	case 2:
		*((u16 *)dstkey) =
		*((u16 *)(rb->keys + (index * rb->key_size)));
		break;
	case 4:
		*((u32 *)dstkey) = 
		*((u32 *)(rb->keys + (index * rb->key_size)));
		break;
	default:
		vmm_memcpy(dstkey, 
			   rb->keys + (index * rb->key_size),
			   rb->key_size);
		break;
	};

	vmm_spin_unlock(&rb->lock);

	return TRUE;
}

u32 vmm_ringbuf_avail(struct vmm_ringbuf *rb)
{
	u32 retval;

	if (!rb) {
		return 0;
	}

	vmm_spin_lock(&rb->lock);

	retval = rb->avail_count;

	vmm_spin_unlock(&rb->lock);

	return retval;
}

int vmm_ringbuf_free(struct vmm_ringbuf *rb)
{
	vmm_free(rb->keys);
	vmm_free(rb);

	return VMM_OK;
}
