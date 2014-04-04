/**
 * Copyright (c) 2014 Anup Patel.
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
 * @file buddy.c
 * @author Anup Patel (anup@brainfault.org)
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief Implementation of buddy allocator library
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <libs/stringlib.h>
#include <libs/buddy.h>

#undef DEBUG

#ifdef DEBUG
#define DPRINTF(fmt, ...)		vmm_printf(fmt, ##__VA_ARGS__)
#else
#define DPRINTF(fmt, ...)
#endif

#define BLOCK_SIZE(bin_num)		(0x01UL << (bin_num))
#define BLOCK_MASK(bin_num)		(BLOCK_SIZE(bin_num) - 0x01UL)
#define BLOCK_COUNT(size, bin_num)	((size) >> (bin_num))
#define BLOCK_MIN_SIZE(ba)		BLOCK_SIZE((ba)->min_bin)
#define BLOCK_MAX_SIZE(ba)		BLOCK_SIZE((ba)->max_bin)

/** House-keeping structure for contiguous blocks belonging to a bin */
struct buddy_area {
	struct dlist hk_head;
	struct rb_node hk_rb;
	unsigned long map;
	unsigned long blk_count;
	unsigned long bin_num;
};

#define AREA_SIZE(a)			((a)->blk_count * BLOCK_SIZE((a)->bin_num))
#define AREA_START(a)			((a)->map)
#define AREA_END(a)			((a)->map + AREA_SIZE(a))

unsigned long buddy_estimate_bin(struct buddy_allocator *ba,
				 unsigned long size)
{
	unsigned long bin, ret;

	if (!ba) {
		return 0;
	}

	if (size < BLOCK_SIZE(ba->min_bin)) {
		return ba->min_bin;
	}

	ret = ba->max_bin;
	for (bin = ba->min_bin; bin < ba->max_bin; bin++) {
		if (size <= BLOCK_SIZE(bin)) {
			ret = bin;
			break;
		}
	}

	return ret;
}

static void buddy_hk_free(struct buddy_allocator *ba, struct buddy_area *a)
{
	irq_flags_t f;

	if (!ba || !a) {
		return;
	}

	vmm_spin_lock_irqsave_lite(&ba->hk_free_lock, f);
	list_add_tail(&a->hk_head, &ba->hk_free_list);
	ba->hk_free_count++;
	vmm_spin_unlock_irqrestore_lite(&ba->hk_free_lock, f);
}

static struct buddy_area *buddy_hk_alloc(struct buddy_allocator *ba,
					 unsigned long map,
					 unsigned long bin_num,
					 unsigned long blk_count)
{
	irq_flags_t f;
	struct dlist *l;
	struct buddy_area *a = NULL;

	if (!ba) {
		return NULL;
	}

	vmm_spin_lock_irqsave_lite(&ba->hk_free_lock, f);

	if (!list_empty(&ba->hk_free_list)) {
		l = list_pop(&ba->hk_free_list);
		a = list_entry(l, struct buddy_area, hk_head);
		RB_CLEAR_NODE(&a->hk_rb);
		a->map = map;
		a->blk_count = blk_count;
		a->bin_num = bin_num;
		ba->hk_free_count--;
	}

	vmm_spin_unlock_irqrestore_lite(&ba->hk_free_lock, f);

	return a;
}

unsigned long buddy_hk_area_free(struct buddy_allocator *ba)
{
	irq_flags_t f;
	unsigned long ret;

	if (!ba) {
		return 0;
	}

	vmm_spin_lock_irqsave_lite(&ba->hk_free_lock, f);
	ret = ba->hk_free_count;
	vmm_spin_unlock_irqrestore_lite(&ba->hk_free_lock, f);

	return ret;
}

unsigned long buddy_hk_area_total(struct buddy_allocator *ba)
{
	if (!ba) {
		return 0;
	}

	return ba->hk_total_count;
}

