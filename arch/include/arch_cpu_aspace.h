/**
 * Copyright (c) 2012 Anup Patel.
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
 * @file arch_cpu.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief generic interface for arch specific CPU address space functions
 */
#ifndef _ARCH_CPU_ASPACE_H__
#define _ARCH_CPU_ASPACE_H__

#include <vmm_types.h>

/** Initialize address space on primary cpu */
int arch_cpu_aspace_primary_init(physical_addr_t *core_resv_pa, 
				 virtual_addr_t *core_resv_va,
				 virtual_size_t *core_resv_sz,
				 physical_addr_t *arch_resv_pa,
				 virtual_addr_t *arch_resv_va,
				 virtual_size_t *arch_resv_sz);

/** Initialize address space on secondary cpu */
int arch_cpu_aspace_secondary_init(void);

/** Map given page virtual address to page physical address */
int arch_cpu_aspace_map(virtual_addr_t page_va, 
			physical_addr_t page_pa,
			u32 mem_flags);

/** Unmap given page based on its virtual address */
int arch_cpu_aspace_unmap(virtual_addr_t page_va);

/** Find out physical address mapped by given virtual address */
int arch_cpu_aspace_va2pa(virtual_addr_t va, 
			  physical_addr_t *pa);

/** Read data from memory with given physical adress 
 *  NOTE: This arch function is optional.
 *  NOTE: The tmp_va is per host CPU temporary virtual address which
 *  can be optionally used to access the physical memory.
 *  NOTE: The len field will be less than or equal to VMM_PAGE_SIZE.
 *  is to ensure that no VCPU over-haul the CPU.
 *  NOTE: If arch implments this function then arch_config.h
 *  will define ARCH_HAS_MEMORY_READWRITE feature.
 */
int arch_cpu_aspace_memory_read(virtual_addr_t tmp_va, 
				physical_addr_t src, 
				void *dst, u32 len, bool cacheable);

/** Write data to memory with given physical adress
 *  NOTE: This arch function is optional.
 *  NOTE: The tmp_va is per host CPU temporary virtual address which
 *  can be optionally used to access the physical memory.
 *  NOTE: The len field will be less than or equal to VMM_PAGE_SIZE.
 *  This is to ensure that no VCPU over-haul the CPU.
 *  NOTE: If arch implments this function then arch_config.h
 *  will define ARCH_HAS_MEMORY_READWRITE feature.
 */
int arch_cpu_aspace_memory_write(virtual_addr_t tmp_va, 
				 physical_addr_t dst, 
				 void *src, u32 len, bool cacheable);

/** Write data to memory with given physical adress
 *  NOTE: This arch function is optional.
 *  NOTE: The tmp_va is per host CPU temporary virtual address which
 *  can be optionally used to access the physical memory.
 *  NOTE: If arch implments this function then arch_config.h
 *  will define ARCH_HAS_MEMORY_READWRITE feature.
 */
int arch_cpu_aspace_memory_rwinit(virtual_addr_t tmp_va);

#endif
