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
 * @file radix-tree.h
 * @author Pranavkumar Sawargaonkar (pranav.sawargaonkar@gmail.com)
 * @brief Interface for Radix Trees.
 *
 * The header has been largely adapted from:
 * linux-xxx/include/linux/radix-tree.h
 *
 * Copyright (C) 2001 Momchil Velikov
 * Portions Copyright (C) 2001 Christoph Hellwig
 * Copyright (C) 2006 Nick Piggin
 * Copyright (C) 2012 Konstantin Khlebnikov
 * 
 * The original code is licensed under the GPL.
 */

#ifndef __RADIX_TREE_H
#define __RADIX_TREE_H

#include <vmm_error.h>
#include <vmm_types.h>
#include <vmm_spinlocks.h>

/*
 * An indirect pointer (root->rnode pointing to a radix_tree_node, rather
 * than a data item) is signalled by the low bit set in the root->rnode
 * pointer.
 *
 * In this case root->height is > 0, but the indirect pointer tests are
 * needed for RCU lookups (because root->height is unreliable). The only
 * time callers need worry about this is when doing a lookup_slot under
 * RCU.
 *
 * Indirect pointer in fact is also used to tag the last pointer of a node
 * when it is shrunk, before we rcu free the node. See shrink code for
 * details.
 */
#define RADIX_TREE_INDIRECT_PTR		1
/*
 * A common use of the radix tree is to store pointers to struct pages;
 * but shmem/tmpfs needs also to store swap entries in the same tree:
 * those are marked as exceptional entries to distinguish them.
 * EXCEPTIONAL_ENTRY tests the bit, EXCEPTIONAL_SHIFT shifts content past it.
 */
#define RADIX_TREE_EXCEPTIONAL_ENTRY	2
#define RADIX_TREE_EXCEPTIONAL_SHIFT	2

static inline int radix_tree_is_indirect_ptr(void *ptr)
{
	return (int)((unsigned long)ptr & RADIX_TREE_INDIRECT_PTR);
}

/*** radix-tree API starts here ***/

#define RADIX_TREE_MAX_TAGS 3

/* root tags are stored in gfp_mask, shifted by __GFP_BITS_SHIFT */
struct radix_tree_root {
	unsigned int		height;
	vmm_rwlock_t		lock;
	struct radix_tree_node	*rnode;
};

#define RADIX_TREE_INIT(name)	{					\
	.height = 0,							\
	.lock = __RWLOCK_INITIALIZER(name.lock),			\
	.rnode = NULL,							\
}

#define RADIX_TREE(name, mask) \
	struct radix_tree_root name = RADIX_TREE_INIT(name)

#define INIT_RADIX_TREE(root, mask)					\
do {									\
	(root)->height = 0;						\
	INIT_RW_LOCK(&(root)->lock);					\
	(root)->rnode = NULL;						\
} while (0)


/**
 * radix_tree_deref_retry	- check radix_tree_deref_slot
 * @arg:	pointer returned by radix_tree_deref_slot
 * Returns:	0 if retry is not required, otherwise retry is required
 *
 * radix_tree_deref_retry must be used with radix_tree_deref_slot.
 */
static inline int radix_tree_deref_retry(void *arg)
{
	return unlikely((unsigned long)arg & RADIX_TREE_INDIRECT_PTR);
}

/**
 * radix_tree_exceptional_entry	- radix_tree_deref_slot gave exceptional entry?
 * @arg:	value returned by radix_tree_deref_slot
 * Returns:	0 if well-aligned pointer, non-0 if exceptional entry.
 */
static inline int radix_tree_exceptional_entry(void *arg)
{
	/* Not unlikely because radix_tree_exception often tested first */
	return (unsigned long)arg & RADIX_TREE_EXCEPTIONAL_ENTRY;
}

/**
 * radix_tree_exception	- radix_tree_deref_slot returned either exception?
 * @arg:	value returned by radix_tree_deref_slot
 * Returns:	0 if well-aligned pointer, non-0 if either kind of exception.
 */
