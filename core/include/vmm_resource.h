/**
 * Copyright (c) 2014 Himanshu Chauhan.
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
 * @file vmm_resource.h
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @author Anup Patel (anup@brainfault.org)
 * @brief Resource management for arbitrary resources (including
 * host IO space and host memory space.
 *
 * This header has been largely adapted from Linux sources:
 * commit 97bf6af1f928216fd6c5a66e8a57bfa95a659672
 * Linux 3.19-rc1
 *
 *	linux/include/linux/ioport.h
 *
 * ioport.h	Definitions of routines for detecting, reserving and
 *		allocating system resources.
 *
 * Authors:	Linus Torvalds
 */

#ifndef __VMM_RESOURCE_H__
#define __VMM_RESOURCE_H__

#include <vmm_types.h>

struct vmm_device;

/*
 * Resources are tree-like, allowing
 * nesting etc..
 */
struct vmm_resource {
	resource_size_t start;
	resource_size_t end;
	const char *name;
	unsigned long flags;
	struct vmm_resource *parent, *sibling, *child;
};

/*
 * IO resources have these defined flags.
 */
#define VMM_IORESOURCE_BITS		0x000000ff	/* Bus-specific bits */

#define VMM_IORESOURCE_TYPE_BITS	0x00001f00	/* Resource type */
#define VMM_IORESOURCE_IO		0x00000100	/* PCI/ISA I/O ports */
#define VMM_IORESOURCE_MEM		0x00000200
#define VMM_IORESOURCE_REG		0x00000300	/* Register offsets */
#define VMM_IORESOURCE_IRQ		0x00000400
#define VMM_IORESOURCE_DMA		0x00000800
#define VMM_IORESOURCE_BUS		0x00001000

#define VMM_IORESOURCE_PREFETCH		0x00002000	/* No side effects */
#define VMM_IORESOURCE_READONLY		0x00004000
#define VMM_IORESOURCE_CACHEABLE	0x00008000
#define VMM_IORESOURCE_RANGELENGTH	0x00010000
#define VMM_IORESOURCE_SHADOWABLE	0x00020000

#define VMM_IORESOURCE_SIZEALIGN	0x00040000	/* size indicates alignment */
#define VMM_IORESOURCE_STARTALIGN	0x00080000	/* start field is alignment */

#define VMM_IORESOURCE_MEM_64		0x00100000
#define VMM_IORESOURCE_WINDOW		0x00200000	/* forwarded by bridge */
#define VMM_IORESOURCE_MUXED		0x00400000	/* Resource is software muxed */

#define VMM_IORESOURCE_EXCLUSIVE	0x08000000	/* Userland may not map this resource */
#define VMM_IORESOURCE_DISABLED		0x10000000
#define VMM_IORESOURCE_UNSET		0x20000000	/* No address assigned yet */
#define VMM_IORESOURCE_AUTO		0x40000000
#define VMM_IORESOURCE_BUSY		0x80000000	/* Driver has marked this resource busy */

/* PnP IRQ specific bits (VMM_IORESOURCE_BITS) */
#define VMM_IORESOURCE_IRQ_HIGHEDGE	(1<<0)
#define VMM_IORESOURCE_IRQ_LOWEDGE	(1<<1)
#define VMM_IORESOURCE_IRQ_HIGHLEVEL	(1<<2)
#define VMM_IORESOURCE_IRQ_LOWLEVEL	(1<<3)
#define VMM_IORESOURCE_IRQ_SHAREABLE	(1<<4)
#define VMM_IORESOURCE_IRQ_OPTIONAL 	(1<<5)

/* PnP DMA specific bits (VMM_IORESOURCE_BITS) */
#define VMM_IORESOURCE_DMA_TYPE_MASK	(3<<0)
#define VMM_IORESOURCE_DMA_8BIT		(0<<0)
#define VMM_IORESOURCE_DMA_8AND16BIT	(1<<0)
#define VMM_IORESOURCE_DMA_16BIT	(2<<0)

#define VMM_IORESOURCE_DMA_MASTER	(1<<2)
#define VMM_IORESOURCE_DMA_BYTE		(1<<3)
#define VMM_IORESOURCE_DMA_WORD		(1<<4)

