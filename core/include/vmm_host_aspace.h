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

#define PAGE_SHIFT		12
#define PAGE_SIZE		(0x01UL << PAGE_SHIFT)
#define PAGE_MASK		~(PAGE_SIZE - 1)
#define PG_FN(x)		(x & PAGE_MASK)
#define ROUNDUP2_PGSZ(x)	((x & ~PAGE_MASK) ? PG_FN(x) + PAGE_SIZE : x)

struct vmm_host_aspace_ctrl {
	u32 *pool_bmap;
	u32 pool_bmap_len;
	virtual_addr_t pool_va;
	virtual_size_t pool_sz;
};

typedef struct vmm_host_aspace_ctrl vmm_host_aspace_ctrl_t;

int vmm_host_aspace_init(void);
virtual_addr_t vmm_host_iomap(physical_addr_t pa, virtual_size_t sz);
int vmm_host_iounmap(virtual_addr_t va, virtual_size_t sz);

#endif /* __VMM_HOST_ASPACE_H_ */
