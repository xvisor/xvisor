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
 * @file vmm_buddy.c
 * @version 0.01
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief source file for buddy allocator in VMM
 */
#if TEST_BUDDY_SYSTEM
#include <stdio.h>
#include <string.h>
#endif

#if !TEST_BUDDY_SYSTEM
#include <vmm_string.h>
#endif

#include <vmm_list.h>
#include <vmm_sections.h>
#include <vmm_heap.h>
#include <mm/vmm_buddy.h>
#include <vmm_stdio.h>
#include <vmm_error.h>

#ifndef NULL
#define NULL			((void *)0)
#endif

#define _DEBUG			0

#if _DEBUG
#define VMM_DPRINTK(fmt, ...)	vmm_printf(fmt, ##__VA_ARGS__)
#else
#define VMM_DPRINTK(fmt, ...)
#endif

#define IS_POW_TWO(x)		(x && !(x & (x - 1)))
#define HEAP_FN_HK_LEN		(vmm_mm_hk_size()/2)
#define HEAP_AN_HK_LEN		(vmm_mm_hk_size()/2)

static struct vmm_heap buddy_heap;
static struct vmm_alloced_area current_allocations;
struct vmm_free_area *free_node_list;
struct vmm_alloced_area *alloced_node_list;

static struct vmm_free_area *get_free_hk_node()
{
	int idx = 0;
	struct vmm_free_area *fren = free_node_list;

	for (idx = 0; idx < (HEAP_FN_HK_LEN / sizeof(struct vmm_free_area));
	     idx++) {
		if (fren->map) {
			fren++;
		} else {
			return fren;
		}
	}

	return NULL;
}

static void free_hk_node(struct vmm_free_area *node)
{
	node->map = NULL;
}

static struct vmm_alloced_area *get_free_ac_node()
{
	int idx = 0;
	struct vmm_alloced_area *fren = alloced_node_list;

	for (idx = 0; idx < (HEAP_AN_HK_LEN / sizeof(struct vmm_alloced_area));
	     idx++) {
		if (fren->map) {
			fren++;
		} else {
			return fren;
		}
	}

	return NULL;
}

static int buddy_init_max_blocks(void)
{
	void *heap_start = buddy_heap.heap_start;
	unsigned int heap_size = buddy_heap.heap_size;
	struct vmm_free_area *freenode;
	int tnodes = 0;

	INIT_LIST_HEAD(&buddy_heap.free_area[BINS_MAX_ORDER - 1].head);

	while (heap_size) {
		freenode = get_free_hk_node();
		if (!freenode) {
			return -1;
		}
		freenode->map = heap_start;
		heap_size -= MAX_BLOCK_SIZE;
		heap_start += MAX_BLOCK_SIZE;
		list_add_tail(&buddy_heap.free_area[BINS_MAX_ORDER - 1].head,
			      &freenode->head);
		buddy_heap.free_area[BINS_MAX_ORDER - 1].count++;
		tnodes++;
	}

	VMM_DPRINTK("Total: %d nodes of size 0x%X added to last bin.\n",
		    tnodes, MAX_BLOCK_SIZE);

	return 0;
}

int buddy_init(void *heap_start, unsigned int heap_size)
{
	int cntr = 0;

	/* We manage heap space only in power of two */
	if (!IS_POW_TWO(heap_size)) {
		return VMM_EFAIL;
	}

	/* keep 4K of initial heap area for our housekeeping */
	free_node_list = (struct vmm_free_area *)vmm_mm_hk_start();
	alloced_node_list =
	    (struct vmm_alloced_area *)(vmm_mm_hk_start() + HEAP_FN_HK_LEN);
	vmm_memset(free_node_list, 0, HEAP_FN_HK_LEN + HEAP_AN_HK_LEN);

	buddy_heap.heap_start = heap_start;
	buddy_heap.heap_size = heap_size;
	buddy_heap.heap_end = buddy_heap.heap_start + heap_size;
	vmm_memset(&buddy_heap.free_area[0], 0, sizeof(buddy_heap.free_area));
	for (cntr = 0; cntr < BINS_MAX_ORDER; cntr++) {
		INIT_LIST_HEAD(&buddy_heap.free_area[cntr].head);
	}
	buddy_init_max_blocks();
	INIT_LIST_HEAD(&current_allocations.head);

	return 0;
}

