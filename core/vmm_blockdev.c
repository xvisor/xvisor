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
 * @file vmm_blockdev.c
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief Block Device framework source
 */

#include <vmm_error.h>
#include <vmm_list.h>
#include <vmm_heap.h>
#include <vmm_string.h>
#include <vmm_blockdev.h>

int vmm_blockdev_doioctl(struct vmm_blockdev * bdev,
			 int cmd, void *buf, size_t buf_len)
{
	int ret;

	if (!bdev) {
		return VMM_EFAIL;
	}
	if (!(bdev->ioctl)) {
		return VMM_EFAIL;
	}

	ret = bdev->ioctl(bdev, cmd, buf, buf_len);

	return ret;
}

int vmm_blockdev_doreadblk(struct vmm_blockdev * bdev,
			   void *dest, u32 blknum, u32 blkcount)
{
	int ret;

	if (!bdev) {
		return 0;
	}
	if (!(bdev->readblk)) {
		return 0;
	}

	ret = bdev->readblk(bdev, dest, blknum, blkcount);

	return ret;
}

int vmm_blockdev_dowriteblk(struct vmm_blockdev * bdev,
			    void *src, u32 blknum, u32 blkcount)
{
	int ret;

	if (!bdev) {
		return 0;
	}
	if (!(bdev->writeblk)) {
		return 0;
	}

	ret = bdev->writeblk(bdev, src, blknum, blkcount);

	return ret;
}

int vmm_blockdev_register(struct vmm_blockdev * bdev)
{
	struct vmm_classdev *cd;

	if (bdev == NULL) {
		return VMM_EFAIL;
	}
	if (bdev->readblk == NULL || bdev->writeblk == NULL) {
		return VMM_EFAIL;
	}

	cd = vmm_malloc(sizeof(struct vmm_classdev));
	if (!cd) {
		return VMM_EFAIL;
	}

	INIT_LIST_HEAD(&cd->head);
	vmm_strcpy(cd->name, bdev->name);
	cd->dev = bdev->dev;
	cd->priv = bdev;

	vmm_devdrv_register_classdev(VMM_BLOCKDEV_CLASS_NAME, cd);

	return VMM_OK;
}

int vmm_blockdev_unregister(struct vmm_blockdev * bdev)
{
	int rc;
	struct vmm_classdev *cd;

	if (bdev == NULL) {
		return VMM_EFAIL;
	}
	if (bdev->dev == NULL) {
		return VMM_EFAIL;
	}

	cd = vmm_devdrv_find_classdev(VMM_BLOCKDEV_CLASS_NAME, bdev->name);
	if (!cd) {
		return VMM_EFAIL;
	}

	rc = vmm_devdrv_unregister_classdev(VMM_BLOCKDEV_CLASS_NAME, cd);
	if (!rc) {
		vmm_free(cd);
	}

	return rc;
}

struct vmm_blockdev *vmm_blockdev_find(const char *name)
{
	struct vmm_classdev *cd;

	cd = vmm_devdrv_find_classdev(VMM_BLOCKDEV_CLASS_NAME, name);

	if (!cd) {
		return NULL;
	}

	return cd->priv;
}

struct vmm_blockdev *vmm_blockdev_get(int num)
{
	struct vmm_classdev *cd;

	cd = vmm_devdrv_classdev(VMM_BLOCKDEV_CLASS_NAME, num);

	if (!cd) {
		return NULL;
	}

	return cd->priv;
}

u32 vmm_blockdev_count(void)
{
	return vmm_devdrv_classdev_count(VMM_BLOCKDEV_CLASS_NAME);
}

int __init vmm_blockdev_init(void)
{
	int rc;
	struct vmm_class *c;

	c = vmm_malloc(sizeof(struct vmm_class));
	if (!c) {
		return VMM_EFAIL;
	}

	INIT_LIST_HEAD(&c->head);
	vmm_strcpy(c->name, VMM_BLOCKDEV_CLASS_NAME);
	INIT_LIST_HEAD(&c->classdev_list);

	rc = vmm_devdrv_register_class(c);
	if (rc) {
		vmm_free(c);
		return rc;
	}

	return VMM_OK;
}
