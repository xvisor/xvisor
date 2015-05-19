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
 * @file vmm_host_ram.h
 * @author Anup patel (anup@brainfault.org)
 * @brief Header file for RAM management.
 */

#ifndef __VMM_HOST_RAM_H_
#define __VMM_HOST_RAM_H_

#include <vmm_types.h>

/** Allocate physical space from RAM */
physical_size_t vmm_host_ram_alloc(physical_addr_t *pa,
				   physical_size_t sz,
				   u32 align_order);

/** Reserve a portion of RAM forcefully */
int vmm_host_ram_reserve(physical_addr_t pa, physical_size_t sz);

/** Free physical space to RAM */
int vmm_host_ram_free(physical_addr_t pa, physical_size_t sz);

/** Base address of RAM */
physical_addr_t vmm_host_ram_base(void);

/** Check if a RAM physical address is free */
bool vmm_host_ram_frame_isfree(physical_addr_t pa);

/** Free frame count of RAM */
u32 vmm_host_ram_free_frame_count(void);

/** Total frame count of RAM */
u32 vmm_host_ram_total_frame_count(void);

/** Total size of RAM */
physical_size_t vmm_host_ram_size(void);

/** Estimate House-keeping size of RAM */
virtual_size_t vmm_host_ram_estimate_hksize(void);

/* Initialize RAM managment */
int vmm_host_ram_init(virtual_addr_t hkbase);

#endif /* __VMM_HOST_RAM_H_ */
