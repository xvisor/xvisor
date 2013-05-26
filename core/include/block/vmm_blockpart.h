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
 * @file vmm_blockpart.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Block device partition managment header
 */

#ifndef __VMM_BLOCKPART_H_
#define __VMM_BLOCKPART_H_

#include <block/vmm_blockdev.h>

#define VMM_BLOCKPART_IPRIORITY		(VMM_BLOCKDEV_CLASS_IPRIORITY + 1)

#define VMM_BLOCKPART_MANAGER_MAX_NAME	64

struct vmm_blockpart_manager {
	struct dlist head;
	u32 sign;
	char name[VMM_BLOCKPART_MANAGER_MAX_NAME];
	int (*parse_part)(struct vmm_blockdev *bdev);
	void (*cleanup_part)(struct vmm_blockdev *bdev);
};

/** Get block device private context of partiton manager */
static inline void *vmm_blockpart_manager_get_priv(struct vmm_blockdev *bdev)
{
	return (bdev) ? bdev->part_manager_priv : NULL;
}

/** Set block device private context of partiton manager */
static inline void vmm_blockpart_manager_set_priv(struct vmm_blockdev *bdev, 
						  void *priv)
{
	if (bdev) {
		bdev->part_manager_priv = priv;
	}
}

/** Register partition manager */
int vmm_blockpart_manager_register(struct vmm_blockpart_manager *mngr);

/** Unregister partition manager */
int vmm_blockpart_manager_unregister(struct vmm_blockpart_manager *mngr);

/** Get partition manager with given number */
struct vmm_blockpart_manager *vmm_blockpart_manager_get(int num);

/** Count number of partition managers */
u32 vmm_blockpart_manager_count(void);

#endif /* __VMM_BLOCKPART_H_ */
