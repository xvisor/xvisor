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
 * Copyright 2005 (C) Jes Sorensen <jes@trained-monkey.org>
 *
 * This source code is licensed under the GNU General Public License,
 * Version 2.  See the file COPYING for more details.
 *
 * @file genalloc.c
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @brief Special purpose memory managing
 */

#include <vmm_heap.h>
#include <vmm_devdrv.h>
#include <vmm_devres.h>
#include <vmm_devtree.h>
#include <vmm_spinlocks.h>
#include <libs/bitmap.h>
#include <libs/genalloc.h>
#include <arch_barrier.h>
#include <asm/atomic.h>

void wrong_size_cmpxchg(volatile void *ptr)
{
	BUG();
}

static inline unsigned long __cmpxchg_mb(volatile void *ptr, unsigned long old,
					 unsigned long new, int size)
{
	unsigned long prev;

	/*
	 * Sanity checking, compile-time.
	 */
	if (size == 8 && sizeof(unsigned long) != 8)
		wrong_size_cmpxchg(ptr);

        switch (size) {
        case 1: prev = *(u8 *)ptr;
                if (prev == old)
                        *(u8 *)ptr = (u8)new;
                break;
        case 2: prev = *(u16 *)ptr;
                if (prev == old)
                        *(u16 *)ptr = (u16)new;
                break;
        case 4: prev = *(u32 *)ptr;
                if (prev == old)
                        *(u32 *)ptr = (u32)new;
                break;
        case 8: prev = *(u64 *)ptr;
                if (prev == old)
                        *(u64 *)ptr = (u64)new;
                break;
        default:
                wrong_size_cmpxchg(ptr);
        }
        return prev;
}

#define cmpxchg(ptr,o,n)                                                \
	((__typeof__(*(ptr)))__cmpxchg_mb((ptr),			\
					  (unsigned long)(o),		\
					  (unsigned long)(n),           \
					  sizeof(*(ptr))))

static int set_bits_ll(unsigned long *addr, unsigned long mask_to_set)
{
	unsigned long val, nval;

	nval = *addr;
	do {
		val = nval;
		if (val & mask_to_set)
			return VMM_EBUSY;
		arch_smp_mb();
	} while ((nval = cmpxchg(addr, val, val | mask_to_set)) != val);

	return 0;
}

static int clear_bits_ll(unsigned long *addr, unsigned long mask_to_clear)
{
	unsigned long val, nval;

	nval = *addr;
	do {
		val = nval;
		if ((val & mask_to_clear) != mask_to_clear)
			return VMM_EBUSY;
		arch_smp_mb();
	} while ((nval = cmpxchg(addr, val, val & ~mask_to_clear)) != val);

	return 0;
}

/*
 * bitmap_set_ll - set the specified number of bits at the specified position
 * @map: pointer to a bitmap
 * @start: a bit position in @map
 * @nr: number of bits to set
 *
 * Set @nr bits start from @start in @map lock-lessly. Several users
 * can set/clear the same bitmap simultaneously without lock. If two
 * users set the same bit, one user will return remain bits, otherwise
 * return 0.
 */
static int bitmap_set_ll(unsigned long *map, int start, int nr)
{
	unsigned long *p = map + BIT_WORD(start);
	const int size = start + nr;
	int bits_to_set = BITS_PER_LONG - (start % BITS_PER_LONG);
	unsigned long mask_to_set = BITMAP_FIRST_WORD_MASK(start);

	while (nr - bits_to_set >= 0) {
		if (set_bits_ll(p, mask_to_set))
			return nr;
		nr -= bits_to_set;
		bits_to_set = BITS_PER_LONG;
		mask_to_set = ~0UL;
		p++;
	}
	if (nr) {
		mask_to_set &= BITMAP_LAST_WORD_MASK(size);
		if (set_bits_ll(p, mask_to_set))
			return nr;
	}

	return 0;
}

/*
 * bitmap_clear_ll - clear the specified number of bits at the specified position
 * @map: pointer to a bitmap
 * @start: a bit position in @map
 * @nr: number of bits to set
 *
 * Clear @nr bits start from @start in @map lock-lessly. Several users
 * can set/clear the same bitmap simultaneously without lock. If two
 * users clear the same bit, one user will return remain bits,
 * otherwise return 0.
 */
