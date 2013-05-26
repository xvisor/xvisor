/**
 * Copyright (c) 2013 Anup Patel.
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
 * @file arch_devtree.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief generic interface for arch specific device tree functions
 */
#ifndef _ARCH_DEVTREE_H__
#define _ARCH_DEVTREE_H__

#include <vmm_devtree.h>

/** Get RAM start physical address 
 *  Note: This function will be called before populating device tree
 */
int arch_devtree_ram_start(physical_addr_t *addr);

/** Get RAM physical size 
 *  Note: This function will be called before populating device tree
 */
int arch_devtree_ram_size(physical_size_t *size);

/** Populate device tree */
int arch_devtree_populate(struct vmm_devtree_node **root);

#endif