static struct vmm_free_area *buddy_get_contiguous_block(unsigned int num_blocks,
							unsigned int idx)
{
	struct vmm_free_area *snode = NULL, *cnode = NULL, *pnode = NULL;
	struct dlist *cnhead;
	unsigned int count = 0;

	if (idx >= BINS_MAX_ORDER) {
		return NULL;
	}

	if (list_empty(&buddy_heap.free_area[idx].head)) {
		return NULL;
	}

	/* First check if we have enough nodes, contiguous or non-contiguous. */
	if (buddy_heap.free_area[idx].count >= num_blocks) {
		/* okay we have enough nodes. Now try allocation contiguous nodes */
		list_for_each(cnhead, &buddy_heap.free_area[idx].head) {
			cnode = list_entry(cnhead, struct vmm_free_area, head);
			if (cnode) {
				if (snode == NULL) {
					snode = cnode;
				}
				if (pnode) {
					if (pnode->map + MAX_BLOCK_SIZE ==
					    cnode->map) {
						pnode = cnode;
						if (++count == num_blocks) {
							goto cont_blocks_found;
						}
						continue;
					}
					snode = NULL;
				} else {
					pnode = cnode;
				}
			}
		}
cont_blocks_found:
		if (snode) {
			cnode = get_free_hk_node();
			BUG_ON(!cnode,
			       "Panic: No free house keeping nodes for buddy allocator!\n");

			memcpy(cnode, snode, sizeof(struct vmm_free_area));
			while (count) {
				/*
				 * Latch the next node to be released because
				 * after list_del information in snode will be gone.
				 */
				pnode =
				    list_entry(snode->head.next,
					       struct vmm_free_area, head);
				list_del(&snode->head);
				buddy_heap.free_area[idx].count--;
				count--;
				snode = pnode;
			}
			if (buddy_heap.free_area[idx].count == 0) {
				INIT_LIST_HEAD(&buddy_heap.free_area[idx].head);
			}

			return cnode;
		}
	}

	return NULL;
}

static struct vmm_free_area *buddy_get_block(unsigned int idx)
{

	struct vmm_free_area *farea = NULL, *rarea = NULL;
	struct dlist *lm;
	int blk_sz;

	if (idx >= BINS_MAX_ORDER) {
		return NULL;
	}

	if (list_empty(&buddy_heap.free_area[idx].head)) {
		/*
		 * We borrowed a block from higher order. keep half of it and rest half
		 * give to the caller.
		 */
		/* This is what we keep for us for further allocation request. */
		farea = buddy_get_block(idx + 1);
		if (farea) {
			rarea = get_free_hk_node();
			if (rarea) {
				blk_sz = MIN_BLOCK_SIZE << idx;
				list_add_tail(&buddy_heap.free_area[idx].head,
					      &farea->head);
				buddy_heap.free_area[idx].count++;

				/* this is our buddy we will give to caller */
				rarea->map = farea->map + blk_sz;
			}
		}
	} else {
		lm = list_pop_tail(&buddy_heap.free_area[idx].head);
		buddy_heap.free_area[idx].count--;
		if (buddy_heap.free_area[idx].count == 0) {
			INIT_LIST_HEAD(&buddy_heap.free_area[idx].head);
		}
		rarea = list_entry(lm, struct vmm_free_area, head);
	}

	return rarea;
}

static struct vmm_alloced_area *search_for_allocated_block(void *addr)
{
	struct vmm_alloced_area *cnode;
	struct dlist *cnhead;