static int bitmap_clear_ll(unsigned long *map, int start, int nr)
{
	unsigned long *p = map + BIT_WORD(start);
	const int size = start + nr;
	int bits_to_clear = BITS_PER_LONG - (start % BITS_PER_LONG);
	unsigned long mask_to_clear = BITMAP_FIRST_WORD_MASK(start);

	while (nr - bits_to_clear >= 0) {
		if (clear_bits_ll(p, mask_to_clear))
			return nr;
		nr -= bits_to_clear;
		bits_to_clear = BITS_PER_LONG;
		mask_to_clear = ~0UL;
		p++;
	}
	if (nr) {
		mask_to_clear &= BITMAP_LAST_WORD_MASK(size);
		if (clear_bits_ll(p, mask_to_clear))
			return nr;
	}

	return 0;
}

/**
 * gen_pool_create - create a new special memory pool
 * @min_alloc_order: log base 2 of number of bytes each bitmap bit represents
 *
 * Create a new special memory pool that can be used to manage special purpose
 * memory not managed by the regular kmalloc/kfree interface.
 */
struct gen_pool *gen_pool_create(int min_alloc_order)
{
	struct gen_pool *pool;

	pool = vmm_malloc(sizeof(struct gen_pool));
	if (pool != NULL) {
		INIT_SPIN_LOCK(&pool->lock);
		INIT_LIST_HEAD(&pool->chunks);
		pool->min_alloc_order = min_alloc_order;
		pool->algo = gen_pool_first_fit;
		pool->data = NULL;
	}
	return pool;
}

/**
 * gen_pool_add_virt - add a new chunk of special memory to the pool
 * @pool: pool to add new memory chunk to
 * @virt: virtual starting address of memory chunk to add to pool
 * @phys: physical starting address of memory chunk to add to pool
 * @size: size in bytes of the memory chunk to add to pool
 *
 * Add a new chunk of special memory to the specified pool.
 *
 * Returns 0 on success or a -ve errno on failure.
 */
int gen_pool_add_virt(struct gen_pool *pool, unsigned long virt,
		      physical_addr_t phys, size_t size)
{
	struct gen_pool_chunk *chunk;
	int nbits = size >> pool->min_alloc_order;
	int nbytes = sizeof(struct gen_pool_chunk) +
		BITS_TO_LONGS(nbits) * sizeof(long);

	chunk = vmm_zalloc(nbytes);
	if (unlikely(chunk == NULL))
		return VMM_ENOMEM;

	chunk->phys_addr = phys;
	chunk->start_addr = virt;
	chunk->end_addr = virt + size;
	arch_atomic_write(&chunk->avail, size);

	vmm_spin_lock(&pool->lock);
	list_add(&chunk->next_chunk, &pool->chunks);
	vmm_spin_unlock(&pool->lock);

	return 0;
}

/**
 * gen_pool_virt_to_phys - return the physical address of memory
 * @pool: pool to allocate from
 * @addr: starting address of memory
 *
 * Returns the physical address on success, or -1 on error.
 */
physical_addr_t gen_pool_virt_to_phys(struct gen_pool *pool, unsigned long addr)
{
	struct gen_pool_chunk *chunk;
	physical_addr_t paddr = -1;

	list_for_each_entry(chunk, &pool->chunks, next_chunk) {
		if (addr >= chunk->start_addr && addr < chunk->end_addr) {
			paddr = chunk->phys_addr + (addr - chunk->start_addr);
			break;
		}
	}

	return paddr;
}

/**
 * gen_pool_destroy - destroy a special memory pool
 * @pool: pool to destroy
 *
 * Destroy the specified special memory pool. Verifies that there are no
 * outstanding allocations.
 */
void gen_pool_destroy(struct gen_pool *pool)
{
	struct list_head *_chunk, *_next_chunk;
	struct gen_pool_chunk *chunk;
	int order = pool->min_alloc_order;
	int bit, end_bit;

	list_for_each_safe(_chunk, _next_chunk, &pool->chunks) {
		chunk = list_entry(_chunk, struct gen_pool_chunk, next_chunk);
		list_del(&chunk->next_chunk);

		end_bit = (chunk->end_addr - chunk->start_addr) >> order;
		bit = find_next_bit(chunk->bits, end_bit, 0);
		BUG_ON(bit < end_bit);

		vmm_free(chunk);
	}
	vmm_free(pool);
	return;
}

/**
 * gen_pool_alloc - allocate special memory from the pool
 * @pool: pool to allocate from
 * @size: number of bytes to allocate from the pool
 *
 * Allocate the requested number of bytes from the specified pool.
 * Uses the pool allocation function (with first-fit algorithm by default).
 * Can not be used in NMI handler on architectures without
 * NMI-safe cmpxchg implementation.
 */
