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
#include <vmm_devdrv.h>
#include <vmm_mutex.h>
#include <vmm_spinlocks.h>
#include <vmm_completion.h>
#include <libs/list.h>

#define VMM_NETSWITCH_CLASS_NAME	"netswitch"

struct vmm_netswitch;
struct vmm_netport;
struct vmm_mbuf;

struct vmm_netswitch {
	char *name;
	int flags;
	struct vmm_device *dev;
	/* Mutex to protect net switch */
	struct vmm_mutex lock;
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

	/* Additional fields for thread-based bottom-half of net switch
	 * Note: this fields should not be accessed directly
	 */
	struct vmm_thread *thread;
	struct vmm_completion rx_not_empty;
	u32 rx_count;
	struct dlist rx_list;
	vmm_spinlock_t rx_list_lock;
};

/** Transfer packets from port to switch */
int vmm_netswitch_port2switch(struct vmm_netport *src, 
			      struct vmm_mbuf *mbuf);

#define vmm_port2switch_xfer 	vmm_netswitch_port2switch

/** Transfer packets from switch to port */
int vmm_netswitch_switch2port(struct vmm_netswitch *nsw,
			      struct vmm_netport *dst, 
			      struct vmm_mbuf *mbuf);

#define vmm_switch2port_xfer 	vmm_netswitch_switch2port

/** Allocate new network switch 
 *  @name name of the network switch
 *  @thread_prio priority of the network switch thread
 */
struct vmm_netswitch *vmm_netswitch_alloc(char *name, u32 thread_prio);

/** Deallocate a network switch */
void vmm_netswitch_free(struct vmm_netswitch *nsw);

/** Add a port to the netswitch */
int vmm_netswitch_port_add(struct vmm_netswitch *nsw, 
			   struct vmm_netport *port);

/** Remove a port to the netswitch */
int vmm_netswitch_port_remove(struct vmm_netport *port);

/** Register network switch to network switch framework */
int vmm_netswitch_register(struct vmm_netswitch *nsw, 
			   struct vmm_device *dev,
			   void *priv);

/** Unregister network switch from network switch framework */
int vmm_netswitch_unregister(struct vmm_netswitch *nsw);

/** Count number of network switches */
u32 vmm_netswitch_count(void);

/** Find a network switch in device driver framework */
struct vmm_netswitch *vmm_netswitch_find(const char *name);

/** Get network switch with given number */
struct vmm_netswitch *vmm_netswitch_get(int num);

#endif /* __VMM_NETSWITCH_H_ */

