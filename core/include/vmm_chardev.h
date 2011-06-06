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
 * @file vmm_chardev.h
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief Character Device framework header
 */

#ifndef __VMM_CHARDEV_H_
#define __VMM_CHARDEV_H_

#include <vmm_types.h>
#include <vmm_spinlocks.h>
#include <vmm_devdrv.h>

#define VMM_CHARDEV_CLASS_NAME				"char"

typedef struct vmm_chardev vmm_chardev_t;
typedef int (*vmm_chardev_ioctl_t) (vmm_chardev_t * cdev,
				    int cmd, void *buf, size_t buf_len);
typedef int (*vmm_chardev_read_t) (vmm_chardev_t * cdev,
				   char *dest, size_t offset, size_t len);
typedef int (*vmm_chardev_write_t) (vmm_chardev_t * cdev,
				    char *src, size_t offset, size_t len);

struct vmm_chardev {
	char name[32];
	vmm_device_t *dev;
	vmm_chardev_ioctl_t ioctl;
	vmm_chardev_read_t read;
	vmm_chardev_write_t write;
	void *priv;
};

/** Do ioctl operation on a character device */
int vmm_chardev_doioctl(vmm_chardev_t * cdev,
			int cmd, void *buf, size_t buf_len);

/** Do read operation on a character device */
int vmm_chardev_doread(vmm_chardev_t * cdev,
		       char *dest, size_t offset, size_t len);

/** Do write operation on a character device */
int vmm_chardev_dowrite(vmm_chardev_t * cdev,
			char *src, size_t offset, size_t len);

/** Register character device to device driver framework */
int vmm_chardev_register(vmm_chardev_t * cdev);

/** Unregister character device from device driver framework */
int vmm_chardev_unregister(vmm_chardev_t * cdev);

/** Find a character device in device driver framework */
vmm_chardev_t *vmm_chardev_find(const char *name);

/** Get character device with given number */
vmm_chardev_t *vmm_chardev_get(int num);

/** Count number of character devices */
u32 vmm_chardev_count(void);

/** Initalize character device framework */
int vmm_chardev_init(void);

#endif /* __VMM_CHARDEV_H_ */
