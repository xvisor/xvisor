/**
 * Copyright (c) 2010 Himanshu Chauhan.
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
 * @file vmm_netdev.h
 * @version 1.0
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief Network Device framework header
 */

#ifndef __VMM_NETDEV_H_
#define __VMM_NETDEV_H_

#include <vmm_types.h>
#include <vmm_spinlocks.h>
#include <vmm_devdrv.h>

#define VMM_NETDEV_CLASS_NAME				"network"
#define VMM_NETDEV_CLASS_IPRIORITY			1

struct vmm_netdev;
typedef int (*vmm_netdev_ioctl_t) (struct vmm_netdev * ndev,
				   int cmd, void *buf, size_t buf_len);
typedef int (*vmm_netdev_read_t) (struct vmm_netdev * ndev,
				  char *dest, size_t offset, size_t len);
typedef int (*vmm_netdev_write_t) (struct vmm_netdev * ndev,
				   char *src, size_t offset, size_t len);

struct vmm_netdev {
	char name[32];
	struct vmm_device *dev;
	vmm_netdev_ioctl_t ioctl;
	vmm_netdev_read_t read;
	vmm_netdev_write_t write;
	void *priv;
};

/** Do ioctl operation on a network device */
int vmm_netdev_doioctl(struct vmm_netdev * ndev, 
			int cmd, void *buf, size_t buf_len);

/** Do read operation on a network device */
int vmm_netdev_doread(struct vmm_netdev * ndev,
		      char *dest, size_t offset, size_t len);

/** Do write operation on a network device */
int vmm_netdev_dowrite(struct vmm_netdev * ndev,
		       char *src, size_t offset, size_t len);

/** Register network device to device driver framework */
int vmm_netdev_register(struct vmm_netdev * ndev);

/** Unregister network device from device driver framework */
int vmm_netdev_unregister(struct vmm_netdev * ndev);

/** Find a network device in device driver framework */
struct vmm_netdev *vmm_netdev_find(const char *name);

/** Get network device with given number */
struct vmm_netdev *vmm_netdev_get(int num);

/** Count number of network devices */
u32 vmm_netdev_count(void);

#endif /* __VMM_NETDEV_H_ */
