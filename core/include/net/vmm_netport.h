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

#include <vmm_limits.h>
#include <vmm_types.h>
#include <vmm_devdrv.h>
#include <vmm_spinlocks.h>
#include <libs/list.h>

#define VMM_NETPORT_CLASS_NAME		"netport"

/* Port Flags (should be defined as bits) */
#define VMM_NETPORT_LINK_UP		1	/* If this bit is set link is up */

/* Default per-port queue size */
#define VMM_NETPORT_MAX_QUEUE_SIZE	256

/* Default per-port queue size */
#define VMM_NETPORT_DEF_QUEUE_SIZE	(VMM_NETPORT_MAX_QUEUE_SIZE / 4)

struct vmm_netswitch;
struct vmm_netport;
struct vmm_mbuf;

enum vmm_netport_xfer_type {
	VMM_NETPORT_XFER_UNKNOWN,
	VMM_NETPORT_XFER_MBUF,
	VMM_NETPORT_XFER_LAZY
};

struct vmm_netport_xfer {
	struct dlist head;
	struct vmm_netport *port;
	enum vmm_netport_xfer_type type;
	struct vmm_mbuf *mbuf;
	int lazy_budget;
	void *lazy_arg;
	void (*lazy_xfer)(struct vmm_netport *, void *, int);
};

struct vmm_netport {
	struct dlist head;
	char name[VMM_FIELD_NAME_SIZE];
	u32 queue_size;
	int flags;
	int mtu;
	u8 macaddr[6];
	struct vmm_netswitch *nsw;
	struct vmm_device dev;

	/* Per-port pool of xfer instances 
	 * Having all these blocks contiguous eases alloc
	 * and free operations 
	 */
	u32 free_count;
	struct dlist free_list;
	vmm_spinlock_t free_list_lock;
	struct vmm_netport_xfer xfer_pool[VMM_NETPORT_MAX_QUEUE_SIZE];

	/* Link status changed */
	void (*link_changed) (struct vmm_netport *); 
	/* Callback to determine if the port can RX */
	int (*can_receive) (struct vmm_netport *); 
	/* Handle RX from switch to port */
	vmm_spinlock_t switch2port_xfer_lock;
	int (*switch2port_xfer) (struct vmm_netport *, struct vmm_mbuf *);
	/* Port private data */
	void *priv;
};

#define list_port(node)		(container_of((node), struct vmm_netport, head))

/** Allocate new netport xfer instance */
struct vmm_netport_xfer *vmm_netport_alloc_xfer(struct vmm_netport *port);

/** Free netport xfer instance */
void vmm_netport_free_xfer(struct vmm_netport *port, 
			   struct vmm_netport_xfer *xfer);

/** Allocate new netport */
struct vmm_netport *vmm_netport_alloc(char *name, u32 queue_size);

/** Free netport */
int vmm_netport_free(struct vmm_netport *port);

/** Register netport to networking framework */
int vmm_netport_register(struct vmm_netport *port);

/** Unregister netport from networking framework */
int vmm_netport_unregister(struct vmm_netport *port);

/** Count number of netports */
u32 vmm_netport_count(void);

/** Find a netport in networking framework */
struct vmm_netport *vmm_netport_find(const char *name);

/** Get netport with given number */
struct vmm_netport *vmm_netport_get(int num);

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
