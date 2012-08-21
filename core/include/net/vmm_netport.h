/**
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
 * @file vmm_netport.h
 * @author Sukanto Ghosh <sukantoghosh@gmail.com>
 * @brief Switch interface layer API.
 */

#ifndef __VMM_NETPORT_H_
#define __VMM_NETPORT_H_

#include <vmm_types.h>
#include <vmm_spinlocks.h>
#include <vmm_devdrv.h>
#include <net/vmm_netswitch.h>

#define VMM_NETPORT_CLASS_NAME		"netport"

/* Port Flags (should be defined as bits) */
#define VMM_NETPORT_LINK_UP		1	/* If this bit is set link is up */

typedef int (*vmm_netport_can_rx_t) (struct vmm_netport *port); 
typedef int (*vmm_netport_rx_handle_t) (struct vmm_netport *port, 
					struct vmm_mbuf *mbuf);
typedef void (*vmm_netport_link_change_t) (struct vmm_netport *port); 

struct vmm_netport {
	char *name;
	struct dlist head;
	int flags;
	int mtu;
	u8 macaddr[6];
	struct vmm_netswitch *nsw;
	struct vmm_device *dev;
	void *priv;
	/* Link status changed */
	vmm_netport_link_change_t link_changed;
	/* Callback to determine if the port can RX */
	vmm_netport_can_rx_t  can_receive;
	/* Handle RX from switch to port */
	vmm_netport_rx_handle_t switch2port_xfer;
};

#define list_port(node)		(container_of((node), struct vmm_netport, head))

/** Allocate new network port */
struct vmm_netport *vmm_netport_alloc(char *name);

/** Register network port from network port framework */
int vmm_netport_register(struct vmm_netport *netport);

/** Unregister network port from network port framework */
int vmm_netport_unregister(struct vmm_netport *netport);

/** Count number of network ports */
u32 vmm_netport_count(void);

/** Find a network port in device driver framework */
struct vmm_netport *vmm_netport_find(const char *name);

/** Get network port with given number */
struct vmm_netport *vmm_netport_get(int num);

#define vmm_port2switch_xfer 	vmm_netswitch_port2switch

#define vmm_netport_mac(port)	((port)->macaddr)

#if 0
static inline void vmm_netport_getmac(struct vmm_netport *port, char *dst)
{
	memcpy(dst, port->macaddr, 6);
}

static inline void vmm_netport_setmac(struct vmm_netport *port, char *src)
{
	memcpy(port->macaddr, src, 6);
}
#endif

#endif /* __VMM_NETPORT_H_ */
