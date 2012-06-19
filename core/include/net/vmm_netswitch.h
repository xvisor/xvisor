/**
 * Copyright (c) 2010 Pranav Sawargaonkar.
 * Copyright (c) 2012 Sukanto Ghosh.
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
 * @author Sukanto Ghosh <sukantoghosh@gmail.com>
 * @brief Switch/Bridge layer for packet switching.
 */

#ifndef __VMM_NETSWITCH_H_
#define __VMM_NETSWITCH_H_

#include <vmm_types.h>
#include <vmm_spinlocks.h>
#include <vmm_devdrv.h>
#include <net/vmm_netdev.h>

#define VMM_NETSWITCH_CLASS_NAME	"netswitch"

struct vmm_netport;
struct vmm_mbuf;

/* Port Flags */
#define VMM_NETSWITCH_PORT_PHYSICAL		1    /* Physical device Port */
#define VMM_NETSWITCH_PORT_VIRTUAL		2    /* Guest Port */
#define VMM_NETSWITCH_PORT_UPLINK		4    /* Current Uplink Port */

typedef int (*vmm_netswitch_rx_handle_t) (struct vmm_netport *src_port, struct vmm_mbuf *mbuf);
typedef int (*vmm_netswitch_en_port_handle_t) (struct vmm_netport *port);
typedef int (*vmm_netswitch_dis_port_handle_t) (struct vmm_netport *port);

struct vmm_netswitch {
	char *name;
	struct dlist port_list;	/* Port list */
	int flags;
//	struct vmm_netport *uplink;	/* Active Uplink Port */
	struct vmm_device *dev;
	void *priv;
	/* Handle RX packets from port to switch */
	vmm_netswitch_rx_handle_t	port2switch_xfer;
	/* Handle enabling of a port */
	vmm_netswitch_en_port_handle_t	enable_port;
	/* Handle disabling of a port */
	vmm_netswitch_dis_port_handle_t	disable_port;
};

/** Allocate new network switch */
struct vmm_netswitch *vmm_netswitch_alloc(char *name);

/** Register network switch to network switch framework */
int vmm_netswitch_register(struct vmm_netswitch *nsw);

/** Unregister network switch from network switch framework */
int vmm_netswitch_unregister(struct vmm_netswitch *nsw);

/** Add a port to the netswitch */
int vmm_netswitch_port_add(struct vmm_netswitch *nsw, 
			   struct vmm_netport *port);

/** Remove a port to the netswitch */
int vmm_netswitch_port_remove(struct vmm_netport *port);

/** Count number of network switches */
u32 vmm_netswitch_count(void);

/** Find a network switch in device driver framework */
struct vmm_netswitch *vmm_netswitch_find(const char *name);

/** Get network switch with given number */
struct vmm_netswitch *vmm_netswitch_get(int num);

#endif /* __VMM_NETSWITCH_H_ */