#define VMM_IORESOURCE_DMA_SPEED_MASK	(3<<6)
#define VMM_IORESOURCE_DMA_COMPATIBLE	(0<<6)
#define VMM_IORESOURCE_DMA_TYPEA	(1<<6)
#define VMM_IORESOURCE_DMA_TYPEB	(2<<6)
#define VMM_IORESOURCE_DMA_TYPEF	(3<<6)

/* PnP memory I/O specific bits (VMM_IORESOURCE_BITS) */
#define VMM_IORESOURCE_MEM_WRITEABLE	(1<<0)	/* dup: VMM_IORESOURCE_READONLY */
#define VMM_IORESOURCE_MEM_CACHEABLE	(1<<1)	/* dup: VMM_IORESOURCE_CACHEABLE */
#define VMM_IORESOURCE_MEM_RANGELENGTH	(1<<2)	/* dup: VMM_IORESOURCE_RANGELENGTH */
#define VMM_IORESOURCE_MEM_TYPE_MASK	(3<<3)
#define VMM_IORESOURCE_MEM_8BIT		(0<<3)
#define VMM_IORESOURCE_MEM_16BIT	(1<<3)
#define VMM_IORESOURCE_MEM_8AND16BIT	(2<<3)
#define VMM_IORESOURCE_MEM_32BIT	(3<<3)
#define VMM_IORESOURCE_MEM_SHADOWABLE	(1<<5)	/* dup: VMM_IORESOURCE_SHADOWABLE */
#define VMM_IORESOURCE_MEM_EXPANSIONROM	(1<<6)

/* PnP I/O specific bits (VMM_IORESOURCE_BITS) */
#define VMM_IORESOURCE_IO_16BIT_ADDR	(1<<0)
#define VMM_IORESOURCE_IO_FIXED		(1<<1)

/* PCI ROM control bits (VMM_IORESOURCE_BITS) */
#define VMM_IORESOURCE_ROM_ENABLE	(1<<0)	/* ROM is enabled, same as PCI_ROM_ADDRESS_ENABLE */
#define VMM_IORESOURCE_ROM_SHADOW	(1<<1)	/* ROM is copy at C000:0 */
#define VMM_IORESOURCE_ROM_COPY		(1<<2)	/* ROM is alloc'd copy, resource field overlaid */
#define VMM_IORESOURCE_ROM_BIOS_COPY	(1<<3)	/* ROM is BIOS copy, resource field overlaid */

/* PCI control bits.  Shares VMM_IORESOURCE_BITS with above PCI ROM.  */
#define VMM_IORESOURCE_PCI_FIXED	(1<<4)	/* Do not move resource */


/* helpers to define resources */
#define DEFINE_RES_NAMED(_start, _size, _name, _flags)		\
	{								\
		.start = (_start),					\
		.end = (_start) + (_size) - 1,				\
		.name = (_name),					\
		.flags = (_flags),					\
	}

#define DEFINE_RES_IO_NAMED(_start, _size, _name)			\
	DEFINE_RES_NAMED((_start), (_size), (_name), VMM_IORESOURCE_IO)
#define DEFINE_RES_IO(_start, _size)				\
	DEFINE_RES_IO_NAMED((_start), (_size), NULL)

#define DEFINE_RES_MEM_NAMED(_start, _size, _name)			\
	DEFINE_RES_NAMED((_start), (_size), (_name), VMM_IORESOURCE_MEM)
#define DEFINE_RES_MEM(_start, _size)				\
	DEFINE_RES_MEM_NAMED((_start), (_size), NULL)

#define DEFINE_RES_IRQ_NAMED(_irq, _name)				\
	DEFINE_RES_NAMED((_irq), 1, (_name), VMM_IORESOURCE_IRQ)
#define DEFINE_RES_IRQ(_irq)					\
	DEFINE_RES_IRQ_NAMED((_irq), NULL)

#define DEFINE_RES_DMA_NAMED(_dma, _name)				\
	DEFINE_RES_NAMED((_dma), 1, (_name), VMM_IORESOURCE_DMA)
