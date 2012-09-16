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

/** Address space functions required by VMM core */
int arch_cpu_aspace_init(physical_addr_t * core_resv_pa, 
			 virtual_addr_t * core_resv_va,
			 virtual_size_t * core_resv_sz,
			 physical_addr_t * arch_resv_pa,
			 virtual_addr_t * arch_resv_va,
			 virtual_size_t * arch_resv_sz);
int arch_cpu_aspace_map(virtual_addr_t va, 
			virtual_size_t sz, 
			physical_addr_t pa,
			u32 mem_flags);
int arch_cpu_aspace_unmap(virtual_addr_t va, 
			 virtual_size_t sz);
int arch_cpu_aspace_va2pa(virtual_addr_t va, 
			 physical_addr_t * pa);

#endif
