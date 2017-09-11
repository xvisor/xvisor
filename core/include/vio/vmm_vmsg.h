/**
 * Copyright (c) 2017 Anup Patel.
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
 * @file vmm_vmsg.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for virtual messaging subsystem
 */

/*
 * This framework will be used for implementing inter-guest messaging
 * emulators (such as VirtIO RPMSG device).
 *
 * It has three important entities:
 * 1. vmm_vmsg: The acutal message
 * 2. vmm_vmsg_node: A participant in message based communication
 * 3. vmm_vmsg_domain: A group of participants doing message based
 *    communication
 *
 * Each vmm_vmsg_node will have unique address (1024 <). Any vmm_vmsg_node
 * can broadcast message to all nodes of vmm_vmsg_domain by sending
 * message to 0xffffffff.
 *
 * In addition, the vmm_vmsg_node get notifications about ready state
 * of it's peers in same vmm_vmsg_domain.
 */

#ifndef __VMM_VMSG_H__
#define __VMM_VMSG_H__

#include <vmm_limits.h>
#include <vmm_types.h>
#include <vmm_spinlocks.h>
#include <vmm_completion.h>
#include <vmm_mutex.h>
#include <vmm_threads.h>
#include <vmm_notifier.h>
#include <arch_atomic.h>
#include <libs/list.h>

#define VMM_VMSG_IPRIORITY			0

#define VMM_VMSG_NODE_ADDR_MIN			1024
#define VMM_VMSG_NODE_ADDR_ANY			0xFFFFFFFF

/* Notifier event when virtual messaging domain is created */
#define VMM_VMSG_EVENT_CREATE_DOMAIN		0x01
/* Notifier event when virtual messaging domain is destroyed */
#define VMM_VMSG_EVENT_DESTROY_DOMAIN		0x02
/* Notifier event when virtual messaging node is created */
#define VMM_VMSG_EVENT_CREATE_NODE		0x03
/* Notifier event when virtual messaging node is destroyed */
#define VMM_VMSG_EVENT_DESTROY_NODE		0x04

/** Representation of virtual messaging notifier event */
struct vmm_vmsg_event {
	void *data;
};

/** Register a notifier client to receive virtual messaging events */
int vmm_vmsg_register_client(struct vmm_notifier_block *nb);

/** Unregister a notifier client to not receive virtual messaging events */
int vmm_vmsg_unregister_client(struct vmm_notifier_block *nb);

/** Representation of a virtual message */
struct vmm_vmsg {
	atomic_t ref_count;
	u32 dst;
	u32 src;
	void *data;
	size_t len;
	void *priv;
	void (*release) (struct vmm_vmsg *);
};

#define INIT_VMSG(__msg, __dst, __src, __data, __len, __priv, __rel)	\
	do {								\
		arch_atomic_write(&(__msg)->ref_count, 1);		\
		(__msg)->dst = (__dst);					\
		(__msg)->src = (__src);					\
		(__msg)->data = (__data);				\
		(__msg)->len = (__len);					\
		(__msg)->priv = (__priv);				\
		(__msg)->release = (__rel);				\
	} while (0)

/** Representation of a virtual messaging domain */
struct vmm_vmsg_domain {
	struct dlist head;
	char name[VMM_FIELD_NAME_SIZE];
	void *priv;
	struct vmm_thread *worker;
	struct vmm_completion work_avail;
	vmm_spinlock_t work_lock;
	struct dlist work_list;
	struct vmm_mutex node_lock;
	struct dlist node_list;
};

struct vmm_vmsg_node;

/** Representation of a virtual messaging node operations */
struct vmm_vmsg_node_ops {
	void (*peer_up) (struct vmm_vmsg_node *node, u32 peer_addr);
	void (*peer_down) (struct vmm_vmsg_node *node, u32 peer_addr);
	void (*recv_msg) (struct vmm_vmsg_node *node, struct vmm_vmsg *msg);
};

/** Representation of a virtual messaging node */
struct vmm_vmsg_node {
	u32 addr;
	struct dlist head;
	struct dlist domain_head;
	char name[VMM_FIELD_NAME_SIZE];
	void *priv;
	atomic_t is_ready;
	struct vmm_vmsg_domain *domain;
	struct vmm_vmsg_node_ops *ops;
};

/** Increment ref count of virtual message */
void vmm_vmsg_ref(struct vmm_vmsg *msg);

/** Decrement ref count of virtual message */
void vmm_vmsg_dref(struct vmm_vmsg *msg);

/** Allocate new virtual message */
struct vmm_vmsg *vmm_vmsg_alloc(u32 dst, u32 src, void *data, size_t len);

/** Free a virtual message */
static inline void vmm_vmsg_free(struct vmm_vmsg *msg)
{
	vmm_vmsg_dref(msg);
}

/** Create a virtual messaging domain */
struct vmm_vmsg_domain *vmm_vmsg_domain_create(const char *name, void *priv);

/** Destroy a virtual messaging domain */
int vmm_vmsg_domain_destroy(struct vmm_vmsg_domain *domain);

/** Iterate over each virtual messaging domain */
int vmm_vmsg_domain_iterate(struct vmm_vmsg_domain *start, void *data,
			    int (*fn)(struct vmm_vmsg_domain *, void *));

/** Find a virtual messaging domain with given name */
struct vmm_vmsg_domain *vmm_vmsg_domain_find(const char *name);

/** Count of available virtual messaging domains */
u32 vmm_vmsg_domain_count(void);

/** Iterate over each virtual messaging node of a domain */
int vmm_vmsg_domain_node_iterate(struct vmm_vmsg_domain *domain,
				 struct vmm_vmsg_node *start, void *data,
				 int (*fn)(struct vmm_vmsg_node *, void *));

/** Get name of virtual messaging domain */
const char *vmm_vmsg_domain_get_name(struct vmm_vmsg_domain *domain);

/** Create a virtual messaging node */
struct vmm_vmsg_node *vmm_vmsg_node_create(const char *name,
				struct vmm_vmsg_node_ops *ops,
				struct vmm_vmsg_domain *domain,
				void *priv);

/** Destroy a virtual messaging node */
int vmm_vmsg_node_destroy(struct vmm_vmsg_node *node);

/** Retrive private context of virtual messaging node */
static inline void *vmm_vmsg_node_priv(struct vmm_vmsg_node *node)
{
	return (node) ? node->priv : NULL;
}

/** Iterate over each virtual messaging node */
int vmm_vmsg_node_iterate(struct vmm_vmsg_node *start, void *data,
			  int (*fn)(struct vmm_vmsg_node *, void *));

/** Find a virtual messaging node with given name */
struct vmm_vmsg_node *vmm_vmsg_node_find(const char *name);

/** Count of available virtual messaging nodes */
u32 vmm_vmsg_node_count(void);

/** Send message from virtual messaging node */
int vmm_vmsg_node_send(struct vmm_vmsg_node *node, struct vmm_vmsg *msg);

/** Mark virtual messaging node as ready */
void vmm_vmsg_node_ready(struct vmm_vmsg_node *node);

/** Mark virtual messaging node as not-ready */
void vmm_vmsg_node_notready(struct vmm_vmsg_node *node);

/** Check whether virtual messaging node is ready */
bool vmm_vmsg_node_is_ready(struct vmm_vmsg_node *node);

/** Get name of virtual messaging node */
const char *vmm_vmsg_node_get_name(struct vmm_vmsg_node *node);

/** Get domain of virtual messaging node */
struct vmm_vmsg_domain *vmm_vmsg_node_get_domain(struct vmm_vmsg_node *node);

#endif /* __VMM_VMSG_H__ */