#define DEFINE_RES_DMA(_dma)					\
	DEFINE_RES_DMA_NAMED((_dma), NULL)

/* PC/ISA/whatever - the normal PC address spaces: IO and memory */
extern struct vmm_resource vmm_hostio_resource;
extern struct vmm_resource vmm_hostmem_resource;

/**
 * Request and reserve an I/O or memory resource
 * @root: root resource descriptor
 * @new: resource descriptor desired by caller
 *
 * Returns NULL for success, conflict resource on error.
 */
struct vmm_resource *vmm_request_resource_conflict(struct vmm_resource *root,
						   struct vmm_resource *new);

/**
 * Request and reserve an I/O or memory resource
 * @root: root resource descriptor
 * @new: resource descriptor desired by caller
 *
 * Returns 0 for success, negative error code on error.
 */
int vmm_request_resource(struct vmm_resource *root, struct vmm_resource *new);

/**
 * Release a previously reserved resource
 * @old: resource pointer
 */
int vmm_release_resource(struct vmm_resource *new);

void vmm_release_child_resources(struct vmm_resource *new);

/**
 * This function calls callback against all memory range of "System RAM"
 * which are marked as VMM_IORESOURCE_MEM and IORESOUCE_BUSY.
 * Now, this function is only for "System RAM".
 */
int vmm_walk_system_ram_range(unsigned long start_pfn,
			unsigned long nr_pages, void *arg,
			int (*func)(unsigned long, unsigned long, void *));

/**
 * This function calls callback against all memory range of "System RAM"
 * which are marked as VMM_IORESOURCE_MEM and IORESOUCE_BUSY.
 * Now, this function is only for "System RAM". This function deals with
 * full ranges and not pfn. If resources are not pfn aligned, dealing
 * with pfn can truncate ranges.
 */
int vmm_walk_system_ram_res(u64 start, u64 end, void *arg,
			    int (*func)(u64, u64, void *));

/**
 * Walk through hostmem resources and call func() with matching resource
 * ranges. This walks through whole tree and not just first level children.
 * All the memory ranges which overlap start,end and also match flags and
 * name are valid candidates.
 *
 * @name: name of resource
 * @flags: resource flags
 * @start: start addr
 * @end: end addr
 */
int vmm_walk_hostmem_res(char *name, unsigned long flags,
			 u64 start, u64 end, void *arg,
			 int (*func)(u64, u64, void *));

/**
 * Inserts resource in the resource tree
 * @parent: parent of the new resource
 * @new: new resource to insert
 *
 * Returns NULL on success, conflict resource if the resource can't be inserted.
 *
 * This function is equivalent to request_resource_conflict when no conflict
 * happens. If a conflict happens, and the conflicting resources
 * entirely fit within the range of the new resource, then the new
 * resource is inserted and the conflicting resources become children of
 * the new resource.
 */
struct vmm_resource *vmm_insert_resource_conflict(struct vmm_resource *parent,
						  struct vmm_resource *new);

/**
 * Inserts a resource in the resource tree
 * @parent: parent of the new resource
 * @new: new resource to insert
 *
 * Returns 0 on success, VMM_EBUSY if the resource can't be inserted.
 */
int vmm_insert_resource(struct vmm_resource *parent,
			struct vmm_resource *new);

/**
 * Insert a resource into the resource tree
 * @root: root resource descriptor
 * @new: new resource to insert
 *
 * Insert a resource into the resource tree, possibly expanding it in order
 * to make it encompass any conflicting resources.
 */
void vmm_insert_resource_expand_to_fit(struct vmm_resource *root,
					struct vmm_resource *new);

/**
 * Allocate empty slot in the resource tree given range & alignment.
 * The resource will be reallocated with a new size if it was already allocated
 *
 * @root: root resource descriptor
 * @new: resource descriptor desired by caller
 * @size: requested resource region size
 * @min: minimum boundary to allocate
 * @max: maximum boundary to allocate
 * @align: alignment requested, in bytes
 * @alignf: alignment function, optional, called if not NULL
 * @alignf_data: arbitrary data to pass to the @alignf function
 */