static struct buddy_area *buddy_alloc_find(struct buddy_allocator *ba,
					   unsigned long addr,
					   unsigned long *alloc_map,
					   unsigned long *alloc_bin,
					   unsigned long *alloc_blk_count)
{
	irq_flags_t f;
	struct rb_node *n = ba->alloc.rb_node;

	if (!ba) {
		return NULL;
	}

	DPRINTF("%s: ba=%p addr=0x%x\n", __func__, ba, addr);

	vmm_spin_lock_irqsave_lite(&ba->alloc_lock, f);

	n = ba->alloc.rb_node;
  	while (n) {
  		struct buddy_area *a = rb_entry(n, struct buddy_area, hk_rb);

		if ((AREA_START(a) <= addr) && (addr < AREA_END(a))) {
			if (alloc_map) {
				*alloc_map = a->map;
			}
			if (alloc_bin) {
				*alloc_bin = a->bin_num;
			}
			if (alloc_blk_count) {
				*alloc_blk_count = a->blk_count;
			}

			vmm_spin_unlock_irqrestore_lite(&ba->alloc_lock, f);
			return a;
		}

		if (addr < AREA_START(a)) {
			n = n->rb_left;
		} else if (AREA_END(a) <= addr) {
			n = n->rb_right;
		} else {
			BUG_ON(1);
		}
	}

	vmm_spin_unlock_irqrestore_lite(&ba->alloc_lock, f);

	return NULL;
}

static void buddy_alloc_add(struct buddy_allocator *ba,
			    struct buddy_area *a)
{
	irq_flags_t f;
	unsigned long depth;
	struct rb_node **new = NULL, *parent = NULL;
	struct buddy_area *parent_area = NULL;

	if (!ba || !a) {
		return;
	}

	DPRINTF("%s: ba=%p map=0x%x bin_num=%d blk_count=%d\n",
		__func__, ba, a->map, a->bin_num, a->blk_count);

	vmm_spin_lock_irqsave_lite(&ba->alloc_lock, f);

	depth = 0;
	new = &(ba->alloc.rb_node);
	while (*new) {
		parent = *new;
		parent_area = rb_entry(parent, struct buddy_area, hk_rb);
		if (AREA_END(a) <= AREA_START(parent_area)) {
			new = &parent->rb_left;
		} else if (AREA_END(parent_area) <= AREA_START(a)) {
			new = &parent->rb_right;
		} else {
			BUG_ON(1);
		}
		depth++;
	}

	rb_link_node(&a->hk_rb, parent, new);
	rb_insert_color(&a->hk_rb, &ba->alloc);

	vmm_spin_unlock_irqrestore_lite(&ba->alloc_lock, f);
}

static void buddy_alloc_del(struct buddy_allocator *ba,
			    struct buddy_area *a)
{
	irq_flags_t f;

	if (!ba || !a) {
		return;
	}

	DPRINTF("%s: ba=%p map=0x%x bin_num=%d blk_count=%d\n",
		__func__, ba, a->map, a->bin_num, a->blk_count);

	vmm_spin_lock_irqsave_lite(&ba->alloc_lock, f);

	rb_erase(&a->hk_rb, &ba->alloc);

	vmm_spin_unlock_irqrestore_lite(&ba->alloc_lock, f);
}

/* NOTE: Don't call this function directly */
static struct buddy_area *__buddy_bins_put(struct buddy_allocator *ba,
					   struct buddy_area *a)
{
	bool done;
	irq_flags_t f;
	unsigned long bin_num;
	struct buddy_area *b, *c, *merge_area, *residue;

	/* Sanity checks */
	if (!ba || !a ||
	    (a->bin_num < ba->min_bin) ||
	    (a->bin_num > ba->max_bin)) {
		return NULL;
	}

	DPRINTF("%s: ba=%p map=0x%x bin_num=%d blk_count=%d\n",
		__func__, ba, a->map, a->bin_num, a->blk_count);

	/* Save the bin number */
	bin_num = a->bin_num;

	/* Lock desired bin */
	vmm_spin_lock_irqsave_lite(&ba->bins_lock[bin_num], f);

	/* Initialize area pointer required for block merging */
	residue = NULL;
	merge_area = a;

