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
 * @author Himanshu Chauhan (hchauhan@nulltrace.org)
 * @brief header file for buddy heap allocator
 */

#ifndef __VMM_BUDDY_H_
#define __VMM_BUDDY_H_

#include <vmm_chardev.h>

#define HOUSE_KEEPING_PERCENT	(CONFIG_BUDDY_HOUSE_KEEPING_PERCENT)
#define MIN_BLOCK_SIZE		(0x01UL << CONFIG_BUDDY_MIN_BLOCK_SIZE_SHIFT)	/* Minimum alloc of bus width */
#define MAX_BLOCK_SIZE		(0x01UL << CONFIG_BUDDY_MAX_BLOCK_SIZE_SHIFT)	/* Maximum alloc of bus width */
#define BINS_MAX_ORDER		(CONFIG_BUDDY_MAX_BLOCK_SIZE_SHIFT - CONFIG_BUDDY_MIN_BLOCK_SIZE_SHIFT + 1)

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
	struct vmm_free_area * hk_fn_array;
	unsigned int hk_fn_count;
	struct vmm_alloced_area * hk_an_array;
	unsigned int hk_an_count;
	struct vmm_alloced_area current;
	void *mem_start;
	unsigned int mem_size;
	void *heap_start;
	unsigned int heap_size;
	struct vmm_free_area free_area[BINS_MAX_ORDER];	/* Bins holding free area. */
} __attribute__ ((packed));

int buddy_init(void *heap_start, unsigned int heap_size);
void *buddy_malloc(unsigned int size);
void *buddy_zalloc(unsigned int size);
void buddy_free(void *ptr);
void buddy_print_state(struct vmm_chardev *cdev);
void buddy_print_hk_state(struct vmm_chardev *cdev);

#endif /* __VMM_BUDDY_H_ */
