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
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief source file for buddy heap allocator
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_stdio.h>
#include <vmm_spinlocks.h>
#include <vmm_host_aspace.h>
#include <vmm_cache.h>
#include <list.h>
#include <stringlib.h>

#ifndef NULL
#define NULL			((void *)0)
#endif

#define _DEBUG			0

#if _DEBUG
#define DPRINTF(fmt, ...)	vmm_printf(fmt, ##__VA_ARGS__)
#else
#define DPRINTF(fmt, ...)
#endif

#define IS_POW_TWO(x)		(x && !(x & (x - 1)))

#define HOUSE_KEEPING_PERCENT	(25) /* 25 per-cent for house keeping */
#define MIN_BLOCK_SHIFT		(VMM_CACHE_LINE_SHIFT)
#define MAX_BLOCK_SHIFT		(VMM_PAGE_SHIFT)
#define MIN_BLOCK_SIZE		(0x01UL << MIN_BLOCK_SHIFT)	/* Minimum alloc of bus width */
#define MAX_BLOCK_SIZE		(0x01UL << MAX_BLOCK_SHIFT)	/* Maximum alloc of bus width */
#define BINS_MAX_ORDER		(MAX_BLOCK_SHIFT - MIN_BLOCK_SHIFT + 1)

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

struct vmm_buddy_heap {
	vmm_spinlock_t lock;
	struct vmm_free_area *hk_fn_array;
	unsigned int hk_fn_count;
	struct vmm_alloced_area *hk_an_array;
	unsigned int hk_an_count;
	struct vmm_alloced_area current;
	void *mem_start;
	unsigned int mem_size;
	void *heap_start;
	unsigned int heap_size;
	struct vmm_free_area free_area[BINS_MAX_ORDER];	/* Bins holding free area. */
} __attribute__ ((packed));

static struct vmm_buddy_heap bheap;

static struct vmm_free_area *get_free_hk_node()
{
	int idx = 0;
	struct vmm_free_area *fren = bheap.hk_fn_array;