static inline int radix_tree_exception(void *arg)
{
	return unlikely((unsigned long)arg &
		(RADIX_TREE_INDIRECT_PTR | RADIX_TREE_EXCEPTIONAL_ENTRY));
}


int radix_tree_insert(struct radix_tree_root *, unsigned long, void *);
void *radix_tree_lookup(struct radix_tree_root *, unsigned long);
void **radix_tree_lookup_slot(struct radix_tree_root *, unsigned long);
void *radix_tree_delete(struct radix_tree_root *, unsigned long);
unsigned int
radix_tree_gang_lookup(struct radix_tree_root *root, void **results,
			unsigned long first_index, unsigned int max_items);
unsigned int radix_tree_gang_lookup_slot(struct radix_tree_root *root,
			void ***results, unsigned long *indices,
			unsigned long first_index, unsigned int max_items);
unsigned long radix_tree_next_hole(struct radix_tree_root *root,
				unsigned long index, unsigned long max_scan);
unsigned long radix_tree_prev_hole(struct radix_tree_root *root,
				unsigned long index, unsigned long max_scan);
void radix_tree_init(void);

/**
 * struct radix_tree_iter - radix tree iterator state
 *
 * @index:	index of current slot
 * @next_index:	next-to-last index for this chunk
 * @tags:	bit-mask for tag-iterating
 *
 * This radix tree iterator works in terms of "chunks" of slots.  A chunk is a
 * subinterval of slots contained within one radix tree leaf node.  It is
 * described by a pointer to its first slot and a struct radix_tree_iter
 * which holds the chunk's position in the tree and its size.  For tagged
 * iteration radix_tree_iter also holds the slots' bit-mask for one chosen
 * radix tree tag.
 */
struct radix_tree_iter {
	unsigned long	index;
	unsigned long	next_index;
	unsigned long	tags;
};

#define RADIX_TREE_ITER_TAG_MASK	0x00FF	/* tag index in lower byte */
#define RADIX_TREE_ITER_TAGGED		0x0100	/* lookup tagged slots */
#define RADIX_TREE_ITER_CONTIG		0x0200	/* stop at first hole */

/**
 * radix_tree_iter_init - initialize radix tree iterator
 *
 * @iter:	pointer to iterator state
 * @start:	iteration starting index
 * Returns:	NULL
 */
static inline void **
radix_tree_iter_init(struct radix_tree_iter *iter, unsigned long start)
{
	/*
	 * Leave iter->tags uninitialized. radix_tree_next_chunk() will fill it
	 * in the case of a successful tagged chunk lookup.  If the lookup was
	 * unsuccessful or non-tagged then nobody cares about ->tags.
	 *
	 * Set index to zero to bypass next_index overflow protection.
	 * See the comment in radix_tree_next_chunk() for details.
	 */
	iter->index = 0;
	iter->next_index = start;
	return NULL;
}

/**
 * radix_tree_next_chunk - find next chunk of slots for iteration
 *
 * @root:	radix tree root
 * @iter:	iterator state
 * @flags:	RADIX_TREE_ITER_* flags and tag index
 * Returns:	pointer to chunk first slot, or NULL if there no more left
 *
 * This function looks up the next chunk in the radix tree starting from
 * @iter->next_index.  It returns a pointer to the chunk's first slot.
 * Also it fills @iter with data about chunk: position in the tree (index),
 * its end (next_index), and constructs a bit mask for tagged iterating (tags).
 */
void **radix_tree_next_chunk(struct radix_tree_root *root,
			     struct radix_tree_iter *iter, unsigned flags);

/**
 * radix_tree_chunk_size - get current chunk size
 *
 * @iter:	pointer to radix tree iterator
 * Returns:	current chunk size
 */
static inline unsigned
radix_tree_chunk_size(struct radix_tree_iter *iter)
{
	return iter->next_index - iter->index;
}

/**
 * radix_tree_next_slot - find next slot in chunk
 *
 * @slot:	pointer to current slot
 * @iter:	pointer to interator state
 * @flags:	RADIX_TREE_ITER_*, should be constant
 * Returns:	pointer to next slot, or NULL if there no more left
 *
 * This function updates @iter->index in the case of a successful lookup.
 * For tagged lookup it also eats @iter->tags.
 */
