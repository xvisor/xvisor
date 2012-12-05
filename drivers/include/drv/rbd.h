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
 * @file rbd.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Interface for RAM backed block device driver.
 */

#ifndef __RBD_H_
#define __RBD_H_

#include <vmm_types.h>
#include <block/vmm_blockdev.h>

#define RBD_BLOCK_SIZE				512

/* RAM backed device (RBD) context */
struct rbd {
	struct vmm_blockdev *bdev;
	physical_addr_t addr;
	physical_size_t size;
};

/** Create RBD instance */
struct rbd *rbd_create(const char *name, 
			physical_addr_t pa, 
			physical_size_t sz);

/** Destroy RBD instance */
void rbd_destroy(struct rbd *d);

#endif /* __RBD_H_ */