	for (idx = 0; idx < bheap.hk_fn_count; idx++) {
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
	struct vmm_alloced_area *fren = bheap.hk_an_array;

	for (idx = 0; idx < bheap.hk_an_count; idx++) {
		if (fren->map) {
			fren++;
		} else {
			return fren;
		}
	}

	return NULL;
}

static struct vmm_free_area *buddy_get_contiguous_block(unsigned int num_blocks,
							unsigned int idx)
{
	struct vmm_free_area *snode = NULL, *cnode = NULL, *pnode = NULL;
	struct dlist *l;
	unsigned int count = 0;

	if (idx >= BINS_MAX_ORDER) {
		return NULL;
	}

	if (list_empty(&bheap.free_area[idx].head)) {
		return NULL;
	}

	/* First check if we have enough nodes, contiguous or non-contiguous. */
	if (bheap.free_area[idx].count >= num_blocks) {
		/* okay we have enough nodes. Now try allocation contiguous nodes */
		list_for_each(l, &bheap.free_area[idx].head) {
			cnode = list_entry(l, struct vmm_free_area, head);
			if (snode == NULL) {
				snode = cnode;
				count = 0;
			}
			if (pnode) {
				if ((pnode->map + MAX_BLOCK_SIZE) == cnode->map) {
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
 cont_blocks_found:
		if (snode) {
			cnode = get_free_hk_node();
			BUG_ON(!cnode);
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
				free_hk_node(snode);
				bheap.free_area[idx].count--;
				count--;
				snode = pnode;
			}
			if (bheap.free_area[idx].count == 0) {
				INIT_LIST_HEAD(&bheap.free_area[idx].head);
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

	if (list_empty(&bheap.free_area[idx].head)) {
		/* We borrowed a block from higher order. 
		 * keep half of it and rest half give to the caller.
		 */
		farea = buddy_get_block(idx + 1);
		if (farea) {
			rarea = get_free_hk_node();
			if (rarea) {
				blk_sz = MIN_BLOCK_SIZE << idx;
				list_add_tail(&farea->head,
					      &bheap.free_area[idx].head);
				bheap.free_area[idx].count++;
				/* this is our buddy we will give to caller */
				rarea->map = farea->map + blk_sz;
			}
		}
	} else {
		lm = list_pop_tail(&bheap.free_area[idx].head);
		bheap.free_area[idx].count--;
		if (bheap.free_area[idx].count == 0) {
			INIT_LIST_HEAD(&bheap.free_area[idx].head);
		}
		rarea = list_entry(lm, struct vmm_free_area, head);
	}

	return rarea;
}

static struct vmm_alloced_area *search_for_allocated_block(void *addr)
{
	struct vmm_alloced_area *cnode;
	struct dlist *cnhead;

	list_for_each(cnhead, &bheap.current.head) {
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
	bool added = FALSE;
	struct vmm_free_area *carea = NULL;
	struct dlist *pos;

	if (list_empty(&bheap.free_area[bin_num].head)) {
		list_add(&free_area->head, &bheap.free_area[bin_num].head);
	} else {
		list_for_each(pos, &bheap.free_area[bin_num].head) {
			carea = list_entry(pos, struct vmm_free_area, head);
			if (free_area->map < carea->map) {
				list_add_tail(&free_area->head, &carea->head);
				added = TRUE;
				break;
			}
		}
		if (!added) {
			list_add_tail(&free_area->head,
				      &bheap.free_area[bin_num].head);
		}
	}
	bheap.free_area[bin_num].count++;

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

	list_for_each(pos, &bheap.free_area[bin].head) {
		cfa = list_entry(pos, struct vmm_free_area, head);
		if (lmap) {
			if ((lmap + (MIN_BLOCK_SIZE << bin)) == cfa->map) {
				DPRINTF
				    ("Coalescing 0x%X and 0x%X to bin %d\n",
				     (unsigned int)lfa->map,
				     (unsigned int)cfa->map, bin + 1);
				list_del(&cfa->head);
				list_del(&lfa->head);
				memset(cfa, 0, sizeof(struct vmm_free_area));
				add_free_area_to_bin(lfa, bin + 1);
				lmap = NULL;
				lfa = NULL;
				cfa = NULL;
				bheap.free_area[bin].count -= 2;

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
		if (bheap.free_area[bin_num].count > 1) {
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
	irq_flags_t flags;
	struct vmm_free_area *farea = NULL;
	struct vmm_alloced_area *aarea = NULL;
	u32 bneeded;

	vmm_spin_lock_irqsave(&bheap.lock, flags);

	if (size > bheap.heap_size) {
		vmm_spin_unlock_irqrestore(&bheap.lock, flags);
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
			BUG_ON(!aarea);
			aarea->map = farea->map;
			aarea->blk_sz = MAX_BLOCK_SIZE * bneeded;
			aarea->bin_num = BINS_MAX_ORDER - 1;
			list_add_tail(&aarea->head, &bheap.current.head);
			bheap.current.count++;
			free_hk_node(farea);
			vmm_spin_unlock_irqrestore(&bheap.lock, flags);
			return aarea->map;
		}
	}

	for (idx = 0; idx < BINS_MAX_ORDER; idx++) {
		if (size > curr_blk) {
			curr_blk <<= 1;
		} else {
			farea = buddy_get_block(idx);
			if (farea) {
				aarea = get_free_ac_node();
				BUG_ON(!aarea);
				aarea->map = farea->map;
				aarea->blk_sz = curr_blk;
				aarea->bin_num = idx;
				memset(farea, 0, sizeof(struct vmm_free_area));
				free_hk_node(farea);
				list_add_tail(&aarea->head,
					      &bheap.current.head);
				bheap.current.count++;
				vmm_spin_unlock_irqrestore(&bheap.lock, flags);
				return aarea->map;
			}
		}
	}

	vmm_spin_unlock_irqrestore(&bheap.lock, flags);

	return NULL;
}

void buddy_free(void *ptr)
{
	u32 aarea_sz;
	irq_flags_t flags;
	struct vmm_alloced_area *aarea;

	vmm_spin_lock_irqsave(&bheap.lock, flags);

	aarea = search_for_allocated_block(ptr);
	BUG_ON(!aarea);

	if (MAX_BLOCK_SIZE < aarea->blk_sz) {
		aarea_sz = aarea->blk_sz;
		aarea->blk_sz = MAX_BLOCK_SIZE;
		while (MAX_BLOCK_SIZE <= aarea_sz) {
			return_to_pool(aarea);
			aarea->map += MAX_BLOCK_SIZE;
			aarea_sz -= MAX_BLOCK_SIZE;
		}
	} else {
		return_to_pool(aarea);
	}

	list_del(&aarea->head);
	memset(aarea, 0, sizeof(struct vmm_alloced_area));

	vmm_spin_unlock_irqrestore(&bheap.lock, flags);
}

int buddy_init(void *heap_start, unsigned int heap_size)
{
	int cntr = 0, tnodes = 0;
	void *mem_start;
	unsigned int mem_size, hk_total_size;
	struct vmm_free_area *freenode = NULL;

	/* We manage heap space only in power of two */
	if (!IS_POW_TWO(heap_size)) {
		return VMM_EFAIL;
	}

	/* initialize lock */
	INIT_SPIN_LOCK(&bheap.lock);

	hk_total_size = (heap_size * HOUSE_KEEPING_PERCENT) / 100;

	/* keep 4K of initial heap area for our housekeeping */
	bheap.hk_fn_array = (struct vmm_free_area *)heap_start;
	bheap.hk_fn_count =
	    (hk_total_size / 2) / sizeof(struct vmm_free_area);
	bheap.hk_an_array =
	    (struct vmm_alloced_area *)(heap_start + (hk_total_size / 2));
	bheap.hk_an_count =
	    (hk_total_size / 2) / sizeof(struct vmm_alloced_area);
	memset(bheap.hk_fn_array, 0, hk_total_size);

	INIT_LIST_HEAD(&bheap.current.head);

	mem_start = heap_start + hk_total_size;
	mem_size = heap_size - hk_total_size;
	bheap.mem_start = mem_start;
	bheap.mem_size = mem_size;
	bheap.heap_start = heap_start;
	bheap.heap_size = heap_size;
	memset(&bheap.free_area[0], 0, sizeof(bheap.free_area));
	for (cntr = 0; cntr < BINS_MAX_ORDER; cntr++) {
		INIT_LIST_HEAD(&bheap.free_area[cntr].head);
	}

	while (mem_size) {
		freenode = get_free_hk_node();
		if (!freenode) {
			return -1;
		}
		freenode->map = mem_start;
		mem_size -= MAX_BLOCK_SIZE;
		mem_start += MAX_BLOCK_SIZE;
		list_add_tail(&freenode->head,
			      &bheap.free_area[BINS_MAX_ORDER - 1].head);
		bheap.free_area[BINS_MAX_ORDER - 1].count++;
		tnodes++;
	}
	DPRINTF("Total: %d nodes of size 0x%X added to last bin.\n",
		tnodes, MAX_BLOCK_SIZE);

	return 0;
}

void *vmm_malloc(virtual_size_t size)
{
	return buddy_malloc(size);
}

void *vmm_zalloc(virtual_size_t size)
{
	void *ret = buddy_malloc(size);

	if (ret) {
		memset(ret, 0, size);
	}

	return ret;
}

void vmm_free(void *pointer)
{
	buddy_free(pointer);
}

int vmm_heap_allocator_name(char *name, int name_sz)
{
	if (!name && name_sz > 0) {
		return VMM_EFAIL;
	}

	strncpy(name, "Buddy System", name_sz);

	return VMM_OK;
}

virtual_addr_t vmm_heap_start_va(void)
{
	return (virtual_addr_t) bheap.heap_start;
}

virtual_size_t vmm_heap_size(void)
{
	return (virtual_size_t) bheap.heap_size;
}

virtual_size_t vmm_heap_hksize(void)
{
	return (virtual_size_t) (bheap.heap_size - bheap.mem_size);
}

int vmm_heap_print_state(struct vmm_chardev *cdev)
{
	int idx = 0;
	irq_flags_t flags;
	struct vmm_free_area *fren = bheap.hk_fn_array;
	struct vmm_alloced_area *valloced, *acn = bheap.hk_an_array;
	struct dlist *pos;
	int free = 0, bfree = 0, balloced = 0;

	vmm_cprintf(cdev, "Heap State\n");

	vmm_spin_lock_irqsave(&bheap.lock, flags);

	for (idx = 0; idx < BINS_MAX_ORDER; idx++) {
		vmm_cprintf(cdev, "  [BLOCK 0x%4X]: ", MIN_BLOCK_SIZE << idx);
		list_for_each(pos, &bheap.free_area[idx].head) {
			bfree++;
		}
		list_for_each(pos, &bheap.current.head) {
			valloced =
			    list_entry(pos, struct vmm_alloced_area, head);
			if (valloced->bin_num == idx) {
				balloced++;
			}
		}
		vmm_cprintf(cdev, "%5d alloced, %5d free block(s)\n",
			    balloced, bfree);
		bfree = 0;
		balloced = 0;
	}

	vmm_cprintf(cdev, "House-Keeping State\n");

	for (idx = 0; idx < bheap.hk_fn_count; idx++) {
		if (!fren->map) {
			free++;
		}
		fren++;
	}

	vmm_cprintf(cdev, "  Free Node List: %d nodes free out of %d\n",
		    free, bheap.hk_fn_count);

	free = 0;
	for (idx = 0; idx < bheap.hk_an_count; idx++) {
		if (!acn->map) {
			free++;
		}
		acn++;
	}
	vmm_cprintf(cdev, "  Allocated Node List: %d nodes free out of %d\n",
		    free, bheap.hk_an_count);

	vmm_spin_unlock_irqrestore(&bheap.lock, flags);

	return VMM_OK;
}

int __init vmm_heap_init(void)
{
	u32 heap_size, heap_page_count, heap_mem_flags;
	void *heap_start;

	heap_size = CONFIG_HEAP_SIZE * 1024;
	heap_page_count = VMM_SIZE_TO_PAGE(heap_size);
	heap_mem_flags =
	    (VMM_MEMORY_READABLE | VMM_MEMORY_WRITEABLE | VMM_MEMORY_CACHEABLE |
	     VMM_MEMORY_BUFFERABLE);

	heap_start =
	    (void *)vmm_host_alloc_pages(heap_page_count, heap_mem_flags);

	if (!heap_start) {
		return VMM_EFAIL;
	}

	return buddy_init(heap_start, heap_size);
}