unsigned long gen_pool_alloc(struct gen_pool *pool, size_t size)
{
	struct gen_pool_chunk *chunk;
	unsigned long addr = 0;
	int order = pool->min_alloc_order;
	int nbits, start_bit = 0, end_bit, remain;

	if (size == 0)
		return 0;

	nbits = (size + (1UL << order) - 1) >> order;
	list_for_each_entry(chunk, &pool->chunks, next_chunk) {
		if (size > arch_atomic_read(&chunk->avail))
			continue;

		end_bit = (chunk->end_addr - chunk->start_addr) >> order;
retry:
		start_bit = pool->algo(chunk->bits, end_bit, start_bit, nbits,
				pool->data);
		if (start_bit >= end_bit)
			continue;
		remain = bitmap_set_ll(chunk->bits, start_bit, nbits);
		if (remain) {
			remain = bitmap_clear_ll(chunk->bits, start_bit,
						 nbits - remain);
			BUG_ON(remain);
			goto retry;
		}

		addr = chunk->start_addr + ((unsigned long)start_bit << order);
		size = nbits << order;
		arch_atomic_sub(&chunk->avail, size);
		break;
	}
	return addr;
}

/**
 * gen_pool_dma_alloc - allocate special memory from the pool for DMA usage
 * @pool: pool to allocate from
 * @size: number of bytes to allocate from the pool
 * @dma: dma-view physical address
 *
 * Allocate the requested number of bytes from the specified pool.
 * Uses the pool allocation function (with first-fit algorithm by default).
 * Can not be used in NMI handler on architectures without
 * NMI-safe cmpxchg implementation.
 */
void *gen_pool_dma_alloc(struct gen_pool *pool, size_t size, dma_addr_t *dma)
{
	unsigned long vaddr;

	if (!pool)
		return NULL;

	vaddr = gen_pool_alloc(pool, size);
	if (!vaddr)
		return NULL;

	*dma = gen_pool_virt_to_phys(pool, vaddr);

	return (void *)vaddr;
}

/**
 * gen_pool_free - free allocated special memory back to the pool
 * @pool: pool to free to
 * @addr: starting address of memory to free back to pool
 * @size: size in bytes of memory to free
 *
 * Free previously allocated special memory back to the specified
 * pool.  Can not be used in NMI handler on architectures without
 * NMI-safe cmpxchg implementation.
 */
void gen_pool_free(struct gen_pool *pool, unsigned long addr, size_t size)
{
	struct gen_pool_chunk *chunk;
	int order = pool->min_alloc_order;
	int start_bit, nbits, remain;

	nbits = (size + (1UL << order) - 1) >> order;
	list_for_each_entry(chunk, &pool->chunks, next_chunk) {
		if (addr >= chunk->start_addr && addr < chunk->end_addr) {
			BUG_ON(addr + size > chunk->end_addr);
			start_bit = (addr - chunk->start_addr) >> order;
			remain = bitmap_clear_ll(chunk->bits, start_bit, nbits);
			BUG_ON(remain);
			size = nbits << order;
			arch_atomic_add(&chunk->avail, size);
			return;
		}
	}
	BUG();
}

/**
 * gen_pool_for_each_chunk - call func for every chunk of generic memory pool
 * @pool:	the generic memory pool
 * @func:	func to call
 * @data:	additional data used by @func
 *
 * Call @func for every chunk of generic memory pool.  The @func is
 * called with rcu_read_lock held.
 */
void gen_pool_for_each_chunk(struct gen_pool *pool,
	void (*func)(struct gen_pool *pool, struct gen_pool_chunk *chunk, void *data),
	void *data)
{
	struct gen_pool_chunk *chunk;

	list_for_each_entry(chunk, &(pool)->chunks, next_chunk)
		func(pool, chunk, data);
}

/**
 * gen_pool_avail - get available free space of the pool
 * @pool: pool to get available free space
 *
 * Return available free space of the specified pool.
 */
size_t gen_pool_avail(struct gen_pool *pool)
{
	struct gen_pool_chunk *chunk;
	size_t avail = 0;

	list_for_each_entry(chunk, &pool->chunks, next_chunk)
		avail += arch_atomic_read(&chunk->avail);
	return avail;
}

/**
 * gen_pool_size - get size in bytes of memory managed by the pool
 * @pool: pool to get size
 *
 * Return size in bytes of memory managed by the pool.
 */
size_t gen_pool_size(struct gen_pool *pool)
{
	struct gen_pool_chunk *chunk;
	size_t size = 0;

	list_for_each_entry(chunk, &pool->chunks, next_chunk)
		size += chunk->end_addr - chunk->start_addr;
	return size;
}

/**
 * gen_pool_set_algo - set the allocation algorithm
 * @pool: pool to change allocation algorithm
 * @algo: custom algorithm function
 * @data: additional data used by @algo
 *
 * Call @algo for each memory allocation in the pool.
 * If @algo is NULL use gen_pool_first_fit as default
 * memory allocation function.
 */
