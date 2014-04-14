/**
 * Copyright (c) 2014 Pranavkumar Sawargaonkar.
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
 * @file radix-tree.c
 * @author Pranavkumar Sawargaonkar (pranav.sawargaonkar@gmail.com)
 * @brief Implementation of Radix Trees.
 *
 * The source has been largely adapted from:
 * linux-xxx/lib/radix-tree.c
 *
 * Copyright (C) 2001 Momchil Velikov
 * Portions Copyright (C) 2001 Christoph Hellwig
 * Copyright (C) 2005 SGI, Christoph Lameter
 * Copyright (C) 2006 Nick Piggin
 * Copyright (C) 2012 Konstantin Khlebnikov
 *
 * The original code is licensed under the GPL.
 */

#include <vmm_macros.h>
#include <vmm_heap.h>
#include <vmm_limits.h>
#include <vmm_stdio.h>
#include <libs/bitmap.h>
#include <libs/mathlib.h>
#include <libs/stringlib.h>
#include <libs/radix-tree.h>

#define RADIX_TREE_MAP_SHIFT	6
#define RADIX_TREE_MAP_SIZE	(1UL << RADIX_TREE_MAP_SHIFT)
#define RADIX_TREE_MAP_MASK	(RADIX_TREE_MAP_SIZE-1)

#define RADIX_TREE_TAG_LONGS	\
	((RADIX_TREE_MAP_SIZE + BITS_PER_LONG - 1) / BITS_PER_LONG)

struct radix_tree_node {
	unsigned int	height;		/* Height from the bottom */
	unsigned int	count;
	union {
		struct radix_tree_node *parent;	/* Used when ascending tree */
	};
	vmm_rwlock_t	lock;
	void	*slots[RADIX_TREE_MAP_SIZE];
};

#define RADIX_TREE_INDEX_BITS  (8 /* CHAR_BIT */ * sizeof(unsigned long))

#define RADIX_DIV_ROUND_UP(n, d)	(((n) + (d) - 1) / (d))
#define RADIX_TREE_MAX_PATH (RADIX_DIV_ROUND_UP(RADIX_TREE_INDEX_BITS, \
					  RADIX_TREE_MAP_SHIFT))

/*
 * The height_to_maxindex array needs to be one deeper than the maximum
 * path as height 0 holds only 1 entry.
 */
static unsigned long height_to_maxindex[RADIX_TREE_MAX_PATH + 1];
static bool  height_to_maxindex_init_done = FALSE;

static unsigned long __maxindex(unsigned int height)
{
	unsigned int width = height * RADIX_TREE_MAP_SHIFT;
	int shift = RADIX_TREE_INDEX_BITS - width;

	if (shift < 0)
		return ~0UL;
	if (shift >= BITS_PER_LONG)
		return 0UL;
	return ~0UL >> shift;
}

static void radix_tree_init_maxindex(void)
{
	unsigned int i;

	if (height_to_maxindex_init_done)
		return;

	for (i = 0; i < array_size(height_to_maxindex); i++)
		height_to_maxindex[i] = __maxindex(i);

	height_to_maxindex_init_done = TRUE;
}

static inline void *ptr_to_indirect(void *ptr)
{
	return (void *)((unsigned long)ptr | RADIX_TREE_INDIRECT_PTR);
}

static inline void *indirect_to_ptr(void *ptr)
{
	return (void *)((unsigned long)ptr & ~RADIX_TREE_INDIRECT_PTR);
}

/*
 * This assumes that the caller has performed appropriate preallocation, and
 * that the caller has pinned this thread of control to the current CPU.
 */
static struct radix_tree_node *
radix_tree_node_alloc(struct radix_tree_root *root)
{
	struct radix_tree_node *ret = NULL;

	ret = vmm_zalloc(sizeof(struct radix_tree_node));
	INIT_RW_LOCK(&ret->lock);

	return ret;
}

static inline void
radix_tree_node_free(struct radix_tree_node *node)
{
	node->slots[0] = NULL;
	node->count = 0;

	vmm_free(node);
}

/*
 *	Return the maximum key which can be store into a
 *	radix tree with height HEIGHT.
 */
static inline unsigned long radix_tree_maxindex(unsigned int height)
{
	radix_tree_init_maxindex();

	return height_to_maxindex[height];
}

/*
 *	Extend a radix tree so it can store key @index.
 */