	list_for_each(cnhead, &current_allocations.head) {
		cnode = list_entry(cnhead, struct vmm_alloced_area, head);
		if (cnode && cnode->map == addr) {
			return cnode;
		}
	}

	return NULL;
}

static int add_free_area_to_bin(struct vmm_free_area *free_area,
				unsigned int bin_num)
{
	struct vmm_free_area *carea = NULL;
	struct dlist *pos;

	if (list_empty(&buddy_heap.free_area[bin_num].head)) {
		list_add(&buddy_heap.free_area[bin_num].head, &free_area->head);
	} else {
		list_for_each(pos, &buddy_heap.free_area[bin_num].head) {
			carea = list_entry(pos, struct vmm_free_area, head);
			if (carea->map > free_area->map) {
				break;
			}
		}
		list_add(&carea->head, &free_area->head);
	}
	buddy_heap.free_area[bin_num].count++;

	return 0;
}

static int coalesce_buddies(unsigned int bin)
{
	struct dlist *pos;
	struct vmm_free_area *lfa = NULL, *cfa = NULL;
	void *lmap = NULL;

restart:
	if (bin == BINS_MAX_ORDER - 1) {
		return 0;
	}

	list_for_each(pos, &buddy_heap.free_area[bin].head) {
		cfa = list_entry(pos, struct vmm_free_area, head);
		if (lmap) {
			if ((lmap + (MIN_BLOCK_SIZE << bin)) == cfa->map) {
				VMM_DPRINTK
				    ("Coalescing 0x%X and 0x%X and giving back to bin %d\n",
				     (unsigned int)lfa->map,
				     (unsigned int)cfa->map, bin + 1);
				list_del(&cfa->head);
				list_del(&lfa->head);
				vmm_memset(cfa, 0,
					   sizeof(struct vmm_free_area));
				add_free_area_to_bin(lfa, bin + 1);
				lmap = NULL;
				lfa = NULL;
				cfa = NULL;
				buddy_heap.free_area[bin].count -= 2;

				/* restart the list afresh */
				goto restart;
			} else {
				lmap = cfa->map;
				lfa = cfa;
			}
		} else {
			lmap = cfa->map;
			lfa = cfa;
		}
	}

	coalesce_buddies(bin + 1);

	return 0;
}

static int return_to_pool(struct vmm_alloced_area *aarea)
{
	struct vmm_free_area *free_area = NULL;
	int bin_num = aarea->bin_num;

	free_area = get_free_hk_node();

	if (free_area) {
		free_area->map = aarea->map;
		add_free_area_to_bin(free_area, bin_num);
		if (buddy_heap.free_area[bin_num].count > 1) {
			coalesce_buddies(bin_num);
		}
	} else {
		return -1;
	}

	return 0;
}

void *buddy_malloc(unsigned int size)
{
	int idx = 0;
	int curr_blk = MIN_BLOCK_SIZE;
	struct vmm_free_area *farea = NULL;
	struct vmm_alloced_area *aarea = NULL;
	u32 bneeded;

	if (size > buddy_heap.heap_size) {
		return NULL;
	}

	if (size > MAX_BLOCK_SIZE) {
		bneeded =
		    (size %
		     MAX_BLOCK_SIZE ? ((size / MAX_BLOCK_SIZE) +
				       1) : size / MAX_BLOCK_SIZE);
		farea = buddy_get_contiguous_block(bneeded, BINS_MAX_ORDER - 1);
		if (farea) {
			aarea = get_free_ac_node();

			BUG_ON(!aarea,
			       "Panic: No house keeping node available for buddy allocator.!\n");

			aarea->map = farea->map;
			aarea->blk_sz = MAX_BLOCK_SIZE * bneeded;
			aarea->bin_num = BINS_MAX_ORDER - 1;
			list_add_tail(&current_allocations.head, &aarea->head);
			current_allocations.count++;
			free_hk_node(farea);
			return aarea->map;
		}
	}

	for (idx = 0; idx <= BINS_MAX_ORDER; idx++) {
		if (size > curr_blk) {
			curr_blk <<= 1;
		} else {
			farea = buddy_get_block(idx);
			if (farea) {
				aarea = get_free_ac_node();
				if (!aarea) {
					VMM_DPRINTK
					    ("Bummer! No free alloc node?\n");
					return NULL;
				}
				aarea->map = farea->map;
				aarea->blk_sz = curr_blk;
				aarea->bin_num = idx;
				vmm_memset(farea, 0,
					   sizeof(struct vmm_free_area));
				free_hk_node(farea);
				list_add_tail(&current_allocations.head,
					      &aarea->head);
				current_allocations.count++;
				return aarea->map;
			}
		}
	}

	return NULL;
}

