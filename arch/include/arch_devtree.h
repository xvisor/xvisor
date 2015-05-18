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

/** Setup/Configure/Parse RAM banks
 *  Note: This function will be called before populating device tree
 */
int arch_devtree_ram_bank_setup(void);

/** Get RAM bank count
 *  Note: This function will be called before populating device tree
 */
int arch_devtree_ram_bank_count(u32 *bank_count);

/** Get start physical address of RAM bank
 *  Note: This function will be called before populating device tree
 */
int arch_devtree_ram_bank_start(u32 bank, physical_addr_t *addr);

/** Get physical size of RAM bank
 *  Note: This function will be called before populating device tree
 */
int arch_devtree_ram_bank_size(u32 bank, physical_size_t *size);

/** Count reserved RAM areas
 *  Note: This function will be called before populating device tree
 */
int arch_devtree_reserve_count(u32 *count);

/** Get reserved RAM area physical address
 *  Note: This function will be called before populating device tree
 */
int arch_devtree_reserve_addr(u32 index, physical_addr_t *addr);

/** Get reserved RAM area physical size
 *  Note: This function will be called before populating device tree
 */
int arch_devtree_reserve_size(u32 index, physical_size_t *size);

/** Populate device tree */
int arch_devtree_populate(struct vmm_devtree_node **root);

#endif