static int radix_tree_extend(struct radix_tree_root *root, unsigned long index)
{
	struct radix_tree_node *node;
	struct radix_tree_node *slot;
	unsigned int height;
	unsigned long flags;

	/* Figure out what the height should be.  */
	height = root->height + 1;
	while (index > radix_tree_maxindex(height))
		height++;

	if (root->rnode == NULL) {
		root->height = height;
		goto out;
	}

	do {
		unsigned int newheight;
		if (!(node = radix_tree_node_alloc(root)))
			return VMM_ENOMEM;

		/* Increase the height.  */
		newheight = root->height+1;
		node->height = newheight;
		node->count = 1;
		node->parent = NULL;
		slot = root->rnode;
		if (newheight > 1) {
			slot = indirect_to_ptr(slot);
			slot->parent = node;
		}
		node->slots[0] = slot;
		node = ptr_to_indirect(node);
		vmm_write_lock_irqsave_lite(&root->lock, flags);
		root->rnode = node;
		root->height = newheight;
		vmm_write_unlock_irqrestore_lite(&root->lock, flags);
	} while (height > root->height);
out:
	return 0;
}

/**
 *	radix_tree_insert    -    insert into a radix tree
 *	@root:		radix tree root
 *	@index:		index key
 *	@item:		item to insert
 *
 *	Insert an item into the radix tree at position @index.
 */
int radix_tree_insert(struct radix_tree_root *root,
			unsigned long index, void *item)
{
	struct radix_tree_node *node = NULL, *slot;
	unsigned int height, shift;
	int offset;
	int error;
	unsigned long flags;

	if (radix_tree_is_indirect_ptr(item))
		return VMM_EINVALID;

	/* Make sure the tree is high enough.  */
	if (index > radix_tree_maxindex(root->height)) {
		error = radix_tree_extend(root, index);
		if (error)
			return error;
	}

	slot = indirect_to_ptr(root->rnode);

	height = root->height;
	shift = (height-1) * RADIX_TREE_MAP_SHIFT;

	offset = 0;			/* uninitialised var warning */
	while (height > 0) {
		if (slot == NULL) {
			/* Have to add a child node.  */
			if (!(slot = radix_tree_node_alloc(root)))
				return VMM_ENOMEM;
			slot->height = height;
			slot->parent = node;
			if (node) {
				vmm_write_lock_irqsave_lite(&node->lock, flags);
				node->slots[offset] = slot;
				node->count++;
				vmm_write_unlock_irqrestore_lite(&node->lock, flags);
			} else {
				vmm_write_lock_irqsave_lite(&root->lock, flags);
				root->rnode = ptr_to_indirect(slot);
				vmm_write_unlock_irqrestore_lite(&root->lock, flags);
			}
		}

		/* Go a level down */
		offset = (index >> shift) & RADIX_TREE_MAP_MASK;
		node = slot;
		slot = node->slots[offset];
		shift -= RADIX_TREE_MAP_SHIFT;
		height--;
	}

	if (slot != NULL)
		return VMM_EEXIST;

	if (node) {
		vmm_write_lock_irqsave_lite(&node->lock, flags);
		node->count++;
		node->slots[offset] = item;
		vmm_write_unlock_irqrestore_lite(&node->lock, flags);
	} else {
		vmm_write_lock_irqsave_lite(&root->lock, flags);
		root->rnode = item;
		vmm_write_unlock_irqrestore_lite(&root->lock, flags);
	}

	return 0;
}

/*
 * is_slot == 1 : search for the slot.
 * is_slot == 0 : search for the node.
 */
static void *radix_tree_lookup_element(struct radix_tree_root *root,
				unsigned long index, int is_slot)
{
	unsigned int height, shift;
	struct radix_tree_node *node, **slot;
	unsigned long flags;

	vmm_read_lock_irqsave_lite(&root->lock, flags);
	node = root->rnode;
	vmm_read_unlock_irqrestore_lite(&root->lock, flags);

	if (node == NULL)
		return NULL;

	if (!radix_tree_is_indirect_ptr(node)) {
		if (index > 0)
			return NULL;
		return is_slot ? (void *)&root->rnode : node;
	}
	node = indirect_to_ptr(node);

	height = node->height;
	if (index > radix_tree_maxindex(height))
		return NULL;

	shift = (height-1) * RADIX_TREE_MAP_SHIFT;

	do {
		slot = (struct radix_tree_node **)
			(node->slots + ((index>>shift) & RADIX_TREE_MAP_MASK));

		vmm_read_lock_irqsave_lite(&root->lock, flags);
		node = *slot;
		vmm_read_unlock_irqrestore_lite(&root->lock, flags);

		if (node == NULL)
			return NULL;

		shift -= RADIX_TREE_MAP_SHIFT;
		height--;
	} while (height > 0);

	return is_slot ? (void *)slot : indirect_to_ptr(node);
}