void gen_pool_set_algo(struct gen_pool *pool, genpool_algo_t algo, void *data)
{
	pool->algo = algo;
	if (!pool->algo)
		pool->algo = gen_pool_first_fit;

	pool->data = data;
}

/**
 * gen_pool_first_fit - find the first available region
 * of memory matching the size requirement (no alignment constraint)
 * @map: The address to base the search on
 * @size: The bitmap size in bits
 * @start: The bitnumber to start searching at
 * @nr: The number of zeroed bits we're looking for
 * @data: additional data - unused
 */
unsigned long gen_pool_first_fit(unsigned long *map, unsigned long size,
		unsigned long start, unsigned int nr, void *data)
{
	return bitmap_find_free_region(map, nr, size);
}

#if 0
/**
 * gen_pool_best_fit - find the best fitting region of memory
 * macthing the size requirement (no alignment constraint)
 * @map: The address to base the search on
 * @size: The bitmap size in bits
 * @start: The bitnumber to start searching at
 * @nr: The number of zeroed bits we're looking for
 * @data: additional data - unused
 *
 * Iterate over the bitmap to find the smallest free region
 * which we can allocate the memory.
 */
unsigned long gen_pool_best_fit(unsigned long *map, unsigned long size,
		unsigned long start, unsigned int nr, void *data)
{
	unsigned long start_bit = size;
	unsigned long len = size + 1;
	unsigned long index;

	index = bitmap_find_next_zero_area(map, size, start, nr, 0);

	while (index < size) {
		int next_bit = find_next_bit(map, size, index + nr);
		if ((next_bit - index) < len) {
			len = next_bit - index;
			start_bit = index;
			if (len == nr)
				return start_bit;
		}
		index = bitmap_find_next_zero_area(map, size,
						   next_bit + 1, nr, 0);
	}

	return start_bit;
}
#endif /* 0 */

static void devm_gen_pool_release(struct vmm_device *dev, void *res)
{
	gen_pool_destroy(*(struct gen_pool **)res);
}

/**
 * devm_gen_pool_create - managed gen_pool_create
 * @dev: device that provides the gen_pool
 * @min_alloc_order: log base 2 of number of bytes each bitmap bit represents
 * @nid: node id of the node the pool structure should be allocated on, or -1
 *
 * Create a new special memory pool that can be used to manage special purpose
 * memory not managed by the regular kmalloc/kfree interface. The pool will be
 * automatically destroyed by the device management code.
 */
struct gen_pool *devm_gen_pool_create(struct vmm_device *dev,
				      int min_alloc_order)
{
	struct gen_pool **ptr, *pool;

	ptr = vmm_devres_alloc(devm_gen_pool_release, sizeof(*ptr));

	pool = gen_pool_create(min_alloc_order);
	if (pool) {
		*ptr = pool;
		vmm_devres_add(dev, ptr);
	} else {
		vmm_devres_free(ptr);
	}

	return pool;
}

/**
 * dev_get_gen_pool - Obtain the gen_pool (if any) for a device
 * @dev: device to retrieve the gen_pool from
 * @name: Optional name for the gen_pool, usually NULL
 *
 * Returns the gen_pool for the device if one is present, or NULL.
 */
struct gen_pool *dev_get_gen_pool(struct vmm_device *dev)
{
	struct gen_pool **p = vmm_devres_find(dev, devm_gen_pool_release, NULL,
					      NULL);

	if (!p)
		return NULL;
	return *p;
}

/**
 * of_get_named_gen_pool - find a pool by phandle property
 * @np: device node
 * @propname: property name containing phandle(s)
 * @index: index into the phandle array
 *
 * Returns the pool that contains the chunk starting at the physical
 * address of the device tree node pointed at by the phandle property,
 * or NULL if not found.
 */
struct gen_pool *of_get_named_gen_pool(struct vmm_devtree_node *np,
				       const char *propname, int index)
{
	int found = 0;
	struct vmm_bus *platform_bus = NULL;
	struct vmm_device *dev;
	struct vmm_devtree_node *np_pool;

	np_pool = vmm_devtree_parse_phandle(np, propname, index);
	if (!np_pool)
		return NULL;

	platform_bus = vmm_devdrv_find_bus("platform");
	if (!platform_bus) {
		vmm_devtree_dref_node(np_pool);
		return NULL;
	}

	list_for_each_entry(dev, &platform_bus->device_list, bus_head) {
		if (dev->of_node == np_pool) {
			found = 1;
			break;
		}
	}

	vmm_devtree_dref_node(np_pool);

	if (!found)
		return NULL;

	return dev_get_gen_pool(dev);
}



























