	/* Add blocks to buddy areas in sorted fashion */
	done = FALSE;
	list_for_each_entry(b, &ba->bins[bin_num], hk_head) {
		if (AREA_END(a) == AREA_START(b)) {
			b->map = a->map;
			b->blk_count += a->blk_count;
			buddy_hk_free(ba, a);
			merge_area = b;
			if (!list_is_first(&b->hk_head, &ba->bins[bin_num])) {
				c = list_entry(b->hk_head.prev,
						struct buddy_area, hk_head);
				if (AREA_END(c) == AREA_START(b)) {
					c->blk_count += b->blk_count;
					list_del(&b->hk_head);
					buddy_hk_free(ba, b);
					merge_area = c;
				}
			}
			done = TRUE;
			break;
		} else if (AREA_END(b) == AREA_START(a)) {
			b->blk_count += a->blk_count;
			buddy_hk_free(ba, a);
			merge_area = b;
			if (!list_is_last(&b->hk_head, &ba->bins[bin_num])) {
				c = list_entry(b->hk_head.next,
						struct buddy_area, hk_head);
				if (AREA_END(b) == AREA_START(c)) {
					b->blk_count += c->blk_count;
					list_del(&c->hk_head);
					buddy_hk_free(ba, c);
					merge_area = b;
				}
			}
			done = TRUE;
			break;
		} else if (AREA_END(a) < AREA_START(b)) {
			list_add_tail(&a->hk_head, &b->hk_head);
			done = TRUE;
			break;
		}
	}
	if (!done) {
		list_add_tail(&a->hk_head, &ba->bins[bin_num]);
	}

	/* If desired bin is max_bin then no need to go further */
	if (bin_num == ba->max_bin) {
		goto skip;
	}

	/* Do block merging on recently updated buddy area */
	if ((AREA_START(merge_area) & BLOCK_MASK(bin_num + 1)) &&
	    (merge_area->blk_count >= 3)) {
		/* Merge area start is not aligned to bigger bin 
		 * and it has 3 or more blocks
		 */
		a = buddy_hk_alloc(ba, merge_area->map, bin_num, 1);
		if (!a) {
			goto skip;
		}
		merge_area->map += BLOCK_SIZE(bin_num);
		merge_area->blk_count--;
		list_add_tail(&a->hk_head, &merge_area->hk_head);
	}
	if (!(AREA_START(merge_area) & BLOCK_MASK(bin_num + 1)) &&
	    (merge_area->blk_count >= 2)) {
		/* Merge area start is aligned to bigger bin 
		 * and it has 2 or more blocks
		 */
		residue = buddy_hk_alloc(ba, merge_area->map, bin_num + 1,
					 merge_area->blk_count >> 1);
		if (!residue) {
			goto skip;
		}
		merge_area->map += AREA_SIZE(residue);
		merge_area->blk_count -= residue->blk_count << 1;
		if (!merge_area->blk_count) {
			list_del(&merge_area->hk_head);
			buddy_hk_free(ba, merge_area);
		}
	}

skip:
	/* Unlock desired bin */
	vmm_spin_unlock_irqrestore_lite(&ba->bins_lock[bin_num], f);

	/* Return residuce blocks */
	return residue;
}

static void buddy_bins_put(struct buddy_allocator *ba,
			   struct buddy_area *a)
{
	struct buddy_area *residue;

	DPRINTF("%s: ba=%p map=0x%x bin_num=%d blk_count=%d\n",
		__func__, ba, a->map, a->bin_num, a->blk_count);

	residue = __buddy_bins_put(ba, a);
	while (residue) {
		residue = __buddy_bins_put(ba, residue);
	}
}

