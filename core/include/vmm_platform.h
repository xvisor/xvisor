/**
 * Copyright (c) 2016 Anup Patel.
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
 * @file vmm_platform.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Platform bus interface header
 */

#ifndef __VMM_PLATFORM_H_
#define __VMM_PLATFORM_H_

#include <vmm_types.h>
#include <vmm_devtree.h>
#include <vmm_devdrv.h>

/** Forward declaration of platform bus */
extern struct vmm_bus platform_bus;

/** Probe device instances under a given device tree node */
int vmm_platform_probe(struct vmm_devtree_node *node);

#endif /* __VMM_PLATFORM_H_ */
