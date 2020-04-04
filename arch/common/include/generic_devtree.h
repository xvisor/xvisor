/**
 * Copyright (c) 2020 Anup Patel.
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
 * @file generic_devtree.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Common SMP operations interface
 */
#ifndef __ARCH_GENERIC_DEVTREE_H__
#define __ARCH_GENERIC_DEVTREE_H__

#include <vmm_types.h>

/** Virtual address of FDT or DTB */
extern virtual_addr_t devtree_virt;

/** Virtual address of first FDT or DTB page */
extern virtual_addr_t devtree_virt_base;

/** Physical address of first FDT or DTB page */
extern physical_addr_t devtree_phys_base;

/** Virtual size of all FDT or DTB pages */
extern virtual_size_t devtree_virt_size;

#endif