static struct buddy_area *buddy_bins_get(struct buddy_allocator *ba,
					 unsigned long bin_num,
					 unsigned long blk_count)
{
	bool found;
	irq_flags_t f;
	struct buddy_area *a, *residue, *ret;

	/* Sanity checks */
	if (!ba || !blk_count ||
	    (bin_num < ba->min_bin) ||
	    (bin_num > ba->max_bin)) {
		return NULL;
	}

	DPRINTF("%s: ba=%p bin_num=%d blk_count=%d\n",
		__func__, ba, bin_num, blk_count);

	/* Lock desired bin */
	vmm_spin_lock_irqsave_lite(&ba->bins_lock[bin_num], f);

	/* Initialize return value */
	ret = NULL;
	residue = NULL;

	/* Try to find existing buddy area of desired bin */
	found = FALSE;
	list_for_each_entry(a, &ba->bins[bin_num], hk_head) {
		if (blk_count <= a->blk_count) {
			found = TRUE;
			break;
		}
	}

	if (found) {
		/* Create a buddy area from desired bin */
		ret = buddy_hk_alloc(ba, a->map, bin_num, blk_count);
		if (!ret) {
			goto skip;
		}

		/* Update existing buddy area */
		a->map += blk_count * BLOCK_SIZE(bin_num);
		a->blk_count -= blk_count;

		/* If existing buddy area is empty then free it */
		if (!a->blk_count) {
			list_del(&a->hk_head);
			buddy_hk_free(ba, a);
		}
	} else {
		/* Create a buddy area from bigger bin */
		ret = buddy_bins_get(ba, bin_num + 1, (blk_count + 1) >> 1);
		if (!ret) {
			goto skip;
		}

		/* Downgrade bigger bin to desired bin */
		ret->bin_num = ret->bin_num - 1;
		ret->blk_count = ret->blk_count * 2;

		/* If we have desired buddy area then return else
		 * create new buddy area for residue blocks
		 */
		if (ret->blk_count == blk_count) {
			goto skip;
		}
		residue = buddy_hk_alloc(ba,
				ret->map + blk_count * BLOCK_SIZE(bin_num),
				bin_num, ret->blk_count - blk_count);
		if (!residue) {
			goto skip;
		}

		/* Make sure we return only requested number of blocks */
		ret->blk_count = blk_count;
	}

skip:
	/* Unlock desired bin */
	vmm_spin_unlock_irqrestore_lite(&ba->bins_lock[bin_num], f);

	/* Put back residuce blocks */
	if (residue) {
		buddy_bins_put(ba, residue);
	}

	return ret;
}

static struct buddy_area *buddy_bins_reserve(struct buddy_allocator *ba,
					     unsigned long bin_num,
					     unsigned long addr,
					     unsigned long size)
{
	bool found;
	irq_flags_t f;
	unsigned long blk_count;
	struct buddy_area *a, *b, *ret = NULL;

	/* Sanity checks */
	if (!ba || !size ||
	    (bin_num < ba->min_bin) ||
	    (bin_num > ba->max_bin)) {
		return NULL;
	}

	DPRINTF("%s: ba=%p bin_num=%d addr=0x%x size=0x%x\n",
		__func__, ba, bin_num, addr, size);

	/* Align address to block boundary */
	size = size + (addr - (addr & ~BLOCK_MASK(bin_num)));
	addr = addr & ~BLOCK_MASK(bin_num);

	/* Compute block count */
	blk_count = BLOCK_COUNT(size, bin_num);
	if ((blk_count * BLOCK_SIZE(bin_num)) < size) {
		blk_count++;
	}
	size = blk_count * BLOCK_SIZE(bin_num);

	/* Lock desired bin */
	vmm_spin_lock_irqsave_lite(&ba->bins_lock[bin_num], f);

	/* Try to find existing buddy area of desired bin */
	found = FALSE;
	list_for_each_entry(a, &ba->bins[bin_num], hk_head) {
		if ((AREA_START(a) <= addr) &&
		    (addr < AREA_END(a)) &&
		    (AREA_START(a) <= (addr + size)) &&
		    ((addr + size) <= AREA_END(a))) {
			found = TRUE;
			break;
		}
	}

	if (!found) {
		goto skip;
	}

	/* Create a buddy area from desired bin */
	ret = buddy_hk_alloc(ba, addr, bin_num, blk_count);
	if (!ret) {
		goto skip;
	}

	/* If we have few blocks between ret area end and
	 * found area end them add them back after found area.
	 */
	if (BLOCK_COUNT(AREA_END(a) - AREA_END(ret), bin_num)) {
		b = buddy_hk_alloc(ba, AREA_END(ret), bin_num,
			BLOCK_COUNT(AREA_END(a) - AREA_END(ret), bin_num));
		if (!b) {
			buddy_hk_free(ba, ret);
			ret = NULL;
			goto skip;
		}
		list_add(&b->hk_head, &a->hk_head);
	}

	/* Update existing buddy area */
	a->blk_count = BLOCK_COUNT(AREA_START(ret) - AREA_START(a), bin_num);

	/* If existing buddy area is empty then free it */
	if (!a->blk_count) {
		list_del(&a->hk_head);
		buddy_hk_free(ba, a);
	}

skip:
	/* Unlock desired bin */
	vmm_spin_unlock_irqrestore_lite(&ba->bins_lock[bin_num], f);

	return ret;
}

