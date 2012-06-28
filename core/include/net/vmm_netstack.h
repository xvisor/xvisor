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
 * @file vmm_netstack.h
 * @author Sukanto Ghosh <sukantoghosh@gmail.com>
 * @brief Network Stack Interface APIs
 */

#ifndef __VMM_NETSTACK_H_
#define __VMM_NETSTACK_H_

#include <vmm_types.h>
#include <vmm_error.h>

/**
 *  Structure containing the ICMP_ECHO_REPLY parameters
 */
struct vmm_icmp_echo_reply {
	u8 ripaddr[4];
	u8 ttl;
	u16 len;
	u16 seqno;
	u64 rtt;
};

/**
 *  Interface of a network-stack which every network-stack should
 *  implement for use with Xvisor
 */
struct vmm_netstack {
	char *name;
	int (*set_ipaddr)(u8 *ipaddr);
	int (*get_ipaddr)(u8 *ipaddr);
	int (*set_ipmask)(u8 *ipmask);
	int (*get_ipmask)(u8 *ipmask);
	int (*get_hwaddr)(u8 *hwaddr);
	int (*send_icmp_echo)(u8 *ripaddr, u16 size, u16 seqno, 
			      struct vmm_icmp_echo_reply *reply);
	void (*prefetch_arp_mapping)(u8 *ipaddr);
};

/**
 *  API to register a netstack
 *
 *  Please note that only one netstack can be registered. It is
 *  erroneous to compile and use multiple network-stacks at the
 *  same time. 
 */
int vmm_netstack_register(struct vmm_netstack *stack);

/** 
 *  Returns a pointer to the registered netstack
 */
struct vmm_netstack *vmm_netstack_get(void);

/** 
 *  Returns the name of the netstack
 */
inline static char *vmm_netstack_get_name(void)
{
	struct vmm_netstack *stack = vmm_netstack_get();
	if(stack)
		return stack->name;
	else
		return NULL;
}

#define VMM_NETSTACK_OP_DEFINE(op)				\
inline static int vmm_netstack_##op(u8 *param)			\
{								\
	struct vmm_netstack *stack = vmm_netstack_get();	\
	if(stack) {						\
		return stack->op(param);			\
	}							\
	return VMM_EFAIL;					\
}

VMM_NETSTACK_OP_DEFINE(set_ipaddr);
VMM_NETSTACK_OP_DEFINE(get_ipaddr);
VMM_NETSTACK_OP_DEFINE(set_ipmask);
VMM_NETSTACK_OP_DEFINE(get_ipmask);
VMM_NETSTACK_OP_DEFINE(get_hwaddr);

/**
 *  Generates an ICMP echo request to a remote host and blocks
 *  for sometime till the reply is received.
 *
 *  @ipaddr - IP address of the remote host
 *  @size - size of the payload inside the ICMP msg
 *  @seqno - sequence-no to be used in the request
 *  @reply - on success, this stores the parameters of echo_reply
 *
 *  returns 
 *    - VMM_OK - if the echo reply was received.
 *    - VMM_EFAIL - if timedout or no network-stack present
 */
static inline int vmm_netstack_send_icmp_echo(u8 * ipaddr, u16 size,
			u16 seqno, struct vmm_icmp_echo_reply *reply)
{
	struct vmm_netstack *stack = vmm_netstack_get();
	if(stack) {
		return stack->send_icmp_echo(ipaddr, size, seqno, reply);
	}
	return VMM_EFAIL;
}

/** 
 *  An optional hook primarily meant for network-stacks which do not 
 *  support reliable arp output processing
 *
 *  e.g. In case of uIP, if there is no ARP mapping for the destination 
 *  ipaddr of an outgoing packet, an ARP request is sent out but the
 *  original packet is discarded. In such cases this hint will allow to
 *  either refresh an existing ARP entry or prefetch the required ARP
 *  mapping (by sending out ARP-request) to avoid discards.
 *
 *  @ipaddr - IP address whose ARP mapping is to be prefetched/refreshed
 */
static inline void vmm_netstack_prefetch_arp_mapping(u8 * ipaddr)
{
	struct vmm_netstack *stack = vmm_netstack_get();
	if(stack && stack->prefetch_arp_mapping) {
		stack->prefetch_arp_mapping(ipaddr);
	}
}

#endif  /* __VMM_NETSTACK_H_ */

