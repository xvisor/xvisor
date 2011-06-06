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
 * @file vmm_ringbuffer.h
 * @version 0.01
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 */

#ifndef __VMM_RINGBUFFER_H__
#define __VMM_RINGBUFFER_H__

#include <vmm_types.h>
#include <vmm_spinlocks.h>

typedef struct rb_info {
	u8 *rb_data;
	u32 rb_size;
	u32 overrun;
	u32 head;		/* read here. */
	u32 tail;		/* write here */
	vmm_spinlock_t head_lock;	/* lock this for head */
	vmm_spinlock_t tail_lock;	/* lock this for tail. */
} rb_info_t;

void *vmm_ringbuffer_init(u32 size);
u32 vmm_ringbuffer_write(void *handle, void *data, u32 len);
u32 vmm_ringbuffer_read(void *handle, void *dest, u32 len);
u32 vmm_ringbuffer_free(void *handle);

#endif /* __VMM_RINGBUFFER_H__ */
