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
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for guest address space
 */
#ifndef _VMM_GUEST_ASPACE_H__
#define _VMM_GUEST_ASPACE_H__

#include <vmm_list.h>
#include <vmm_manager.h>

/** Get region to which given guest physical address belongs */
vmm_region_t *vmm_guest_aspace_getregion(vmm_guest_t *guest,
					 physical_addr_t gphys_addr);

/** Reset Guest Address space */
int vmm_guest_aspace_reset(vmm_guest_t *guest);

/** Probe Guest Address space */
int vmm_guest_aspace_probe(vmm_guest_t *guest);

/** Initialize Guest Address space */
int vmm_guest_aspace_init(vmm_guest_t *guest);

#endif