unsigned long buddy_bins_area_count(struct buddy_allocator *ba,
				    unsigned long bin_num)
{
	irq_flags_t f;
	unsigned long ret;
	struct buddy_area *a;

	/* Sanity checks */
	if (!ba ||
	    (bin_num < ba->min_bin) ||
	    (bin_num > ba->max_bin)) {
		return 0;
	}

	/* Lock desired bin */
	vmm_spin_lock_irqsave_lite(&ba->bins_lock[bin_num], f);

	/* Count areas */
	ret = 0;
	list_for_each_entry(a, &ba->bins[bin_num], hk_head) {
		ret++;
	}

	/* Unlock desired bin */
	vmm_spin_unlock_irqrestore_lite(&ba->bins_lock[bin_num], f);

	return ret;
}

unsigned long buddy_bins_block_count(struct buddy_allocator *ba,
				     unsigned long bin_num)
{
	irq_flags_t f;
	unsigned long ret;
	struct buddy_area *a;

	/* Sanity checks */
	if (!ba ||
	    (bin_num < ba->min_bin) ||
	    (bin_num > ba->max_bin)) {
		return 0;
	}

	/* Lock desired bin */
	vmm_spin_lock_irqsave_lite(&ba->bins_lock[bin_num], f);

	/* Count areas */
	ret = 0;
	list_for_each_entry(a, &ba->bins[bin_num], hk_head) {
		ret += a->blk_count;
	}

	/* Unlock desired bin */
	vmm_spin_unlock_irqrestore_lite(&ba->bins_lock[bin_num], f);

	return ret;
}

unsigned long buddy_bins_free_space(struct buddy_allocator *ba)
{
	unsigned long bin, ret;

	/* Sanity checks */
	if (!ba) {
		return 0;
	}

	/* Count free space */
	ret = 0;
	for (bin = ba->min_bin; bin <= ba->max_bin; bin++) {
		ret += buddy_bins_block_count(ba, bin) * BLOCK_SIZE(bin);
	}

	return ret;
}

int buddy_mem_alloc(struct buddy_allocator *ba,
		    unsigned long size,
		    unsigned long *addr)
{
	struct buddy_area *a, *t;
	unsigned long bin_num, blk_count;

	/* Sanity checks */
	if (!ba || !size || !addr) {
		return VMM_EINVALID;
	}

	DPRINTF("%s: ba=%p size=%d\n", __func__, ba, size);

	/* Estimated bin number and block count */
	bin_num = buddy_estimate_bin(ba, size);
	blk_count = BLOCK_COUNT(size, bin_num);
	if ((blk_count * BLOCK_SIZE(bin_num)) < size) {
		blk_count++;
	}

	/* Get buddy area from bins */
	a = buddy_bins_get(ba, bin_num, blk_count);
	if (!a) {
		return VMM_ENOMEM;
	}

	/* Downgrade to smallest bin */
	a->blk_count = a->blk_count * (0x1UL << (a->bin_num - ba->min_bin));
	a->bin_num = ba->min_bin;

	/* Try to reduce memory wastage */
	blk_count = BLOCK_COUNT(size, a->bin_num);
	if ((blk_count * BLOCK_SIZE(a->bin_num)) < size) {
		blk_count++;
	}
	if (blk_count < a->blk_count) {
		t = buddy_hk_alloc(ba,
			AREA_START(a) + blk_count * BLOCK_SIZE(a->bin_num),
			a->bin_num, a->blk_count - blk_count);
		if (!t) {
			goto skip;
		}
		a->blk_count = blk_count;
		buddy_bins_put(ba, t);
	}

skip:
	/* Add buddy area to alloc tree */
	buddy_alloc_add(ba, a);

	/* Return allocated address */
	*addr = a->map;

	return VMM_OK;
}

