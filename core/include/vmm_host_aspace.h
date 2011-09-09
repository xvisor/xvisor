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
#define VMM_PAGE_MASK		~(VMM_PAGE_SIZE - 1)
#define VMM_PG_FN(x)		(x & VMM_PAGE_MASK)
#define VMM_ROUNDUP2_PGSZ(x)	((x & ~VMM_PAGE_MASK) ? VMM_PG_FN(x) + VMM_PAGE_SIZE : x)

enum vmm_host_memory_flags {
	VMM_MEMORY_CACHEABLE=0x00000001,
	VMM_MEMORY_READABLE=0x00000002,
	VMM_MEMORY_WRITEABLE=0x00000004,
	VMM_MEMORY_EXECUTABLE=0x00000008
};

struct vmm_host_aspace_ctrl {
	u32 *pool_bmap;
	u32 pool_bmap_len;
	virtual_addr_t pool_va;
	virtual_size_t pool_sz;
};

typedef struct vmm_host_aspace_ctrl vmm_host_aspace_ctrl_t;

/** Map IO physical address to a virtual address */
virtual_addr_t vmm_host_iomap(physical_addr_t pa, virtual_size_t sz);

/** Unmap IO virtual address */
int vmm_host_iounmap(virtual_addr_t va, virtual_size_t sz);

/** Map Memory physical address to a virtual address */
virtual_addr_t vmm_host_memmap(physical_addr_t pa, virtual_size_t sz);

/** Unmap Memory virtual address */
int vmm_host_memunmap(virtual_addr_t va, virtual_size_t sz);

/** Read from host memory address space */
u32 vmm_host_physical_read(physical_addr_t hphys_addr, 
			   void * dst, u32 len);

/** Write to host memory address space */
u32 vmm_host_physical_write(physical_addr_t hphys_addr, 
			    void * src, u32 len);

int vmm_host_aspace_init(void);

#endif /* __VMM_HOST_ASPACE_H_ */
