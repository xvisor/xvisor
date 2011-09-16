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
 * @file vmm_host_aspace.h
 * @version 0.01
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief Header file for virtual address space management.
 */

#ifndef __VMM_HOST_ASPACE_H_
#define __VMM_HOST_ASPACE_H_

#include <vmm_types.h>

#define VMM_PAGE_SHIFT		12
#define VMM_PAGE_SIZE		(0x01UL << VMM_PAGE_SHIFT)
#define VMM_PAGE_MASK		(VMM_PAGE_SIZE - 1)
#define VMM_PG_FN(x)		((x) & ~VMM_PAGE_MASK)
#define VMM_ROUNDUP2_PGSZ(x)	(((x) & VMM_PAGE_MASK) ? VMM_PG_FN(x) + VMM_PAGE_SIZE : (x))

enum vmm_host_memory_flags {
	VMM_MEMORY_CACHEABLE=0x00000001,
	VMM_MEMORY_READABLE=0x00000002,
	VMM_MEMORY_WRITEABLE=0x00000004,
	VMM_MEMORY_EXECUTABLE=0x00000008
};

/** Allocate virtual space from virtual address pool */
int vmm_host_vapool_alloc(virtual_addr_t * va, 
			  virtual_size_t sz, 
			  bool aligned);

/** Free virtual space to virtual address pool */
int vmm_host_vapool_free(virtual_addr_t va, virtual_size_t sz);

/** Allocate physical space from RAM */
int vmm_host_ram_alloc(physical_addr_t * pa, 
		       physical_size_t sz, 
		       bool aligned);

/** Free physical space to RAM */
int vmm_host_ram_free(physical_addr_t va, physical_size_t sz);

/** Map physical memory to a virtual memory */
virtual_addr_t vmm_host_memmap(physical_addr_t pa, 
			       virtual_size_t sz, 
			       u32 mem_flags);

/** Unmap virtual memory */
int vmm_host_memunmap(virtual_addr_t va, 
		      virtual_size_t sz);

/** Map IO physical memory to a virtual memory */
static inline virtual_addr_t vmm_host_iomap(physical_addr_t pa, 
					    virtual_size_t sz)
{
	return vmm_host_memmap(pa, sz, 
			       (VMM_MEMORY_READABLE | 
				VMM_MEMORY_WRITEABLE));
}

/** Unmap IO virtual memory */
static inline int vmm_host_iounmap(virtual_addr_t va, 
				   virtual_size_t sz)
{
	return vmm_host_memunmap(va, sz);
}

/** Allocate pages from host memory */
virtual_addr_t vmm_host_alloc_pages(u32 page_count, u32 mem_flags);

/** Free pages back to host memory */
int vmm_host_free_pages(virtual_addr_t page_va, u32 page_count);

/** Read from host physical memory */
u32 vmm_host_physical_read(physical_addr_t hphys_addr, 
			   void * dst, u32 len);

/** Write to host physical memory */
u32 vmm_host_physical_write(physical_addr_t hphys_addr, 
			    void * src, u32 len);

int vmm_host_aspace_init(void);

#endif /* __VMM_HOST_ASPACE_H_ */
