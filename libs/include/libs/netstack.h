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
 * @file netstack.h
 * @author Sukanto Ghosh <sukantoghosh@gmail.com>
 * @brief Network Stack Interface APIs
 */

#ifndef __NETSTACK_H_
#define __NETSTACK_H_

#include <vmm_types.h>
#include <vmm_error.h>

/**
 * Structure containing the ICMP_ECHO_REPLY parameters
 */
struct icmp_echo_reply {
	u8 ripaddr[4];
	u8 ttl;
	u16 len;
	u16 seqno;
	u64 rtt;
};

/** 
 * Returns the name of the netstack
 */
char *netstack_get_name(void);

/**
 * Set IP-address of the host
 */
int netstack_set_ipaddr(u8 *ipaddr);

/**
 * Get IP-address of the host
 */
int netstack_get_ipaddr(u8 *ipaddr);

/**
 * Set IP-netmask of the host
 */
int netstack_set_ipmask(u8 *ipmask);

/**
 * Get IP-netmask of the host
 */
int netstack_get_ipmask(u8 *ipmask);

/**
 * Get HW-address of the host
 */
int netstack_get_hwaddr(u8 *hwaddr);

/**
 *  Generates an ICMP echo request to a remote host and blocks for 
 *  sometime till the reply is received.
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
int netstack_send_icmp_echo(u8 *ipaddr, u16 size, u16 seqno, 
			    struct icmp_echo_reply *reply);

/** 
 *  This is meant for network-stacks which do not support reliable 
 *  arp output processing
 *
 *  e.g. In case of uIP, if there is no ARP mapping for the destination 
 *  ipaddr of an outgoing packet, an ARP request is sent out but the
 *  original packet is discarded. In such cases this hint will allow to
 *  either refresh an existing ARP entry or prefetch the required ARP
 *  mapping (by sending out ARP-request) to avoid discards.
 *
 *  @ipaddr - IP address whose ARP mapping is to be prefetched/refreshed
 */
void netstack_prefetch_arp_mapping(u8 *ipaddr);

#endif  /* __VMM_NETSTACK_H_ */

