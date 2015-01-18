/*
 * ioport.h	Definitions of routines for detecting, reserving and
 *		allocating system resources.
 *
 * Authors:	Linus Torvalds
 */

#ifndef _LINUX_IOPORT_H
#define _LINUX_IOPORT_H

#include <vmm_host_ram.h>
#include <vmm_host_aspace.h>
#include <vmm_resource.h>

#ifndef __ASSEMBLY__
#include <linux/compiler.h>
#include <linux/types.h>

#define resource vmm_resource

/*
 * IO resources have these defined flags.
 */
#define IORESOURCE_BITS		VMM_IORESOURCE_BITS

#define IORESOURCE_TYPE_BITS	VMM_IORESOURCE_TYPE_BITS
#define IORESOURCE_IO		VMM_IORESOURCE_IO
#define IORESOURCE_MEM		VMM_IORESOURCE_MEM
#define IORESOURCE_REG		VMM_IORESOURCE_REG
#define IORESOURCE_IRQ		VMM_IORESOURCE_IRQ
#define IORESOURCE_DMA		VMM_IORESOURCE_DMA
#define IORESOURCE_BUS		VMM_IORESOURCE_BUS

#define IORESOURCE_PREFETCH	VMM_IORESOURCE_PREFETCH
#define IORESOURCE_READONLY	VMM_IORESOURCE_READONLY
#define IORESOURCE_CACHEABLE	VMM_IORESOURCE_CACHEABLE
#define IORESOURCE_RANGELENGTH	VMM_IORESOURCE_RANGELENGTH
#define IORESOURCE_SHADOWABLE	VMM_IORESOURCE_SHADOWABLE

#define IORESOURCE_SIZEALIGN	VMM_IORESOURCE_SIZEALIGN
#define IORESOURCE_STARTALIGN	VMM_IORESOURCE_STARTALIGN

#define IORESOURCE_MEM_64	VMM_IORESOURCE_MEM_64
#define IORESOURCE_WINDOW	VMM_IORESOURCE_WINDOW
#define IORESOURCE_MUXED	VMM_IORESOURCE_MUXED

#define IORESOURCE_EXCLUSIVE	VMM_IORESOURCE_EXCLUSIVE
#define IORESOURCE_DISABLED	VMM_IORESOURCE_DISABLED
#define IORESOURCE_UNSET	VMM_IORESOURCE_UNSET
#define IORESOURCE_AUTO		VMM_IORESOURCE_AUTO
#define IORESOURCE_BUSY		VMM_IORESOURCE_BUSY

/* PnP IRQ specific bits (IORESOURCE_BITS) */
#define IORESOURCE_IRQ_HIGHEDGE		VMM_IORESOURCE_IRQ_HIGHEDGE
#define IORESOURCE_IRQ_LOWEDGE		VMM_IORESOURCE_IRQ_LOWEDGE
#define IORESOURCE_IRQ_HIGHLEVEL	VMM_IORESOURCE_IRQ_HIGHLEVEL
#define IORESOURCE_IRQ_LOWLEVEL		VMM_IORESOURCE_IRQ_LOWLEVEL
#define IORESOURCE_IRQ_SHAREABLE	VMM_IORESOURCE_IRQ_SHAREABLE
#define IORESOURCE_IRQ_OPTIONAL 	VMM_IORESOURCE_IRQ_OPTIONAL

/* PnP DMA specific bits (IORESOURCE_BITS) */
#define IORESOURCE_DMA_TYPE_MASK	VMM_IORESOURCE_DMA_TYPE_MASK
#define IORESOURCE_DMA_8BIT		VMM_IORESOURCE_DMA_8BIT
#define IORESOURCE_DMA_8AND16BIT	VMM_IORESOURCE_DMA_8AND16BIT
#define IORESOURCE_DMA_16BIT		VMM_IORESOURCE_DMA_16BIT

#define IORESOURCE_DMA_MASTER		VMM_IORESOURCE_DMA_MASTER
#define IORESOURCE_DMA_BYTE		VMM_IORESOURCE_DMA_BYTE
#define IORESOURCE_DMA_WORD		VMM_IORESOURCE_DMA_WORD

#define IORESOURCE_DMA_SPEED_MASK	VMM_IORESOURCE_DMA_SPEED_MASK
#define IORESOURCE_DMA_COMPATIBLE	VMM_IORESOURCE_DMA_COMPATIBLE
#define IORESOURCE_DMA_TYPEA		VMM_IORESOURCE_DMA_TYPEA
#define IORESOURCE_DMA_TYPEB		VMM_IORESOURCE_DMA_TYPEB
#define IORESOURCE_DMA_TYPEF		VMM_IORESOURCE_DMA_TYPEF

/* PnP memory I/O specific bits (IORESOURCE_BITS) */
#define IORESOURCE_MEM_WRITEABLE	VMM_IORESOURCE_MEM_WRITEABLE
#define IORESOURCE_MEM_CACHEABLE	VMM_IORESOURCE_MEM_CACHEABLE
#define IORESOURCE_MEM_RANGELENGTH	VMM_IORESOURCE_MEM_RANGELENGTH
#define IORESOURCE_MEM_TYPE_MASK	VMM_IORESOURCE_MEM_TYPE_MASK
#define IORESOURCE_MEM_8BIT		VMM_IORESOURCE_MEM_8BIT
#define IORESOURCE_MEM_16BIT		VMM_IORESOURCE_MEM_16BIT
#define IORESOURCE_MEM_8AND16BIT	VMM_IORESOURCE_MEM_8AND16BIT
#define IORESOURCE_MEM_32BIT		VMM_IORESOURCE_MEM_32BIT
#define IORESOURCE_MEM_SHADOWABLE	VMM_IORESOURCE_MEM_SHADOWABLE
#define IORESOURCE_MEM_EXPANSIONROM	VMM_IORESOURCE_MEM_EXPANSIONROM

