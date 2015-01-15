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
 * @brief Generic netswitch interface header.
 */

#ifndef __VMM_NETSWITCH_H_
#define __VMM_NETSWITCH_H_

#include <vmm_types.h>
#include <vmm_limits.h>
#include <vmm_devdrv.h>
#include <libs/list.h>

#define VMM_NETSWITCH_CLASS_NAME	"netswitch"

struct vmm_netswitch;
struct vmm_netport;
struct vmm_mbuf;

struct vmm_netswitch {
	char name[VMM_FIELD_NAME_SIZE];
	int flags;
	struct vmm_device dev;
	/* Lock to protect port list */
	vmm_rwlock_t port_list_lock;
	/* List of ports */
	struct dlist port_list;
	/* Handle RX packets from port to switch */
	int (*port2switch_xfer) (struct vmm_netswitch *,
				 struct vmm_netport *,
				 struct vmm_mbuf *);
	/* Handle enabling of a port */
	int (*port_add) (struct vmm_netswitch *,
			 struct vmm_netport *);
	/* Handle disabling of a port */
	int (*port_remove) (struct vmm_netswitch *,
			    struct vmm_netport *);
	/* Switch private data */
	void *priv;
};

/** Transfer packets from port to switch */
int vmm_port2switch_xfer_mbuf(struct vmm_netport *src,
			      struct vmm_mbuf *mbuf);

/** Lazy transfer from port to switch */
int vmm_port2switch_xfer_lazy(struct vmm_netport *src,
			 void (*lazy_xfer)(struct vmm_netport *, void *, int),
			 void *lazy_arg, int lazy_budget);

/** Transfer packets from switch to port */
int vmm_switch2port_xfer_mbuf(struct vmm_netswitch *nsw,
			      struct vmm_netport *dst,
			      struct vmm_mbuf *mbuf);

/** Allocate new network switch
 *  @name name of the network switch
 */
struct vmm_netswitch *vmm_netswitch_alloc(char *name);

/** Deallocate a network switch */
void vmm_netswitch_free(struct vmm_netswitch *nsw);

/** Add a port to the netswitch */
int vmm_netswitch_port_add(struct vmm_netswitch *nsw,
			   struct vmm_netport *port);

/** Remove a port to the netswitch */
int vmm_netswitch_port_remove(struct vmm_netport *port);

/** Register network switch to network switch framework */
int vmm_netswitch_register(struct vmm_netswitch *nsw,
			   struct vmm_device *parent,
			   void *priv);

/** Unregister network switch from network switch framework */
int vmm_netswitch_unregister(struct vmm_netswitch *nsw);

/** Find a network switch in device driver framework */
struct vmm_netswitch *vmm_netswitch_find(const char *name);

/** Iterate over each network switch in networking framework */
int vmm_netswitch_iterate(struct vmm_netswitch *start, void *data,
			  int (*fn)(struct vmm_netswitch *nsw, void *data));

/** Get default network switch */
struct vmm_netswitch *vmm_netswitch_default(void);

/** Count number of network switches */
u32 vmm_netswitch_count(void);

#endif /* __VMM_NETSWITCH_H_ */

