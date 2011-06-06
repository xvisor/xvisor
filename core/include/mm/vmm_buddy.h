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
 * @file vmm_buddy.h
 * @version 0.01
 * @author Himanshu Chauhan (hchauhan@nulltrace.org)
 * @brief header file for buddy allocator in VMM
 */

#ifndef __VMM_BUDDY_ALLOC_H_
#define __VMM_BUDDY_ALLOC_H_

#define BINS_MAX_ORDER		(CONFIG_BINS_MAX_ORDER)
#define MIN_BLOCK_SIZE		(0x01UL << CONFIG_MIN_BLOCK_SIZE_SHIFT)	/* Minimum alloc of bus width */
#define MAX_BLOCK_SIZE		(MIN_BLOCK_SIZE << (BINS_MAX_ORDER - 1))	/* Max block size of 4 KiB */

struct vmm_free_area {
	struct dlist head;
	void *map;
	unsigned int count;
} __attribute__ ((packed));

struct vmm_alloced_area {
	struct dlist head;
	void *map;
	unsigned int blk_sz;
	unsigned int bin_num;
	unsigned int count;
} __attribute__ ((packed));

struct vmm_heap {
	void *heap_start;
	void *heap_end;
	unsigned int heap_size;
	struct vmm_free_area free_area[BINS_MAX_ORDER];	/* Bins holding free area. */
} __attribute__ ((packed));

void print_current_buddy_state(void);
void print_current_hk_state(void);

#endif /* __BUDDY_ALLOC_H_ */