/* PnP I/O specific bits (IORESOURCE_BITS) */
#define IORESOURCE_IO_16BIT_ADDR	VMM_IORESOURCE_IO_16BIT_ADDR
#define IORESOURCE_IO_FIXED		VMM_IORESOURCE_IO_FIXED

/* PCI ROM control bits (IORESOURCE_BITS) */
#define IORESOURCE_ROM_ENABLE		VMM_IORESOURCE_ROM_ENABLE
#define IORESOURCE_ROM_SHADOW		VMM_IORESOURCE_ROM_SHADOW
#define IORESOURCE_ROM_COPY		VMM_IORESOURCE_ROM_COPY
#define IORESOURCE_ROM_BIOS_COPY	VMM_IORESOURCE_ROM_BIOS_COPY

/* PCI control bits.  Shares IORESOURCE_BITS with above PCI ROM.  */
#define IORESOURCE_PCI_FIXED		VMM_IORESOURCE_PCI_FIXED

#define ioport_resource			vmm_hostio_resource
#define iomem_resource			vmm_hostmem_resource

#define request_resource_conflict(root, new)	\
	vmm_request_resource_conflict(root, new)
#define request_resource(root, new)		\
	vmm_request_resource(root, new)
#define release_resource(new)			\
	vmm_release_resource(new)
#define release_child_resources(new)		\
	vmm_release_child_resources(new)
#define reserve_region_with_split(root, start, end, name)	\
	vmm_reserve_region_with_split(root, start, end, name)
#define insert_resource_conflict(parent, new)	\
	vmm_insert_resource_conflict(parent, new)
#define insert_resource(parent, new)		\
	vmm_insert_resource(parent, new)
#define insert_resource_expand_to_fit(root, new)	\
	vmm_insert_resource_expand_to_fit(root, new)
#define allocate_resource(root, new, size, min, max, align, alignf, alignf_data) \
	vmm_allocate_resource(root, new, size, min, max, align, alignf, alignf_data)
#define lookup_resource(root, start)		\
	vmm_lookup_resource(root, start)
#define adjust_resource(res, start, size)	\
	vmm_adjust_resource(res, start, size)
#define resource_alignment(res)			\
	vmm_resource_alignment(res)
#define resource_size(res)			\
	vmm_resource_size(res)
#define resource_type(res)			\
	vmm_resource_type(res)
#define resource_contains(r1, r2)		\
	vmm_resource_contains(r1, r2)

/* Convenience shorthand with allocation */
#define request_region(start,n,name)		\
	vmm_request_region(start,n,name)
#define request_muxed_region(start,n,name)	\
	vmm_request_muxed_region(start,n,name)
#define __request_mem_region(start,n,name,excl)	\
	__vmm_request_mem_region(start,n,name,excl)
#define request_mem_region(start,n,name)	\
	vmm_request_mem_region(start,n,name)
#define request_mem_region_exclusive(start,n,name)	\
	vmm_request_mem_region_exclusive(start,n,name)
#define rename_region(region, newname)		\
	vmm_rename_region(region, newname)

#define __request_region(parent,start,n,name,flags)	\
	__vmm_request_region(parent,start,n,name,flags)

/* Compatibility cruft */
#define release_region(start,n)			\
	vmm_release_region(start,n)
#define check_mem_region(start,n)		\
	vmm_check_mem_region(start,n)
#define release_mem_region(start,n)		\
	vmm_release_mem_region(start,n)

#define __check_region(parent, start, size)	\
	__vmm_check_region(parent, start, size)
#define __release_region(parent, start, size)	\
	__vmm_release_region(parent, start, size)

#ifdef CONFIG_MEMORY_HOTREMOVE
#define release_mem_region_adjustable(parent, start, size) \
	vmm_release_mem_region_adjustable(parent, start, size)
#endif

#define devm_request_resource(dev, root, new)	\
	vmm_devm_request_resource(dev, root, new)
#define devm_release_resource(dev, new)		\
	vmm_devm_release_resource(dev, new)

#define devm_request_region(dev,start,n,name)	\
	vmm_devm_request_region(dev,start,n,name)
#define devm_request_mem_region(dev,start,n,name)	\
	vmm_devm_request_mem_region(dev,start,n,name)

#define __devm_request_region(dev, parent, start, n, name)	\
	__vmm_devm_request_region(dev, parent, start, n, name)

#define devm_release_region(dev, start, n)	\
	vmm_devm_release_region(dev, start, n)
#define devm_release_mem_region(dev, start, n)	\
	vmm_devm_release_mem_region(dev, start, n)

#define __devm_release_region(dev, parent, start, n)	\
	__vmm_devm_release_region(dev, parent, start, n)
#define iomem_map_sanity_check(addr, size)	\
	vmm_hostmem_map_sanity_check(addr, size)
#define iomem_is_exclusive(addr)	\
	vmm_hostmem_is_exclusive(addr)

#define walk_system_ram_range(start_pfn, nr_pages, arg, func)	\
	vmm_walk_system_ram_range(start_pfn, nr_pages, arg, func)
#define walk_system_ram_res(start, end, arg, func)	\
	vmm_walk_system_ram_res(start, end, arg, func)
#define walk_iomem_res(name, flags, start, end, arg, func)	\
	vmm_walk_hostmem_res(name, flags, start, end, arg, func)

#define resource_overlaps(r1, r2)	\
	vmm_resource_overlaps(r1, r2)

#endif /* __ASSEMBLY__ */

#endif
