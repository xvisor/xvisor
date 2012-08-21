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
 * @file vmm_chardev.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Character Device framework source
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_scheduler.h>
#include <vmm_chardev.h>
#include <stringlib.h>

int vmm_chardev_doioctl(struct vmm_chardev * cdev,
			int cmd, void *buf, u32 len)
{
	if (cdev && cdev->ioctl) {
		return cdev->ioctl(cdev, cmd, buf, len);
	} else {
		return VMM_EFAIL;
	}
}

u32 vmm_chardev_doread(struct vmm_chardev * cdev,
		       u8 *dest, u32 offset, u32 len, bool block)
{
	u32 b;
	bool sleep;

	if (cdev && cdev->read) {
		if (block) {
			b = 0;
			sleep = vmm_scheduler_orphan_context() ? TRUE : FALSE;
			while (b < len) {
				b += cdev->read(cdev, &dest[b], 
					        offset + b, len - b, sleep);
			}
			return b;
		} else {
			return cdev->read(cdev, dest, offset, len, FALSE);
		}
	} else {
		return 0;
	}
}

u32 vmm_chardev_dowrite(struct vmm_chardev * cdev,
			u8 *src, u32 offset, u32 len, bool block)
{
	u32 b;
	bool sleep;

	if (cdev && cdev->write) {
		if (block) {
			b = 0;
			sleep = vmm_scheduler_orphan_context() ? TRUE : FALSE;
			while (b < len) {
				b += cdev->write(cdev, &src[b], 
					         offset + b, len - b, sleep);
			}
			return b;
		} else {
			return cdev->write(cdev, src, offset, len, FALSE);
		}
	} else {
		return 0;
	}
}

int vmm_chardev_register(struct vmm_chardev * cdev)
{
	int rc;
	struct vmm_classdev *cd;

	if (!(cdev && cdev->read && cdev->write)) {
		return VMM_EFAIL;
	}

	cd = vmm_malloc(sizeof(struct vmm_classdev));
	if (!cd) {
		return VMM_EFAIL;
	}

	memset(cd, 0, sizeof(struct vmm_classdev));

	INIT_LIST_HEAD(&cd->head);
	strcpy(cd->name, cdev->name);
	cd->dev = cdev->dev;
	cd->priv = cdev;

	rc = vmm_devdrv_register_classdev(VMM_CHARDEV_CLASS_NAME, cd);
	if (rc != VMM_OK) {
		vmm_free(cd);
	}

	return rc;
}

int vmm_chardev_unregister(struct vmm_chardev * cdev)
{
	int rc;
	struct vmm_classdev *cd;

	if (!cdev) {
		return VMM_EFAIL;
	}

	cd = vmm_devdrv_find_classdev(VMM_CHARDEV_CLASS_NAME, cdev->name);
	if (!cd) {
		return VMM_EFAIL;
	}

	rc = vmm_devdrv_unregister_classdev(VMM_CHARDEV_CLASS_NAME, cd);
	if (rc == VMM_OK) {
		vmm_free(cd);
	}

	return rc;
}

struct vmm_chardev *vmm_chardev_find(const char *name)
{
	struct vmm_classdev *cd;

	cd = vmm_devdrv_find_classdev(VMM_CHARDEV_CLASS_NAME, name);
	if (!cd) {
		return NULL;
	}

	return cd->priv;
}

struct vmm_chardev *vmm_chardev_get(int num)
{
	struct vmm_classdev *cd;

	cd = vmm_devdrv_classdev(VMM_CHARDEV_CLASS_NAME, num);
	if (!cd) {
		return NULL;
	}

	return cd->priv;
}

u32 vmm_chardev_count(void)
{
	return vmm_devdrv_classdev_count(VMM_CHARDEV_CLASS_NAME);
}

int __init vmm_chardev_init(void)
{
	int rc;
	struct vmm_class *c;

	c = vmm_malloc(sizeof(struct vmm_class));
	if (!c) {
		return VMM_EFAIL;
	}

	memset(c, 0, sizeof(struct vmm_class));

	INIT_LIST_HEAD(&c->head);
	strcpy(c->name, VMM_CHARDEV_CLASS_NAME);
	INIT_LIST_HEAD(&c->classdev_list);

	rc = vmm_devdrv_register_class(c);
	if (rc != VMM_OK) {
		vmm_free(c);
	}

	return rc;
}