/**
 *	radix_tree_lookup_slot    -    lookup a slot in a radix tree
 *	@root:		radix tree root
 *	@index:		index key
 *
 *	Returns:  the slot corresponding to the position @index in the
 *	radix tree @root. This is useful for update-if-exists operations.
 *
 *	This function can be called under rcu_read_lock iff the slot is not
 *	modified by radix_tree_replace_slot, otherwise it must be called
 *	exclusive from other writers. Any dereference of the slot must be done
 *	using radix_tree_deref_slot.
 */
void **radix_tree_lookup_slot(struct radix_tree_root *root, unsigned long index)
{
	return (void **)radix_tree_lookup_element(root, index, 1);
}

/**
 *	radix_tree_lookup    -    perform lookup operation on a radix tree
 *	@root:		radix tree root
 *	@index:		index key
 *
 *	Lookup the item at the position @index in the radix tree @root.
 *
 *	This function can be called under rcu_read_lock, however the caller
 *	must manage lifetimes of leaf nodes (eg. RCU may also be used to free
 *	them safely). No RCU barriers are required to access or modify the
 *	returned item, however.
 */
void *radix_tree_lookup(struct radix_tree_root *root, unsigned long index)
{
	return radix_tree_lookup_element(root, index, 0);
}

/**
 * radix_tree_next_chunk - find next chunk of slots for iteration
 *
 * @root:	radix tree root
 * @iter:	iterator state
 * @flags:	RADIX_TREE_ITER_* flags and tag index
 * Returns:	pointer to chunk first slot, or NULL if iteration is over
 */
void **radix_tree_next_chunk(struct radix_tree_root *root,
			     struct radix_tree_iter *iter, unsigned flags)
{
	unsigned shift;
	struct radix_tree_node *rnode, *node;
	unsigned long index, offset;
	unsigned long irq_flags;

	/*
	 * Catch next_index overflow after ~0UL. iter->index never overflows
	 * during iterating; it can be zero only at the beginning.
	 * And we cannot overflow iter->next_index in a single step,
	 * because RADIX_TREE_MAP_SHIFT < BITS_PER_LONG.
	 *
	 * This condition also used by radix_tree_next_slot() to stop
	 * contiguous iterating, and forbid swithing to the next chunk.
	 */
	index = iter->next_index;
	if (!index && iter->index)
		return NULL;


	vmm_read_lock_irqsave_lite(&root->lock, irq_flags);	
	rnode = root->rnode;
	vmm_read_unlock_irqrestore_lite(&root->lock, irq_flags);

	if (radix_tree_is_indirect_ptr(rnode)) {
		rnode = indirect_to_ptr(rnode);
	} else if (rnode && !index) {
		/* Single-slot tree */
		iter->index = 0;
		iter->next_index = 1;
		iter->tags = 1;
		return (void **)&root->rnode;
	} else
		return NULL;

restart:
	shift = (rnode->height - 1) * RADIX_TREE_MAP_SHIFT;
	offset = index >> shift;

	/* Index outside of the tree */
	if (offset >= RADIX_TREE_MAP_SIZE)
		return NULL;

	node = rnode;
	while (1) {
		/* This is leaf-node */
		if (!shift)
			break;

		vmm_read_lock_irqsave_lite(&node->lock, irq_flags);
		node = node->slots[offset];
		vmm_read_unlock_irqrestore_lite(&node->lock, irq_flags);

		if (node == NULL)
			goto restart;
		shift -= RADIX_TREE_MAP_SHIFT;
		offset = (index >> shift) & RADIX_TREE_MAP_MASK;
	}

	/* Update the iterator state */
	iter->index = index;
	iter->next_index = (index | RADIX_TREE_MAP_MASK) + 1;

	return node->slots + offset;
}

/**
 *	radix_tree_next_hole    -    find the next hole (not-present entry)
 *	@root:		tree root
 *	@index:		index key
 *	@max_scan:	maximum range to search
 *
 *	Search the set [index, min(index+max_scan-1, MAX_INDEX)] for the lowest
 *	indexed hole.
 *
 *	Returns: the index of the hole if found, otherwise returns an index
 *	outside of the set specified (in which case 'return - index >= max_scan'
 *	will be true). In rare cases of index wrap-around, 0 will be returned.
 *
 *	radix_tree_next_hole may be called under rcu_read_lock. However, like
 *	radix_tree_gang_lookup, this will not atomically search a snapshot of
 *	the tree at a single point in time. For example, if a hole is created
 *	at index 5, then subsequently a hole is created at index 10,
 *	radix_tree_next_hole covering both indexes may return 10 if called
 *	under rcu_read_lock.
 */
