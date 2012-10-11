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
 * @file uip-netstack.c
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief Source for implementation of netstack interface for uIP
 */

#include <vmm_stdio.h>
#include <vmm_types.h>
#include <vmm_error.h>
#include <vmm_completion.h>
#include <net/vmm_mbuf.h>
#include <libs/stringlib.h>
#include <libs/netstack.h>

#include "uip.h"
#include "uip-arp.h"
#include "uip-netport.h"

struct vmm_completion uip_arp_prefetch_done;
static struct vmm_completion uip_ping_done;

int uip_netstack_init(void)
{
	INIT_COMPLETION(&uip_ping_done);
	INIT_COMPLETION(&uip_arp_prefetch_done);
	return VMM_OK;
}

char *netstack_get_name(void)
{
	return "uIP";
}

int netstack_set_ipaddr(u8 *addr)
{
	uip_ipaddr_t ipaddr;
	uip_ipaddr(ipaddr, addr[0], addr[1], addr[2], addr[3]);
	uip_sethostaddr(ipaddr);
	return VMM_OK;
}

int netstack_get_ipaddr(u8 *addr)
{
	uip_ipaddr_t ipaddr;
	uip_gethostaddr(ipaddr);
	memcpy(addr, ipaddr, 4);
	return VMM_OK;
}

int netstack_set_ipmask(u8 *addr)
{
	uip_ipaddr_t ipaddr;
	uip_ipaddr(ipaddr, addr[0], addr[1], addr[2], addr[3]);
	uip_setnetmask(ipaddr);
	return VMM_OK;
}

int netstack_get_ipmask(u8 *addr)
{
	uip_ipaddr_t ipaddr;
	uip_getnetmask(ipaddr);
	memcpy(addr, ipaddr, 4);
	return VMM_OK;
}

int netstack_get_hwaddr(u8 *addr)
{
	memcpy(addr, &uip_ethaddr, 6);
	return VMM_OK;
}

static struct icmp_echo_reply *uip_ping_reply;

/** 
 * Callback to notify reception of a ICMP_ECHO_REPLY
 */
void uip_ping_callback(struct icmp_echo_reply *reply)
{
	if(uip_ping_reply) {
		memcpy(uip_ping_reply, reply, 
				sizeof(struct icmp_echo_reply));
		vmm_completion_complete(&uip_ping_done);
	}
}

/**
 * uIP doesn't provide a mechanism to create a raw-IP packet so
 * we trigger the sending of ECHO_REQUEST by sending ourself an
 * ECHO_REPLY message with all-zeroes destination IP address.
 *
 * A global completion variable is used to notify the reception 
 * of the actual ECHO_REPLY
 */
int netstack_send_icmp_echo(u8 *ripaddr, u16 size, u16 seqno, 
			    struct icmp_echo_reply *reply)
{
	struct vmm_mbuf *mbuf;
	struct uip_icmp_echo_request *echo_req;
	u16 all_zeroes_addr[] = {0, 0}; 
	u8 *tmp;
	u64 timeout = (u64)20000000000;
	u16 ethsize;

	/* Create a mbuf */
	MGETHDR(mbuf, 0, 0);
	ethsize = UIP_ICMP_LLH_LEN + UIP_ICMP_ECHO_DLEN;
	MEXTMALLOC(mbuf, ethsize, 0);
	mbuf->m_len = mbuf->m_pktlen = ethsize;
	/* Skip the src & dst mac addresses as they will be filled by 
	 * uip_netport_loopback_send */
	tmp = mtod(mbuf, u8 *) + 12;
	/* IPv4 ethertype */
	*tmp++ = 0x08;
	*tmp++ = 0x00;
	/* Fillup the echo_request structure embedded in ICMP payload */
	echo_req = (struct uip_icmp_echo_request *)(tmp + UIP_ICMP_IPH_LEN);
	uip_ipaddr_copy(echo_req->ripaddr, ripaddr);
	echo_req->len = size;
	echo_req->seqno = seqno;
	/* Fillup the IP header */
	uip_create_ip_pkt(tmp, all_zeroes_addr, (ethsize - UIP_LLH_LEN));
	/* Fillup the ICMP header at last as the icmpchksum is calculated 
	 * over entire icmp message */
	uip_create_icmp_pkt(tmp, ICMP_ECHO_REPLY, 
			    (ethsize - UIP_LLH_LEN - UIP_IPH_LEN), 0);

	/* Update pointer to store uip_ping_reply */
	uip_ping_reply = reply;

	/* Send the mbuf to self to trigger ICMP_ECHO */
	uip_netport_loopback_send(mbuf);
	/* Wait for the reply until timeout */
	vmm_completion_wait_timeout(&uip_ping_done, &timeout);
	/* The callback has copied the reply data before completing, so we
	 * can safely set the pointer as NULL to prevent unwanted callbacks */
	uip_ping_reply = NULL;
	if(timeout == (u64)0) 
		return VMM_EFAIL;
	return VMM_OK;
}

/**
 *  Prefetching of ARP mapping is done by sending ourself a broadcast ARP 
 *  message with ARP_HINT as opcode.
 */
void netstack_prefetch_arp_mapping(u8 *ipaddr)
{
	struct vmm_mbuf *mbuf;
	int size;
	u64 timeout = (u64)5000000000;

	/* No need to prefetch our own mapping */
	if(!memcmp(ipaddr, uip_hostaddr, 4)) {
		return;
	}

	/* Create a mbuf */
	MGETHDR(mbuf, 0, 0);
	size = sizeof(struct arp_hdr);
	MEXTMALLOC(mbuf, size, 0);
	mbuf->m_len = mbuf->m_pktlen = size;
	/* Create an ARP HINT packet in the buffer */
	uip_create_broadcast_eth_arp_pkt(mtod(mbuf, u8 *), ipaddr, ARP_HINT);
	/* Send the mbuf to self to trigger ARP prefetch */
	uip_netport_loopback_send(mbuf);

	/* Block till arp prefetch is done */
	vmm_completion_wait_timeout(&uip_arp_prefetch_done, &timeout);
}


