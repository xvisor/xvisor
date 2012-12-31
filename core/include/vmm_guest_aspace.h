/**
 * Copyright (c) 2010 Anup Patel.
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
 * @file vmm_guest_aspace.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for guest address space
 */
#ifndef _VMM_GUEST_ASPACE_H__
#define _VMM_GUEST_ASPACE_H__

#include <vmm_manager.h>

/** Find region corresponding to a guest physical address and also
 *  resolve aliased regions to real or virtual regions if required.
 */
struct vmm_region *vmm_guest_find_region(struct vmm_guest *guest,
					 physical_addr_t gphys_addr,
					 bool resolve_alias);

/** Read from guest memory address space */
u32 vmm_guest_physical_read(struct vmm_guest *guest, 
			    physical_addr_t gphys_addr, 
			    void *dst, u32 len);

/** Write to guest memory address space */
u32 vmm_guest_physical_write(struct vmm_guest *guest, 
			     physical_addr_t gphys_addr, 
			     void *src, u32 len);

/** Map guest physical address to some host physical address */
int vmm_guest_physical_map(struct vmm_guest *guest,
			   physical_addr_t gphys_addr,
			   physical_size_t gphys_size,
			   physical_addr_t *hphys_addr,
			   physical_size_t *hphys_size,
			   u32 *reg_flags);

/** Unmap guest physical address */
int vmm_guest_physical_unmap(struct vmm_guest *guest,
			     physical_addr_t gphys_addr,
			     physical_size_t gphys_size);

/** Reset Guest Address space */
int vmm_guest_aspace_reset(struct vmm_guest *guest);

/** Initialize Guest Address space */
int vmm_guest_aspace_init(struct vmm_guest *guest);

/** DeInitialize Guest Address space */
int vmm_guest_aspace_deinit(struct vmm_guest *guest);

#endif