void *buddy_zalloc(unsigned int size)
{
	void *ptr = buddy_malloc(size);

	if (ptr) {
		vmm_memset(ptr, 0, size);
	}

	return ptr;
}

void buddy_free(void *ptr)
{
	/* FIXME: Handle the freeing of contiguously allocated nodes. */
	struct vmm_alloced_area *freed_node = search_for_allocated_block(ptr);
	if (!freed_node) {
		VMM_DPRINTK("Bugger! No allocations found for address %X\n",
			    (unsigned int)ptr);
		return;
	}

	VMM_DPRINTK("Freeing 0x%X of block size: %d bin: %d\n",
		    (unsigned int)ptr, freed_node->blk_sz, freed_node->bin_num);
	return_to_pool(freed_node);
	list_del(&freed_node->head);
	vmm_memset(freed_node, 0, sizeof(struct vmm_alloced_area));
}

void print_current_buddy_state(void)
{
	int idx = 0;
	struct vmm_free_area *varea;
	struct vmm_alloced_area *valloced;
	struct dlist *pos;
	int bfree = 0, balloced = 0;

	vmm_printf("Heap size: %d KiB\n", (buddy_heap.heap_size / 1024));

	for (idx = 0; idx < BINS_MAX_ORDER; idx++) {
		vmm_printf("[BLOCK 0x%4X]: ", MIN_BLOCK_SIZE << idx);
		list_for_each(pos, &buddy_heap.free_area[idx].head) {
			varea = list_entry(pos, struct vmm_free_area, head);
			bfree++;
		}
		list_for_each(pos, &current_allocations.head) {
			valloced =
			    list_entry(pos, struct vmm_alloced_area, head);
			if (valloced->bin_num == idx) {
				balloced++;
			}
		}
		vmm_printf("%5d alloced, %5d free block(s)\n", balloced, bfree);
		bfree = 0;
		balloced = 0;
	}
}

void print_current_hk_state(void)
{
	u32 free = 0, idx;
	struct vmm_free_area *fren = free_node_list;
	struct vmm_alloced_area *acn = alloced_node_list;

	for (idx = 0; idx < (HEAP_FN_HK_LEN / sizeof(struct vmm_free_area));
	     idx++) {
		if (!fren->map) {
			free++;
			fren++;
		}
	}

	vmm_printf("Free Node List: %d nodes free out of %d.\n", free,
		   (HEAP_FN_HK_LEN / sizeof(struct vmm_free_area)));

	free = 0;
	for (idx = 0; idx < (HEAP_AN_HK_LEN / sizeof(struct vmm_alloced_area));
	     idx++) {
		if (!acn->map) {
			free++;
			acn++;
		}
	}
	vmm_printf("Alloced Node List: %d nodes free out of %d.\n", free,
		   (HEAP_FN_HK_LEN / sizeof(struct vmm_alloced_area)));
}

void *vmm_malloc(virtual_size_t size)
{
	return buddy_malloc(size);
}

void vmm_free(void *pointer)
{
	buddy_free(pointer);
}

int vmm_heap_init(void)
{
	return buddy_init((void *)vmm_heap_start(), vmm_heap_size());
}