int buddy_mem_aligned_alloc(struct buddy_allocator *ba,
			    unsigned long order,
			    unsigned long size,
			    unsigned long *addr)
{
	struct buddy_area *a, *t;
	unsigned long bin_num, blk_count;
	unsigned long order_bin_num, order_blk_count;

	/* Sanity checks */
	if (!ba || !size || !addr ||
	    (ba->max_bin < order)) {
		return VMM_EINVALID;
	}

	DPRINTF("%s: ba=%p order=%d size=%d\n",
		__func__, ba, order, size);

	/* Estimated bin number and block count */
	bin_num = buddy_estimate_bin(ba, size);
	blk_count = BLOCK_COUNT(size, bin_num);
	if ((blk_count * BLOCK_SIZE(bin_num)) < size) {
		blk_count++;
	}

	/* Desired bin number and block count */
	order_bin_num = order;
	order_blk_count = BLOCK_COUNT(size, order_bin_num);
	if ((order_blk_count * BLOCK_SIZE(order_bin_num)) < size) {
		order_blk_count++;
	}

	if (order_bin_num <= bin_num) {
		/* Get buddy area from estimated bin */
		a = buddy_bins_get(ba, bin_num, blk_count);
		if (!a) {
			return VMM_ENOMEM;
		}
	} else {
		/* Get buddy area from desired bin */
		a = buddy_bins_get(ba, order_bin_num, order_blk_count);
		if (!a) {
			return VMM_ENOMEM;
		}

		/* Downgrade bin number to estimated bin */
		a->blk_count =
			a->blk_count * (0x1UL << (order_bin_num - bin_num));
		a->bin_num = bin_num;

		/* If we have residue blocks then put back to bins */
		if (blk_count < a->blk_count) {
			t = buddy_hk_alloc(ba,
				a->map + blk_count * BLOCK_SIZE(bin_num),
				bin_num, a->blk_count - blk_count);
			if (!t) {
				goto skip;
			}
			a->blk_count = blk_count;
			buddy_bins_put(ba, t);
		}
	}

	/* Downgrade to smallest bin */
	a->blk_count = a->blk_count * (0x1UL << (a->bin_num - ba->min_bin));
	a->bin_num = ba->min_bin;

	/* Try to reduce memory wastage */
	blk_count = BLOCK_COUNT(size, a->bin_num);
	if ((blk_count * BLOCK_SIZE(a->bin_num)) < size) {
		blk_count++;
	}
	if (blk_count < a->blk_count) {
		t = buddy_hk_alloc(ba,
			AREA_START(a) + blk_count * BLOCK_SIZE(a->bin_num),
			a->bin_num, a->blk_count - blk_count);
		if (!t) {
			goto skip;
		}
		a->blk_count = blk_count;
		buddy_bins_put(ba, t);
	}

skip:
	/* Add buddy area to alloc tree */
	buddy_alloc_add(ba, a);
	
	/* Return allocated address */
	*addr = a->map;

	return VMM_OK;
}

int buddy_mem_reserve(struct buddy_allocator *ba,
		      unsigned long addr,
		      unsigned long size)
{
	struct buddy_area *a, *b;
	unsigned long bin, tsz;

	/* Sanity checks */
	if (!ba || !size ||
	    (addr < ba->mem_start) ||
	    ((ba->mem_start + ba->mem_size) <= addr)) {
		return VMM_EINVALID;
	}

	DPRINTF("%s: ba=%p addr=0x%x size=0x%x\n",
		__func__, ba, addr, size);

	/* Try to reserve from smallest bin to biggest bin */
	for (bin = ba->min_bin; bin <= ba->max_bin; bin++) {
		a = buddy_bins_reserve(ba, bin, addr, size);
		if (a) {
			break;
		}
	}
	if (!a) {
		return VMM_ENOTAVAIL;
	}

	/* Downgrade to smallest bin */
	a->blk_count = a->blk_count * (0x1UL << (a->bin_num - ba->min_bin));
	a->bin_num = ba->min_bin;

	/* Collect residue from start of reserved buddy area */
	if (BLOCK_COUNT(addr - AREA_START(a), a->bin_num)) {
		b = buddy_hk_alloc(ba, AREA_START(a), a->bin_num,
			BLOCK_COUNT(addr - AREA_START(a), a->bin_num));
		if (!b) {
			goto skip;
		}
		a->map = a->map + AREA_SIZE(b);
		a->blk_count = a->blk_count - b->blk_count;
		buddy_bins_put(ba, b);
	}

	/* Collect residue from end of reserved buddy area */
	tsz = (addr + size) & BLOCK_MASK(a->bin_num);
	if (tsz) {
		tsz = size + (BLOCK_SIZE(a->bin_num) - tsz);
	} else {
		tsz = size;
	}
	if (BLOCK_COUNT(AREA_END(a) - (addr + tsz), a->bin_num)) {
		b = buddy_hk_alloc(ba, (addr + tsz), a->bin_num,
			BLOCK_COUNT(AREA_END(a) - (addr + tsz), a->bin_num));
		if (!b) {
			goto skip;
		}
		a->blk_count = a->blk_count - b->blk_count;
		buddy_bins_put(ba, b);
	}

skip:
	/* Add buddy area to alloc tree */
	buddy_alloc_add(ba, a);

	return VMM_OK;
}

