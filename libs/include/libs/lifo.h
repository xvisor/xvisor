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
 * @file lifo.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for generic last-in-first-out queue.
 */

#ifndef __LIFO_H__
#define __LIFO_H__

#include <vmm_types.h>
#include <vmm_spinlocks.h>

/** LIFO representation */
struct lifo {
	void *elements;
	u32 element_size;
	u32 element_count;
	vmm_spinlock_t lock;
	u32 head_pos;
	u32 avail_count;
};

/** Alloc a new LIFO */
struct lifo *lifo_alloc(u32 element_size, u32 element_count);

/** Free a LIFO */
int lifo_free(struct lifo *l);

/** Check if LIFO is empty */
bool lifo_isempty(struct lifo *l);

/** Check if LIFO is full */
bool lifo_isfull(struct lifo *l);

/** Enqueue an element to LIFO
 *  @returns TRUE on success and FALSE on failure
 */
bool lifo_enqueue(struct lifo *l, void *src, bool overwrite);

/** Dequeue an element from LIFO
 *  @returns TRUE on success and FALSE on failure
 */
bool lifo_dequeue(struct lifo *l, void *dst);

/** Get element from given logical index
 *  @returns TRUE on success and FALSE on failure
 */
bool lifo_getelement(struct lifo *l, u32 index, void *dst);

/** Get count of available elements */
u32 lifo_avail(struct lifo *l);

#endif /* __LIFO_H__ */
