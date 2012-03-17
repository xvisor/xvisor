/**
 * Copyright (c) 2010 Pranav Sawargaonkar.
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
 * @file vmm_netswitch.h
 * @author Pranav Sawargaonkar <pranav.sawargaonkar@gmail.com>
 * @brief Switch/Bridge layer for packet switching.
 */

#ifndef __VMM_NETSWITCH_H_
#define __VMM_NETSWITCH_H_

#include <vmm_types.h>
#include <vmm_spinlocks.h>
#include <vmm_devdrv.h>
#include <net/vmm_netdev.h>

#define VMM_NETSWITCH_CLASS_NAME		"network-switch"
#define VMM_NETSWITCH_CLASS_IPRIORITY		2

/* Port Flags */
#define VMM_NETSWITCH_PORT_PHYSICAL		1    /* Physical device Port */
#define VMM_NETSWITCH_PORT_VIRTUAL		2    /* Guest Port */
#define VMM_NETSWITCH_PORT_UPLINK		4    /* Current Uplink Port */

/* Port Status Flags */
#define    VMM_NETSWITCH_PORT_UNINITIALIZED	0
#define    VMM_NETSWITCH_PORT_UP		1
#define    VMM_NETSWITCH_PORT_DOWN		2

struct vmm_netswitch_port {
	int		id;
	struct dlist    head;
	struct vmm_netdev    *ndev;	/* Netdevice associated, if
                         		 * port is physical one
					 */
	int    port_flags;
	int    port_status_flags;
};

struct vmm_netswitch {
	char			  *name;
	struct dlist		  port_list;	/* Port list */
	struct vmm_netswitch_port *uplink;	/* Active Uplink Port */
	int			  flags;
	struct vmm_device 	  *dev;
};

/** Allocate new network switch */
struct vmm_netswitch *vmm_netswitch_alloc(char *name);

/** Register network switch to network switch framework */
int vmm_netswitch_register(struct vmm_netswitch *nsw);

/** Unregister network switch from network switch framework */
int vmm_netswitch_unregister(struct vmm_netswitch *nsw);

/** Count number of network switches */
u32 vmm_netswitch_count(void);

/** Find a network switch in device driver framework */
struct vmm_netswitch *vmm_netswitch_find(const char *name);

/** Get network switch with given number */
struct vmm_netswitch *vmm_netswitch_get(int num);

#if 0
struct vmm_netswitch_port *vmm_netswitch_port_alloc(void);
int struct vmm_netswitch_port_register(struct vmm_netswitch *netsw,
                    int uplink_flags);
int struct vmm_netswitch_port_unregister(struct vmm_netswitch *netsw,
                    int port_id);
#endif

#endif /* __VMM_NETSWITCH_H_ */