int buddy_mem_find(struct buddy_allocator *ba,
		   unsigned long addr,
		   unsigned long *alloc_addr,
		   unsigned long *alloc_bin,
		   unsigned long *alloc_size)
{
	struct buddy_area *a;
	unsigned long a_addr, a_bin, a_blk_count;

	/* Sanity checks */
	if (!ba || (addr < ba->mem_start) ||
	    ((ba->mem_start + ba->mem_size) <= addr)) {
		return VMM_EINVALID;
	}

	DPRINTF("%s: ba=%p addr=0x%x\n", __func__, ba, addr);

	/* Find buddy area from alloc tree */
	a = buddy_alloc_find(ba, addr, &a_addr, &a_bin, &a_blk_count);
	if (!a) {
		return VMM_ENOTAVAIL;
	}

	/* Fill-up return values */
	if (alloc_addr) {
		*alloc_addr = a_addr;
	}
	if (alloc_bin) {
		*alloc_bin = a_bin;
	}
	if (alloc_size) {
		*alloc_size = BLOCK_SIZE(a_bin) * a_blk_count;
	}

	return VMM_OK;
}

int buddy_mem_free(struct buddy_allocator *ba, unsigned long addr)
{
	struct buddy_area *a;

	/* Sanity checks */
	if (!ba || (addr < ba->mem_start) ||
	    ((ba->mem_start + ba->mem_size) <= addr)) {
		return VMM_EINVALID;
	}

	DPRINTF("%s: ba=%p addr=0x%x\n", __func__, ba, addr);

	/* Find buddy area from alloc tree */
	a = buddy_alloc_find(ba, addr, NULL, NULL, NULL);
	if (!a) {
		return VMM_ENOTAVAIL;
	}

	/* Delete buddy area from alloc tree */
	buddy_alloc_del(ba, a);

	/* Put back blocks to bins */
	buddy_bins_put(ba, a);

	return VMM_OK;
}

int buddy_mem_partial_free(struct buddy_allocator *ba,
			   unsigned long addr, unsigned long size)
{
	struct buddy_area *a, *b;
	unsigned long old_bin_num, old_blk_count;

	/* Sanity checks */
	if (!ba || (addr < ba->mem_start) ||
	    ((ba->mem_start + ba->mem_size) <= addr)) {
		return VMM_EINVALID;
	}

	DPRINTF("%s: ba=%p addr=0x%x size=0x%x\n",
		__func__, ba, addr, size);

	/* Find buddy area from alloc tree */
	a = buddy_alloc_find(ba, addr, NULL, NULL, NULL);
	if (!a) {
		return VMM_ENOTAVAIL;
	}

	/* Downgrade to smallest bin */
	old_bin_num = a->bin_num;
	old_blk_count = a->blk_count;
	a->blk_count = a->blk_count * (0x1UL << (a->bin_num - ba->min_bin));
	a->bin_num = ba->min_bin;

	/* More sanity checks */
	if (BLOCK_COUNT(addr - AREA_START(a), a->bin_num) &&
	    (addr & BLOCK_MASK(a->bin_num))) {
		a->blk_count = old_blk_count;
		a->bin_num = old_bin_num;
		return VMM_EINVALID;
	}
	if (BLOCK_COUNT(AREA_END(a) - (addr + size), a->bin_num) &&
	    ((addr + size) & BLOCK_MASK(a->bin_num))) {
		a->blk_count = old_blk_count;
		a->bin_num = old_bin_num;
		return VMM_EINVALID;
	}

	/* Delete buddy area from alloc tree */
	buddy_alloc_del(ba, a);

	/* Collect residue from start of freed buddy area */
	if (BLOCK_COUNT(addr - AREA_START(a), a->bin_num) &&
	    !(addr & BLOCK_MASK(a->bin_num))) {
		b = buddy_hk_alloc(ba, AREA_START(a), a->bin_num,
			BLOCK_COUNT(addr - AREA_START(a), a->bin_num));
		if (!b) {
			goto skip;
		}
		a->map = a->map + AREA_SIZE(b);
		a->blk_count = a->blk_count - b->blk_count;
		buddy_alloc_add(ba, b);
	}

	/* Collect residue from end of freed buddy area */
	if (BLOCK_COUNT(AREA_END(a) - (addr + size), a->bin_num) &&
	    !((addr + size) & BLOCK_MASK(a->bin_num))) {
		b = buddy_hk_alloc(ba, (addr + size), a->bin_num,
			BLOCK_COUNT(AREA_END(a) - (addr + size), a->bin_num));
		if (!b) {
			goto skip;
		}
		a->blk_count = a->blk_count - b->blk_count;
		buddy_alloc_add(ba, b);
	}

skip:
	/* Put back blocks to bins */
	buddy_bins_put(ba, a);

	return VMM_OK;
}