unsigned long radix_tree_next_hole(struct radix_tree_root *root,
				unsigned long index, unsigned long max_scan)
{
	unsigned long i;

	for (i = 0; i < max_scan; i++) {
		if (!radix_tree_lookup(root, index))
			break;
		index++;
		if (index == 0)
			break;
	}

	return index;
}

/**
 *	radix_tree_prev_hole    -    find the prev hole (not-present entry)
 *	@root:		tree root
 *	@index:		index key
 *	@max_scan:	maximum range to search
 *
 *	Search backwards in the range [max(index-max_scan+1, 0), index]
 *	for the first hole.
 *
 *	Returns: the index of the hole if found, otherwise returns an index
 *	outside of the set specified (in which case 'index - return >= max_scan'
 *	will be true). In rare cases of wrap-around, ULONG_MAX will be returned.
 *
 *	radix_tree_next_hole may be called under rcu_read_lock. However, like
 *	radix_tree_gang_lookup, this will not atomically search a snapshot of
 *	the tree at a single point in time. For example, if a hole is created
 *	at index 10, then subsequently a hole is created at index 5,
 *	radix_tree_prev_hole covering both indexes may return 5 if called under
 *	rcu_read_lock.
 */
unsigned long radix_tree_prev_hole(struct radix_tree_root *root,
				   unsigned long index, unsigned long max_scan)
{
	unsigned long i;

	for (i = 0; i < max_scan; i++) {
		if (!radix_tree_lookup(root, index))
			break;
		index--;
		if (index == ULONG_MAX)
			break;
	}

	return index;
}

/**
 *	radix_tree_gang_lookup - perform multiple lookup on a radix tree
 *	@root:		radix tree root
 *	@results:	where the results of the lookup are placed
 *	@first_index:	start the lookup from this key
 *	@max_items:	place up to this many items at *results
 *
 *	Performs an index-ascending scan of the tree for present items.  Places
 *	them at *@results and returns the number of items which were placed at
 *	*@results.
 *
 *	The implementation is naive.
 *
 *	Like radix_tree_lookup, radix_tree_gang_lookup may be called under
 *	rcu_read_lock. In this case, rather than the returned results being
 *	an atomic snapshot of the tree at a single point in time, the semantics
 *	of an RCU protected gang lookup are as though multiple radix_tree_lookups
 *	have been issued in individual locks, and results stored in 'results'.
 */
unsigned int
radix_tree_gang_lookup(struct radix_tree_root *root, void **results,
			unsigned long first_index, unsigned int max_items)
{
	struct radix_tree_iter iter;
	void **slot;
	unsigned int ret = 0;
	unsigned long flags;

	if (unlikely(!max_items))
		return 0;

	radix_tree_for_each_slot(slot, root, &iter, first_index) {
		vmm_read_lock_irqsave_lite(&root->lock, flags);
		results[ret] = indirect_to_ptr((*slot));
		vmm_read_unlock_irqrestore_lite(&root->lock, flags);

		if (!results[ret])
			continue;
		if (++ret == max_items)
			break;
	}

	return ret;
}

/**
 *	radix_tree_gang_lookup_slot - perform multiple slot lookup on radix tree
 *	@root:		radix tree root
 *	@results:	where the results of the lookup are placed
 *	@indices:	where their indices should be placed (but usually NULL)
 *	@first_index:	start the lookup from this key
 *	@max_items:	place up to this many items at *results
 *
 *	Performs an index-ascending scan of the tree for present items.  Places
 *	their slots at *@results and returns the number of items which were
 *	placed at *@results.
 *
 *	The implementation is naive.
 *
 *	Like radix_tree_gang_lookup as far as RCU and locking goes. Slots must
 *	be dereferenced with radix_tree_deref_slot, and if using only RCU
 *	protection, radix_tree_deref_slot may fail requiring a retry.
 */
unsigned int
radix_tree_gang_lookup_slot(struct radix_tree_root *root,
			void ***results, unsigned long *indices,
			unsigned long first_index, unsigned int max_items)
{
	struct radix_tree_iter iter;
	void **slot;
	unsigned int ret = 0;

	if (unlikely(!max_items))
		return 0;

	radix_tree_for_each_slot(slot, root, &iter, first_index) {
		results[ret] = slot;
		if (indices)
			indices[ret] = iter.index;
		if (++ret == max_items)
			break;
	}

	return ret;
}

