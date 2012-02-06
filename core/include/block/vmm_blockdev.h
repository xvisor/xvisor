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
 * @file vmm_blockdev.h
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief Block Device framework header
 */

#ifndef __VMM_BLOCKDEV_H_
#define __VMM_BLOCKDEV_H_

#include <vmm_types.h>
#include <vmm_devdrv.h>

#define VMM_BLOCKDEV_CLASS_NAME				"block"
#define VMM_BLOCKDEV_CLASS_IPRIORITY			1

struct vmm_blockdev;

typedef int (*vmm_blockdev_ioctl_t) (struct vmm_blockdev * bdev,
				     int cmd, void *buf, size_t buf_len);
typedef int (*vmm_blockdev_readblk_t) (struct vmm_blockdev * bdev,
				       void *dest, u32 blknum, u32 blkcount);
typedef int (*vmm_blockdev_writeblk_t) (struct vmm_blockdev * bdev,
					void *src, u32 blknum, u32 blkcount);

struct vmm_blockdev {
	char name[32];
	struct vmm_device *dev;
	vmm_blockdev_ioctl_t ioctl;
	vmm_blockdev_readblk_t readblk;
	vmm_blockdev_writeblk_t writeblk;
};

/** Do ioctl operation on a block device */
int vmm_blockdev_doioctl(struct vmm_blockdev * bdev,
			 int cmd, void *buf, size_t buf_len);

/** Do read blocks operation on a block device */
int vmm_blockdev_doreadblk(struct vmm_blockdev * bdev,
			   void *dest, u32 blknum, u32 blkcount);

/** Do write blocks operation on a block device */
int vmm_blockdev_dowriteblk(struct vmm_blockdev * bdev,
			    void *src, u32 blknum, u32 blkcount);

/** Register block device to device driver framework */
int vmm_blockdev_register(struct vmm_blockdev * bdev);

/** Unregister block device from device driver framework */
int vmm_blockdev_unregister(struct vmm_blockdev * bdev);

/** Find a block device in device driver framework */
struct vmm_blockdev *vmm_blockdev_find(const char *name);

/** Get block device with given number */
struct vmm_blockdev *vmm_blockdev_get(int num);

/** Count number of block devices */
u32 vmm_blockdev_count(void);

#endif /* __VMM_BLOCKDEV_H_ */
