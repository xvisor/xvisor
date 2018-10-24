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

struct vmm_netswitch_policy;
struct vmm_netswitch;
struct vmm_netport;
struct vmm_netport_lazy;
struct vmm_mbuf;

struct vmm_netswitch {
	/* === Private members === */
	/* Underly class device */
	struct vmm_device dev;
	/* Lock to protect port list */
	vmm_rwlock_t port_list_lock;
	/* List of ports */
	struct dlist port_list;
	/* === Public members === */
	/* Policy */
	struct vmm_netswitch_policy *policy;
	/* Name */
	char name[VMM_FIELD_NAME_SIZE];
	/* Flags */
	int flags;
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

struct vmm_netswitch_policy {
	/* === Private members === */
	struct dlist head;
	/* === Public members === */
	char name[VMM_FIELD_NAME_SIZE];
	struct vmm_netswitch *(*create)(struct vmm_netswitch_policy *policy,
					const char *name,
					int argc, char **argv);
	void (*destroy)(struct vmm_netswitch_policy *policy,
			struct vmm_netswitch *nsw);
};

/** Transfer packets from port to switch */
int vmm_port2switch_xfer_mbuf(struct vmm_netport *src,
			      struct vmm_mbuf *mbuf);

/** Lazy transfer from port to switch */
int vmm_port2switch_xfer_lazy(struct vmm_netport_lazy *lazy);

/** Transfer packets from switch to port */
int vmm_switch2port_xfer_mbuf(struct vmm_netswitch *nsw,
			      struct vmm_netport *dst,
			      struct vmm_mbuf *mbuf);

/** Allocate new network switch (used by network switch policy)
 *  @name name of the network switch
 */
struct vmm_netswitch *vmm_netswitch_alloc(struct vmm_netswitch_policy *nsp,
					  const char *name);

/** Deallocate a network switch (used by network switch policy) */
void vmm_netswitch_free(struct vmm_netswitch *nsw);

/** Add a port to the netswitch */
int vmm_netswitch_port_add(struct vmm_netswitch *nsw,
			   struct vmm_netport *port);

/** Remove a port to the netswitch */
int vmm_netswitch_port_remove(struct vmm_netport *port);

/** Register a network switch (used by network switch policy) */
int vmm_netswitch_register(struct vmm_netswitch *nsw,
			   struct vmm_device *parent,
			   void *priv);

/** Unregister a network switch (used by network switch policy) */
int vmm_netswitch_unregister(struct vmm_netswitch *nsw);

/** Find a network switch */
struct vmm_netswitch *vmm_netswitch_find(const char *name);

/** Iterate over each network switch */
int vmm_netswitch_iterate(struct vmm_netswitch *start, void *data,
			  int (*fn)(struct vmm_netswitch *nsw, void *data));

/** Get default network switch */
struct vmm_netswitch *vmm_netswitch_default(void);

/** Count number of network switches */
u32 vmm_netswitch_count(void);

/** Register network switch policy */
int vmm_netswitch_policy_register(struct vmm_netswitch_policy *nsp);

/** Unregister network switch policy */
void vmm_netswitch_policy_unregister(struct vmm_netswitch_policy *nsp);

/** Iterate over each network switch policy */
int vmm_netswitch_policy_iterate(struct vmm_netswitch_policy *start,
		void *data, int (*fn)(struct vmm_netswitch_policy *, void *));

/** Find a network switch policy */
struct vmm_netswitch_policy *vmm_netswitch_policy_find(const char *name);

/** Count number of network switch policy */
u32 vmm_netswitch_policy_count(void);

/** Create a network switch using a network switch policy */
int vmm_netswitch_policy_create_switch(const char *policy_name,
				       const char *switch_name,
				       int argc, char **argv);

/** Create a network switch using a network switch policy */
int vmm_netswitch_policy_destroy_switch(struct vmm_netswitch *nsw);

#endif /* __VMM_NETSWITCH_H_ */