static inline void **
radix_tree_next_slot(void **slot, struct radix_tree_iter *iter, unsigned flags)
{
	unsigned size = radix_tree_chunk_size(iter) - 1;

	while (size--) {
		slot++;
		iter->index++;
		if (likely(*slot))
			return slot;
		if (flags & RADIX_TREE_ITER_CONTIG) {
			/* forbid switching to the next chunk */
			iter->next_index = 0;
			break;
		}
	}
	return NULL;
}

/**
 * radix_tree_for_each_chunk - iterate over chunks
 *
 * @slot:	the void** variable for pointer to chunk first slot
 * @root:	the struct radix_tree_root pointer
 * @iter:	the struct radix_tree_iter pointer
 * @start:	iteration starting index
 * @flags:	RADIX_TREE_ITER_* and tag index
 *
 * Locks can be released and reacquired between iterations.
 */
#define radix_tree_for_each_chunk(slot, root, iter, start, flags)	\
	for (slot = radix_tree_iter_init(iter, start) ;			\
	      (slot = radix_tree_next_chunk(root, iter, flags)) ;)

/**
 * radix_tree_for_each_chunk_slot - iterate over slots in one chunk
 *
 * @slot:	the void** variable, at the beginning points to chunk first slot
 * @iter:	the struct radix_tree_iter pointer
 * @flags:	RADIX_TREE_ITER_*, should be constant
 *
 * This macro is designed to be nested inside radix_tree_for_each_chunk().
 * @slot points to the radix tree slot, @iter->index contains its index.
 */
#define radix_tree_for_each_chunk_slot(slot, iter, flags)		\
	for (; slot ; slot = radix_tree_next_slot(slot, iter, flags))

/**
 * radix_tree_for_each_slot - iterate over non-empty slots
 *
 * @slot:	the void** variable for pointer to slot
 * @root:	the struct radix_tree_root pointer
 * @iter:	the struct radix_tree_iter pointer
 * @start:	iteration starting index
 *
 * @slot points to radix tree slot, @iter->index contains its index.
 */
#define radix_tree_for_each_slot(slot, root, iter, start)		\
	for (slot = radix_tree_iter_init(iter, start) ;			\
	     slot || (slot = radix_tree_next_chunk(root, iter, 0)) ;	\
	     slot = radix_tree_next_slot(slot, iter, 0))

/**
 * radix_tree_for_each_contig - iterate over contiguous slots
 *
 * @slot:	the void** variable for pointer to slot
 * @root:	the struct radix_tree_root pointer
 * @iter:	the struct radix_tree_iter pointer
 * @start:	iteration starting index
 *
 * @slot points to radix tree slot, @iter->index contains its index.
 */
#define radix_tree_for_each_contig(slot, root, iter, start)		\
	for (slot = radix_tree_iter_init(iter, start) ;			\
	     slot || (slot = radix_tree_next_chunk(root, iter,		\
				RADIX_TREE_ITER_CONTIG)) ;		\
	     slot = radix_tree_next_slot(slot, iter,			\
				RADIX_TREE_ITER_CONTIG))

/**
 * radix_tree_for_each_tagged - iterate over tagged slots
 *
 * @slot:	the void** variable for pointer to slot
 * @root:	the struct radix_tree_root pointer
 * @iter:	the struct radix_tree_iter pointer
 * @start:	iteration starting index
 * @tag:	tag index
 *
 * @slot points to radix tree slot, @iter->index contains its index.
 */
#define radix_tree_for_each_tagged(slot, root, iter, start, tag)	\
	for (slot = radix_tree_iter_init(iter, start) ;			\
	     slot || (slot = radix_tree_next_chunk(root, iter,		\
			      RADIX_TREE_ITER_TAGGED | tag)) ;		\
	     slot = radix_tree_next_slot(slot, iter,			\
				RADIX_TREE_ITER_TAGGED))

#endif /* __RADIX_TREE_H */
