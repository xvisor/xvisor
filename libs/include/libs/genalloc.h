/*
 * Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWide.
 * All rights reserved.
 * Modified by Jimmy Durand Wesolowski <jimmy.durand-wesolowski@openwide.fr>
 *
 * Basic general purpose allocator for managing special purpose
 * memory, for example, memory that is not managed by the regular
 * kmalloc/kfree interface.  Uses for this includes on-device special
 * memory, uncached memory etc.
 *
 * It is safe to use the allocator in NMI handlers and other special
 * unblockable contexts that could otherwise deadlock on locks.  This
 * is implemented by using atomic operations and retries on any
 * conflicts.  The disadvantage is that there may be livelocks in
 * extreme cases.  For better scalability, one allocator can be used
 * for each CPU.
 *
 * The lockless operation only works if there is enough memory
 * available.  If new memory is added to the pool a lock has to be
 * still taken.  So any user relying on locklessness has to ensure
 * that sufficient memory is preallocated.
 *
 * The basic atomic operation of this allocator is cmpxchg on long.
 * On architectures that don't have NMI-safe cmpxchg implementation,
 * the allocator can NOT be used in NMI handler.  So code uses the
 * allocator in NMI handler should depend on
 * CONFIG_ARCH_HAVE_NMI_SAFE_CMPXCHG.
 *
 * This source code is licensed under the GNU General Public License,
 * Version 2.  See the file COPYING for more details.
 *
 * @file genalloc.h
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @brief Special purpose memory managing
 */


#ifndef __GENALLOC_H__
#define __GENALLOC_H__

struct vmm_device;
struct vmm_devtree_node;

#include <vmm_types.h>
#include <arch_atomic.h>
#include <libs/list.h>

/**
 * Allocation callback function type definition
 * @map: Pointer to bitmap
 * @size: The bitmap size in bits
 * @start: The bitnumber to start searching at
 * @nr: The number of zeroed bits we're looking for
 * @data: optional additional data used by @genpool_algo_t
 */
typedef unsigned long (*genpool_algo_t)(unsigned long *map,
			unsigned long size,
			unsigned long start,
			unsigned int nr,
			void *data);

/*
 *  General purpose special memory pool descriptor.
 */
struct gen_pool {
	vmm_spinlock_t lock;
	struct dlist chunks;	/* list of chunks in this pool */
	int min_alloc_order;		/* minimum allocation order */

	genpool_algo_t algo;		/* allocation function */
	void *data;
};

/*
 *  General purpose special memory pool chunk descriptor.
 */
struct gen_pool_chunk {
	struct list_head next_chunk;	/* next chunk in pool */
	atomic_t avail;
	physical_addr_t phys_addr;	/* physical starting address of memory chunk */
	unsigned long start_addr;	/* starting address of memory chunk */
	unsigned long end_addr;		/* ending address of memory chunk */
	unsigned long bits[0];		/* bitmap for allocating memory chunk */
};

extern struct gen_pool *gen_pool_create(int);
extern physical_addr_t gen_pool_virt_to_phys(struct gen_pool *pool,
					     unsigned long);
extern int gen_pool_add_virt(struct gen_pool *, unsigned long, physical_addr_t,
			     size_t);
/**
 * gen_pool_add - add a new chunk of special memory to the pool
 * @pool: pool to add new memory chunk to
 * @addr: starting address of memory chunk to add to pool
 * @size: size in bytes of the memory chunk to add to pool
 *
 * Add a new chunk of special memory to the specified pool.
 *
 * Returns 0 on success or a -ve errno on failure.
 */
static inline int gen_pool_add(struct gen_pool *pool, unsigned long addr,
			       size_t size)
{
	return gen_pool_add_virt(pool, addr, -1, size);
}
extern void gen_pool_destroy(struct gen_pool *);
extern unsigned long gen_pool_alloc(struct gen_pool *, size_t);
extern void *gen_pool_dma_alloc(struct gen_pool *pool, size_t size,
				dma_addr_t *dma);
extern void gen_pool_free(struct gen_pool *, unsigned long, size_t);
extern void gen_pool_for_each_chunk(struct gen_pool *,
	void (*)(struct gen_pool *, struct gen_pool_chunk *, void *), void *);
extern size_t gen_pool_avail(struct gen_pool *);
extern size_t gen_pool_size(struct gen_pool *);

extern void gen_pool_set_algo(struct gen_pool *pool, genpool_algo_t algo,
		void *data);

extern unsigned long gen_pool_first_fit(unsigned long *map, unsigned long size,
		unsigned long start, unsigned int nr, void *data);

extern unsigned long gen_pool_best_fit(unsigned long *map, unsigned long size,
		unsigned long start, unsigned int nr, void *data);

extern struct gen_pool *devm_gen_pool_create(struct vmm_device *dev,
					     int min_alloc_order);
extern struct gen_pool *dev_get_gen_pool(struct vmm_device *dev);

extern struct gen_pool *of_get_named_gen_pool(struct vmm_devtree_node *np,
					      const char *propname, int index);
#endif /* __GENALLOC_H__ */