int vmm_allocate_resource(struct vmm_resource *root, struct vmm_resource *new,
			  resource_size_t size, resource_size_t min,
			  resource_size_t max, resource_size_t align,
			  resource_size_t (*alignf)(void *,
						const struct vmm_resource *,
						resource_size_t,
						resource_size_t),
						void *alignf_data);

/**
 * Find an existing resource by a resource start address
 * @root: root resource descriptor
 * @start: resource start address
 *
 * Returns a pointer to the resource if found, NULL otherwise
 */
struct vmm_resource *vmm_lookup_resource(struct vmm_resource *root,
					 resource_size_t start);

/**
 * Modify a resource's start and size
 * @res: resource to modify
 * @start: new start value
 * @size: new size
 *
 * Given an existing resource, change its start and size to match the
 * arguments.  Returns 0 on success, VMM_EBUSY if it can't fit.
 * Existing children of the resource are assumed to be immutable.
 */
int vmm_adjust_resource(struct vmm_resource *res,
			resource_size_t start,
			resource_size_t size);

/**
 * Calculate resource's alignment
 * @res: resource pointer
 *
 * Returns alignment on success, 0 (invalid alignment) on failure.
 */
resource_size_t vmm_resource_alignment(struct vmm_resource *res);

static inline resource_size_t vmm_resource_size(const struct vmm_resource *res)
{
	return res->end - res->start + 1;
}

static inline unsigned long vmm_resource_type(const struct vmm_resource *res)
{
	return res->flags & VMM_IORESOURCE_TYPE_BITS;
}

/* True iff r1 completely contains r2 */
static inline bool vmm_resource_contains(struct vmm_resource *r1,
					 struct vmm_resource *r2)
{
	if (vmm_resource_type(r1) != vmm_resource_type(r2))
		return FALSE;
	if ((r1->flags & VMM_IORESOURCE_UNSET) ||
	    (r2->flags & VMM_IORESOURCE_UNSET))
		return FALSE;
	return r1->start <= r2->start && r1->end >= r2->end;
}

void vmm_reserve_region_with_split(struct vmm_resource *root,
				   resource_size_t start,
				   resource_size_t end,
				   const char *name);

/**
 * Create a new busy resource region
 * @parent: parent resource descriptor
 * @start: resource start address
 * @n: resource region size
 * @name: reserving caller's ID string
 * @flags: IO resource flags
 */
struct vmm_resource *__vmm_request_region(struct vmm_resource *parent,
					  resource_size_t start,
					  resource_size_t n,
					  const char *name, int flags);

/* Convenience shorthand with allocation */
#define vmm_request_region(start,n,name)		\
	__vmm_request_region(&vmm_hostio_resource, (start), (n), (name), 0)
#define vmm_request_muxed_region(start,n,name)		\
	__vmm_request_region(&vmm_hostio_resource, (start), (n), (name), VMM_IORESOURCE_MUXED)
#define __vmm_request_mem_region(start,n,name,excl) 	\
	__vmm_request_region(&vmm_hostmem_resource, (start), (n), (name), excl)
#define vmm_request_mem_region(start,n,name)		\
	__vmm_request_region(&vmm_hostmem_resource, (start), (n), (name), 0)
#define vmm_request_mem_region_exclusive(start,n,name)	\
	__vmm_request_region(&vmm_hostmem_resource, (start), (n), (name), VMM_IORESOURCE_EXCLUSIVE)
#define vmm_rename_region(region, newname)		\
	do { (region)->name = (newname); } while (0)

/**
 * Check if a resource region is busy or free
 * @parent: parent resource descriptor
 * @start: resource start address
 * @n: resource region size
 *
 * Returns 0 if the region is free at the moment it is checked,
 * returns VMM_EBUSY if the region is busy.
 *
 * NOTE:
 * This function is deprecated because its use is racy.
 * Even if it returns 0, a subsequent call to request_region()
 * may fail because another driver etc. just allocated the region.
 * Do NOT use it.  It will be removed from the kernel.
 */
int __vmm_check_region(struct vmm_resource *,
			resource_size_t, resource_size_t);

/**
 * Release a previously reserved resource region
 * @parent: parent resource descriptor
 * @start: resource start address
 * @n: resource region size
 *
 * The described resource region must match a currently busy region.
 */
