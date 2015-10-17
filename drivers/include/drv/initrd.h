/**
 * Copyright (c) 2015 Anup Patel.
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
 * @file initrd.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Interface for INITRD device driver.
 */

#ifndef __INITRD_H_
#define __INITRD_H_

#include <vmm_error.h>
#include <vmm_types.h>
#include <drv/rbd.h>

#define INITRD_IPRIORITY		(RBD_IPRIORITY + 1)

#define INITRD_START_ATTR_NAME		"linux,initrd-start"
#define INITRD_END_ATTR_NAME		"linux,initrd-end"

#define INITRD_START_ATTR2_NAME		"initrd-start"
#define INITRD_END_ATTR2_NAME		"initrd-end"

#if IS_ENABLED(CONFIG_BLOCK_INITRD)

/** Destroy RBD instance for INITRD */
void initrd_rbd_destroy(void);

/** Get RBD instance for INITRD */
struct rbd *initrd_rbd_get(void);

/** Update the location of INITRD in device tree
 *  NOTE: This API will only work before INITRD driver
 *  is initialized.
 */
int initrd_devtree_update(u64 start, u64 end);

#else

static inline void initrd_rbd_destroy(void) {}

static inline struct rbd *initrd_rbd_get(void)
{
	return NULL;
}

static inline int initrd_devtree_update(u64 start, u64 end)
{
	return VMM_ENOSYS;
}

#endif

#endif /* __INITRD_H_ */
