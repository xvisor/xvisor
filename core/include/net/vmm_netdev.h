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
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @author Pranav Sawargaonkar <pranav.sawargaonkar@gmail.com>
 * @brief Network Device framework header
 */

#ifndef __VMM_NETDEV_H_
#define __VMM_NETDEV_H_

#include <vmm_types.h>
#include <vmm_spinlocks.h>
#include <vmm_devdrv.h>

#define VMM_NETDEV_CLASS_NAME                  "netdev"

#define MAX_VMM_NETDEV_NAME_LEN			32
#define MAX_VMM_NDEV_HW_ADDRESS			32

enum vmm_netdev_status {
	VMM_NETDEV_UNINITIALIZED = 0,
	VMM_NETDEV_REGISTERED,
};

struct vmm_netdev;

struct vmm_netdev_ops {
	int (*ndev_init) (struct vmm_netdev *ndev);
	int (*ndev_open) (struct vmm_netdev *ndev);
	int (*ndev_close) (struct vmm_netdev *ndev);
	int (*ndev_xmit) (struct vmm_netdev *ndev, void *buf);
};

struct vmm_netdev {
	char name[MAX_VMM_NETDEV_NAME_LEN];
	struct vmm_device *dev;
	struct vmm_netdev_ops *dev_ops;
	unsigned int state;
	void *priv;		/* Driver specific private data */
	void *nsw_priv;		/* VMM virtual packet switching layer
				 * specific private data.
				 */
	void *net_priv;		/* VMM specific private data -
				 * Usecase is currently undefined
				 */
	unsigned char hw_addr[MAX_VMM_NDEV_HW_ADDRESS];
	unsigned int hw_addr_len;
};


/** Allocate new network device */
struct vmm_netdev *vmm_netdev_alloc(const char *name);

/** Register network device to device driver framework */
int vmm_netdev_register(struct vmm_netdev *ndev);

/** Unregister network device from device driver framework */
int vmm_netdev_unregister(struct vmm_netdev * ndev);

/** Find a network device in device driver framework */
struct vmm_netdev *vmm_netdev_find(const char *name);

/** Get network device with given number */
struct vmm_netdev *vmm_netdev_get(int num);

/** Count number of network devices */
u32 vmm_netdev_count(void);


#endif /* __VMM_NETDEV_H_ */