void __vmm_release_region(struct vmm_resource *,
			resource_size_t, resource_size_t);

/* Compatibility cruft */
#define vmm_release_region(start,n)				\
	__vmm_release_region(&vmm_hostio_resource, (start), (n))
#define vmm_check_mem_region(start,n)			\
	__vmm_check_region(&vmm_hostmem_resource, (start), (n))
#define vmm_release_mem_region(start,n)			\
	__vmm_release_region(&vmm_hostmem_resource, (start), (n))

#ifdef CONFIG_MEMORY_HOTREMOVE
/**
 * Release a previously reserved memory region
 * @parent: parent resource descriptor
 * @start: resource start address
 * @size: resource region size
 *
 * This interface is intended for memory hot-delete.  The requested region
 * is released from a currently busy memory resource.  The requested region
 * must either match exactly or fit into a single busy resource entry.  In
 * the latter case, the remaining resource is adjusted accordingly.
 * Existing children of the busy memory resource must be immutable in the
 * request.
 *
 * Note:
 * - Additional release conditions, such as overlapping region, can be
 *   supported after they are confirmed as valid cases.
 * - When a busy memory resource gets split into two entries, the code
 *   assumes that all children remain in the lower address entry for
 *   simplicity.  Enhance this logic when necessary.
 */
int vmm_release_mem_region_adjustable(struct vmm_resource *,
				      resource_size_t, resource_size_t);
#endif

/*
 * Managed region resource
 */

struct vmm_device;

/**
 * Request and reserve an I/O or memory resource
 * @dev: device for which to request the resource
 * @root: root of the resource tree from which to request the resource
 * @new: descriptor of the resource to request
 *
 * This is a device-managed version of request_resource(). There is usually
 * no need to release resources requested by this function explicitly since
 * that will be taken care of when the device is unbound from its driver.
 * If for some reason the resource needs to be released explicitly, because
 * of ordering issues for example, drivers must call devm_release_resource()
 * rather than the regular release_resource().
 *
 * When a conflict is detected between any existing resources and the newly
 * requested resource, an error message will be printed.
 *
 * Returns 0 on success or a negative error code on failure.
 */
int vmm_devm_request_resource(struct vmm_device *dev,
				struct vmm_resource *root,
				struct vmm_resource *new);

/**
 * Release a previously requested resource
 * @dev: device for which to release the resource
 * @new: descriptor of the resource to release
 *
 * Releases a resource previously requested using devm_request_resource().
 */
void vmm_devm_release_resource(struct vmm_device *dev,
				struct vmm_resource *new);

#define vmm_devm_request_region(dev,start,n,name) \
	__vmm_devm_request_region(dev, &vmm_hostio_resource, (start), (n), (name))
#define vmm_devm_request_mem_region(dev,start,n,name) \
	__vmm_devm_request_region(dev, &vmm_hostmem_resource, (start), (n), (name))

struct vmm_resource * __vmm_devm_request_region(struct vmm_device *dev,
						struct vmm_resource *parent,
						resource_size_t start,
						resource_size_t n,
						const char *name);

#define vmm_devm_release_region(dev, start, n) \
	__vmm_devm_release_region(dev, &vmm_hostio_resource, (start), (n))
#define vmm_devm_release_mem_region(dev, start, n) \
	__vmm_devm_release_region(dev, &vmm_hostmem_resource, (start), (n))

void __vmm_devm_release_region(struct vmm_device *dev,
				struct vmm_resource *parent,
				resource_size_t start, resource_size_t n);

/**
 * Check if the requested addr and size spans more than any slot in the
 * hostmem resource tree.
 */
int vmm_hostmem_map_sanity_check(resource_size_t addr, unsigned long size);

/**
 * check if an address is reserved in the hostmem resource tree
 * returns 1 if reserved, 0 if not reserved.
 */
int vmm_hostmem_is_exclusive(u64 addr);

/* True if any part of r1 overlaps r2 */
static inline bool vmm_resource_overlaps(struct vmm_resource *r1,
					 struct vmm_resource *r2)
{
       return (r1->start <= r2->end && r1->end >= r2->start);
}

#endif