int buddy_allocator_init(struct buddy_allocator *ba,
			 void *hk_area, unsigned long hk_area_size,
			 unsigned long mem_start, unsigned long mem_size,
			 unsigned long min_bin, unsigned long max_bin)
{
	unsigned long i, bin, count;
	struct buddy_area *a = NULL;

	/* Sanity checks */
	if (!ba || !hk_area) {
		return VMM_EFAIL;
	}
	if ((min_bin > max_bin) ||
	    (BUDDY_MAX_SUPPORTED_BIN <= min_bin) ||
	    (BUDDY_MAX_SUPPORTED_BIN <= max_bin) ||
	    (mem_size < (1 << min_bin)) ||
	    (mem_start & BLOCK_MASK(min_bin)) ||
	    ((mem_start + mem_size) <= mem_start) ||
	    (hk_area_size < sizeof(struct buddy_area))) {
		return VMM_EINVALID;
	}

	/* Initialize house-keeping */
	ba->hk_area = hk_area;
	ba->hk_area_size = hk_area_size;
	INIT_SPIN_LOCK(&ba->hk_free_lock);
	ba->hk_total_count = ba->hk_area_size / sizeof(struct buddy_area);
	ba->hk_free_count = ba->hk_total_count;
	INIT_LIST_HEAD(&ba->hk_free_list);
	for (i = 0; i < ba->hk_total_count; i++) {
		a = ba->hk_area + i * sizeof(struct buddy_area);
		memset(a, 0, sizeof(struct buddy_area));
		INIT_LIST_HEAD(&a->hk_head);
		RB_CLEAR_NODE(&a->hk_rb);
		list_add_tail(&a->hk_head, &ba->hk_free_list);
	}
	DPRINTF("%s: ba=%p hk_total_count=%d\n",
		__func__, ba, ba->hk_total_count);

	/* Save configuration */
	ba->mem_start = mem_start;
	ba->mem_size = mem_size;
	ba->min_bin = min_bin;
	ba->max_bin = max_bin;

	/* Setup empty alloc tree */
	INIT_SPIN_LOCK(&ba->alloc_lock);
	ba->alloc = RB_ROOT;

	/* Setup empty bins and alloc trees */
	for (i = 0; i < BUDDY_MAX_SUPPORTED_BIN; i++) {
		INIT_SPIN_LOCK(&ba->bins_lock[i]);
		INIT_LIST_HEAD(&ba->bins[i]);
	}

	/* Fill-up bins */
	while (mem_size) {
		bin = buddy_estimate_bin(ba, mem_size);
		if (mem_size < BLOCK_SIZE(bin)) {
			break;
		}
		count = BLOCK_COUNT(mem_size, bin);
		a = buddy_hk_alloc(ba, mem_start, bin, count);
		if (!a) {
			return VMM_ENOMEM;
		}
		mem_size -= count * BLOCK_SIZE(bin);
		mem_start += count * BLOCK_SIZE(bin);
		buddy_bins_put(ba, a);
	}

	return VMM_OK;
}

