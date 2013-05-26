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
 * @file fifo.h
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for generic first-in-first-out queue.
 */

#ifndef __FIFO_H__
#define __FIFO_H__

#include <vmm_types.h>
#include <vmm_spinlocks.h>

/** FIFO representation */
struct fifo {
	void *elements;
	u32 element_size;
	u32 element_count;
	vmm_spinlock_t lock;
	u32 read_pos;
	u32 write_pos;
	u32 avail_count;
};

/** Alloc a new FIFO */
struct fifo *fifo_alloc(u32 element_size, u32 element_count);

/** Free a FIFO */
int fifo_free(struct fifo *f);

/** Check if FIFO is empty */
bool fifo_isempty(struct fifo *f);

/** Check if FIFO is full */
bool fifo_isfull(struct fifo *f);

/** Enqueue an element to FIFO
 *  @returns TRUE on success and FALSE on failure
 */
bool fifo_enqueue(struct fifo *f, void *src, bool overwrite);

/** Dequeue an element from FIFO
 *  @returns TRUE on success and FALSE on failure
 */
bool fifo_dequeue(struct fifo *f, void *dst);

/** Get element from given logical index
 *  @returns TRUE on success and FALSE on failure
 */
bool fifo_getelement(struct fifo *f, u32 index, void *dst);

/** Get count of available elements */
u32 fifo_avail(struct fifo *f);

#endif /* __FIFO_H__ */
