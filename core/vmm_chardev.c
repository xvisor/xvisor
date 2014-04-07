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
#include <vmm_compiler.h>
#include <vmm_heap.h>
#include <vmm_scheduler.h>
#include <vmm_chardev.h>
#include <libs/stringlib.h>

int vmm_chardev_doioctl(struct vmm_chardev *cdev,
			int cmd, void *buf, u32 len)
{
	if (cdev && cdev->ioctl) {
		return cdev->ioctl(cdev, cmd, buf, len);
	} else {
		return VMM_EFAIL;
	}
}

u32 vmm_chardev_doread(struct vmm_chardev *cdev,
		       u8 *dest, u32 len, bool block)
{
	u32 b;
	bool sleep;

	if (cdev && cdev->read) {
		if (block) {
			b = 0;
			sleep = vmm_scheduler_orphan_context() ? TRUE : FALSE;
			while (b < len) {
				b += cdev->read(cdev, &dest[b], 
						len - b, sleep);
			}
			return b;
		} else {
			return cdev->read(cdev, dest, len, FALSE);
		}
	} else {
		return 0;
	}
}

u32 vmm_chardev_dowrite(struct vmm_chardev *cdev,
			u8 *src, u32 len, bool block)
{
	u32 b;
	bool sleep;

	if (cdev && cdev->write) {
		if (block) {
			b = 0;
			sleep = vmm_scheduler_orphan_context() ? TRUE : FALSE;
			while (b < len) {
				b += cdev->write(cdev, &src[b], 
						 len - b, sleep);
			}
			return b;
		} else {
			return cdev->write(cdev, src, len, FALSE);
		}
	} else {
		return 0;
	}
}

static struct vmm_class chardev_class = {
	.name = VMM_CHARDEV_CLASS_NAME,
};

int vmm_chardev_register(struct vmm_chardev *cdev)
{
	if (!(cdev && cdev->read && cdev->write)) {
		return VMM_EFAIL;
	}

	vmm_devdrv_initialize_device(&cdev->dev);
	if (strlcpy(cdev->dev.name, cdev->name, sizeof(cdev->dev.name)) >=
	    sizeof(cdev->dev.name)) {
		return VMM_EOVERFLOW;
	}
	cdev->dev.class = &chardev_class;
	vmm_devdrv_set_data(&cdev->dev, cdev);
	
	return vmm_devdrv_register_device(&cdev->dev);
}

int vmm_chardev_unregister(struct vmm_chardev *cdev)
{
	if (!cdev) {
		return VMM_EFAIL;
	}

	return vmm_devdrv_unregister_device(&cdev->dev);
}

struct vmm_chardev *vmm_chardev_find(const char *name)
{
	struct vmm_device *dev;

	dev = vmm_devdrv_class_find_device(&chardev_class, name);
	if (!dev) {
		return NULL;
	}

	return vmm_devdrv_get_data(dev);
}

struct vmm_chardev *vmm_chardev_get(int num)
{
	struct vmm_device *dev;

	dev = vmm_devdrv_class_device(&chardev_class, num);
	if (!dev) {
		return NULL;
	}

	return vmm_devdrv_get_data(dev);
}

u32 vmm_chardev_count(void)
{
	return vmm_devdrv_class_device_count(&chardev_class);
}

int __init vmm_chardev_init(void)
{
	return vmm_devdrv_register_class(&chardev_class);
}