/**
 *	radix_tree_shrink    -    shrink height of a radix tree to minimal
 *	@root		radix tree root
 */
static inline void radix_tree_shrink(struct radix_tree_root *root)
{
	/* try to shrink tree height */
	while (root->height > 0) {
		struct radix_tree_node *to_free = root->rnode;
		struct radix_tree_node *slot;

		if(!radix_tree_is_indirect_ptr(to_free)) {
			vmm_printf("%s Unexpected Indirect pointer\n", __func__);
			return;
		}

		to_free = indirect_to_ptr(to_free);

		/*
		 * The candidate node has more than one child, or its child
		 * is not at the leftmost slot, we cannot shrink.
		 */
		if (to_free->count != 1)
			break;
		if (!to_free->slots[0])
			break;

		/*
		 * We don't need rcu_assign_pointer(), since we are simply
		 * moving the node from one part of the tree to another: if it
		 * was safe to dereference the old pointer to it
		 * (to_free->slots[0]), it will be safe to dereference the new
		 * one (root->rnode) as far as dependent read barriers go.
		 */
		slot = to_free->slots[0];
		if (root->height > 1) {
			slot->parent = NULL;
			slot = ptr_to_indirect(slot);
		}
		root->rnode = slot;
		root->height--;

		/*
		 * We have a dilemma here. The node's slot[0] must not be
		 * NULLed in case there are concurrent lookups expecting to
		 * find the item. However if this was a bottom-level node,
		 * then it may be subject to the slot pointer being visible
		 * to callers dereferencing it. If item corresponding to
		 * slot[0] is subsequently deleted, these callers would expect
		 * their slot to become empty sooner or later.
		 *
		 * For example, lockless pagecache will look up a slot, deref
		 * the page pointer, and if the page is 0 refcount it means it
		 * was concurrently deleted from pagecache so try the deref
		 * again. Fortunately there is already a requirement for logic
		 * to retry the entire slot lookup -- the indirect pointer
		 * problem (replacing direct root node with an indirect pointer
		 * also results in a stale slot). So tag the slot as indirect
		 * to force callers to retry.
		 */
		if (root->height == 0)
			*((unsigned long *)&to_free->slots[0]) |=
						RADIX_TREE_INDIRECT_PTR;

		radix_tree_node_free(to_free);
	}
}

/**
 *	radix_tree_delete    -    delete an item from a radix tree
 *	@root:		radix tree root
 *	@index:		index key
 *
 *	Remove the item at @index from the radix tree rooted at @root.
 *
 *	Returns the address of the deleted item, or NULL if it was not present.
 */
void *radix_tree_delete(struct radix_tree_root *root, unsigned long index)
{
	struct radix_tree_node *node = NULL;
	struct radix_tree_node *slot = NULL;
	struct radix_tree_node *to_free;
	unsigned int height, shift;
	int uninitialized_var(offset);

	height = root->height;
	if (index > radix_tree_maxindex(height))
		goto out;

	slot = root->rnode;
	if (height == 0) {
		root->rnode = NULL;
		goto out;
	}
	slot = indirect_to_ptr(slot);
	shift = height * RADIX_TREE_MAP_SHIFT;

	do {
		if (slot == NULL)
			goto out;

		shift -= RADIX_TREE_MAP_SHIFT;
		offset = (index >> shift) & RADIX_TREE_MAP_MASK;
		node = slot;
		slot = slot->slots[offset];
	} while (shift);

	if (slot == NULL)
		goto out;

	to_free = NULL;
	/* Now free the nodes we do not need anymore */
	while (node) {
		node->slots[offset] = NULL;
		node->count--;
		/*
		 * Queue the node for deferred freeing after the
		 * last reference to it disappears (set NULL, above).
		 */
		if (to_free)
			radix_tree_node_free(to_free);

		if (node->count) {
			if (node == indirect_to_ptr(root->rnode))
				radix_tree_shrink(root);
			goto out;
		}

		/* Node with zero slots in use so free it */
		to_free = node;

		index >>= RADIX_TREE_MAP_SHIFT;
		offset = index & RADIX_TREE_MAP_MASK;
		node = node->parent;
	}

	root->height = 0;
	root->rnode = NULL;
	if (to_free)
		radix_tree_node_free(to_free);

out:
	return slot;
}
