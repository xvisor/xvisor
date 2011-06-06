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
 * @file vmm_ringbuffer.c
 * @version 0.01
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief Source file for ring buffer implementation.
 */

#include <vmm_types.h>
#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_heap.h>
#include <vmm_string.h>
#include <vmm_ringbuffer.h>

void *vmm_ringbuffer_init(u32 size)
{
	rb_info_t *rb;
	rb = vmm_malloc(sizeof(rb_info_t));
	if (!rb) {
		return NULL;
	}

	rb->rb_data = vmm_malloc(size);
	if (!rb->rb_data) {
		goto rb_data_fail;
	}

	/* Simultaneous read/writes are lock free. Updates need lock */
	INIT_SPIN_LOCK(&rb->head_lock);
	INIT_SPIN_LOCK(&rb->tail_lock);
	rb->head = 0;
	rb->tail = 0;
	rb->overrun = 0;
	rb->rb_size = size;

	return (void *)rb;

rb_data_fail:
	vmm_free(rb);
	return NULL;
}

u32 vmm_ringbuffer_write(void *handle, void *data, u32 len)
{
	rb_info_t *rb = (rb_info_t *) handle;

	/* latch the tail */
	u32 ctail = rb->tail, wrapped = 0;
	u32 behind = (rb->head < ctail ? 1 : 0);

	vmm_spin_lock(&rb->head_lock);

	if ((rb->rb_size - rb->head) > len) {
		vmm_memcpy(rb->rb_data + rb->head, data, len);
		rb->head += len;
		rb->head = (rb->head >= rb->rb_size ? 0 : rb->head);
	} else {
		wrapped = 1;
		vmm_memcpy(rb->rb_data + rb->head, data,
			   (rb->rb_size - rb->head));
		vmm_memcpy(rb->rb_data, data + (rb->rb_size - rb->head),
			   (len - (rb->rb_size - rb->head)));
		rb->head = len - (rb->rb_size - rb->head);
	}
	if (behind) {
		if (rb->head > ctail)
			rb->overrun++;
	} else {
		if (wrapped && rb->head > ctail)
			rb->overrun++;
	}

	vmm_spin_unlock(&rb->head_lock);

	return 0;
}

u32 vmm_ringbuffer_read(void *handle, void *dest, u32 len)
{
	rb_info_t *rb = (rb_info_t *) handle;
	u32 to_read = 0, avail;
	u32 chead;

	/* latch the head */
	chead = rb->head;

	vmm_spin_lock(&rb->tail_lock);
	if (rb->tail < rb->head) {
		/* case when head hasn't wrapped. */
		avail = chead - rb->tail;
		to_read = (avail > len ? len : avail);
		memcpy(dest, rb->rb_data + rb->tail, to_read);
		rb->tail += to_read;
	} else {
		/* case when head had wrapped around since last we read */
		vmm_memcpy(dest, rb->rb_data + rb->tail,
			   (rb->rb_size - rb->tail));
		vmm_memcpy(dest + (rb->rb_size - rb->tail), rb->rb_data,
			   (to_read - (rb->rb_size - rb->tail)));
		rb->tail = (to_read - (rb->rb_size - rb->tail));
	}
	vmm_spin_unlock(&rb->tail_lock);

	return to_read;
}

u32 vmm_ringbuffer_free(void *handle)
{
	rb_info_t *rb = (rb_info_t *) handle;
	vmm_free(rb->rb_data);
	vmm_free(rb);
	return 0;
}
