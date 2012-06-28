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
 * @file uip-daemon.c
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief source file of uip daemon
 */

#include "uip.h"
#include "timer.h"
#include "uip-fw.h"
#include "uip-arp.h"
#include "uip-netport.h"
#include <mathlib.h>
#include <net/vmm_net.h>
#include <net/vmm_mbuf.h>
#include <net/vmm_protocol.h>
#include <net/vmm_netstack.h>
#include <vmm_heap.h>
#include <vmm_stdio.h>
#include <vmm_string.h>
#include <vmm_types.h>
#include <vmm_error.h>
#include <vmm_modules.h>
#include <vmm_threads.h>
#include <vmm_scheduler.h>
#include <vmm_completion.h>

#define MODULE_VARID			daemon_uip_module
#define MODULE_NAME			"UIP Network Daemon"
#define MODULE_AUTHOR			"Sukanto Ghosh"
#define MODULE_IPRIORITY		(VMM_NET_CLASS_IPRIORITY + 2)
#define	MODULE_INIT			daemon_uip_init
#define	MODULE_EXIT			daemon_uip_exit

#define BUF ((struct uip_eth_hdr *)&uip_buf[0])

u8 *uip_buf;

struct vmm_thread *uip_thread;
struct vmm_completion uip_arp_prefetch_done;
static struct vmm_completion uip_ping_done;

#undef UIP_NO_ROUTING

static int uip_loop(void *data)
{
	int i;
	uip_ipaddr_t ipaddr;
	struct timer periodic_timer, arp_timer;

	timer_set(&periodic_timer, CLOCK_SECOND / 2);
	timer_set(&arp_timer, CLOCK_SECOND * 10);

	INIT_COMPLETION(&uip_ping_done);
	INIT_COMPLETION(&uip_arp_prefetch_done);

	uip_netport_init();
	uip_init();

	uip_ipaddr(ipaddr, 192,168,0,1);
	uip_sethostaddr(ipaddr);
	uip_ipaddr(ipaddr, 255,255,255,0);
	uip_setnetmask(ipaddr);

	while(1) {
		uip_len = uip_netport_read();
		if(uip_len > 0) {
			if(BUF->type == htons(UIP_ETHTYPE_IP)) {
				uip_arp_ipin();
				uip_input();
				/* If the above function invocation resulted in data that
				   should be sent out on the network, the global variable
				   uip_len is set to a value > 0. */
				if(uip_len > 0) {
					uip_arp_out();
#ifdef UIP_NO_ROUTING
					uip_netport_send();
#else
					uip_fw_output();
#endif
				}
			} else if(BUF->type == htons(UIP_ETHTYPE_ARP)) {
				uip_arp_arpin();
				/* If the above function invocation resulted in data that
				   should be sent out on the network, the global variable
				   uip_len is set to a value > 0. */
				if(uip_len > 0) {
#ifdef UIP_NO_ROUTING
					uip_netport_send();
#else
					uip_fw_output();
#endif
				}
			}

		} else if(timer_expired(&periodic_timer)) {
			timer_reset(&periodic_timer);
			for(i = 0; i < UIP_CONNS; i++) {
				uip_periodic(i);
				/* If the above function invocation resulted in data that
				   should be sent out on the network, the global variable
				   uip_len is set to a value > 0. */
				if(uip_len > 0) {
					uip_arp_out();
#ifdef UIP_NO_ROUTING
					uip_netport_send();
#else
					uip_fw_output();
#endif
				}
			}

#if UIP_UDP
			for(i = 0; i < UIP_UDP_CONNS; i++) {
				uip_udp_periodic(i);
				/* If the above function invocation resulted in data that
				   should be sent out on the network, the global variable
				   uip_len is set to a value > 0. */
				if(uip_len > 0) {
					uip_arp_out();
#ifdef UIP_NO_ROUTING
					uip_netport_send();
#else
					uip_fw_output();
#endif
				}
			}
#endif /* UIP_UDP */

			/* Call the ARP timer function every 10 seconds. */
			if(timer_expired(&arp_timer)) {
				timer_reset(&arp_timer);
				uip_arp_timer();
			}
		}
	}
	return VMM_OK;
}

int uip_set_ipaddr(u8 *addr)
{
	uip_ipaddr_t ipaddr;
	uip_ipaddr(ipaddr, addr[0], addr[1], addr[2], addr[3]);
	uip_sethostaddr(ipaddr);
	return VMM_OK;
}

int uip_get_ipaddr(u8 *addr)
{
	uip_ipaddr_t ipaddr;
	uip_gethostaddr(ipaddr);
	vmm_memcpy(addr, ipaddr, 4);
	return VMM_OK;
}

int uip_set_ipmask(u8 *addr)
{
	uip_ipaddr_t ipaddr;
	uip_ipaddr(ipaddr, addr[0], addr[1], addr[2], addr[3]);
	uip_setnetmask(ipaddr);
	return VMM_OK;
}

int uip_get_ipmask(u8 *addr)
{
	uip_ipaddr_t ipaddr;
	uip_getnetmask(ipaddr);
	vmm_memcpy(addr, ipaddr, 4);
	return VMM_OK;
}

int uip_get_hwaddr(u8 *addr)
{
	vmm_memcpy(addr, &uip_ethaddr, 6);
	return VMM_OK;
}

static struct vmm_icmp_echo_reply *uip_ping_reply;

/** 
 * Callback to notify reception of a ICMP_ECHO_REPLY
 */
void uip_ping_callback(struct vmm_icmp_echo_reply *reply)
{
	if(uip_ping_reply) {
		vmm_memcpy(uip_ping_reply, reply, 
				sizeof(struct vmm_icmp_echo_reply));
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
static int uip_send_icmp_echo(u8 *ripaddr, u16 size, u16 seqno, 
			      struct vmm_icmp_echo_reply *reply)
{
	struct vmm_mbuf *mbuf;
	struct uip_icmp_echo_request *echo_req;
	u16 all_zeroes_addr[] = {0, 0}; 
	u8 *tmp;
	u64 timeout = (u64)5000000000;
	u16 ethsize;

	/* Update pointer to store uip_ping_reply */
	uip_ping_reply = reply;
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

	/* Send the mbuf to self to trigger ICMP_ECHO */
	uip_netport_loopback_send(mbuf);
	/* Wait for the reply until timeout */
	vmm_completion_wait_timeout(&uip_ping_done, &timeout);
	if(timeout == (u64)0) 
		return VMM_EFAIL;
	/* The callback has copied the reply data before completing, so we
	 * can safely set the pointer as NULL to prevent unwanted callbacks */
	uip_ping_reply = NULL;
	return VMM_OK;
}

/**
 *  Prefetching of ARP mapping is done by sending ourself a broadcast ARP 
 *  message with ARP_HINT as opcode.
 */
static void uip_prefetch_arp_mapping(u8 *ipaddr)
{
	struct vmm_mbuf *mbuf;
	int size;
	u64 timeout = (u64)5000000000;

	/* No need to prefetch our own mapping */
	if(!vmm_memcmp(ipaddr, uip_hostaddr, 4)) {
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

static struct vmm_netstack uip_stack  = {
	.name = "uIP",
	.set_ipaddr = uip_set_ipaddr,
	.get_ipaddr = uip_get_ipaddr,
	.set_ipmask = uip_set_ipmask,
	.get_ipmask = uip_get_ipmask,
	.get_hwaddr = uip_get_hwaddr,
	.send_icmp_echo = uip_send_icmp_echo,
	.prefetch_arp_mapping = uip_prefetch_arp_mapping,
};

/*---------------------------------------------------------------------------*/
static int __init daemon_uip_init(void)
{
	u8 uip_priority;
	u32 uip_time_slice;
	struct vmm_devtree_node * node;
	const char * attrval;

	/* Retrive mterm time slice */
	node = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPARATOR_STRING
				   VMM_DEVTREE_VMMINFO_NODE_NAME);
	if (!node) {
		return VMM_EFAIL;
	}
	attrval = vmm_devtree_attrval(node,
				      "uip_priority");
	if (attrval) {
		uip_priority = *((u32 *) attrval);
	} else {
		uip_priority = VMM_THREAD_DEF_PRIORITY;
	}
	attrval = vmm_devtree_attrval(node,
				      "uip_time_slice");
	if (attrval) {
		uip_time_slice = *((u32 *) attrval);
	} else {
		uip_time_slice = VMM_THREAD_DEF_TIME_SLICE;
	}

	uip_thread = vmm_threads_create("uip", 
					   &uip_loop, 
					   NULL, 
					   uip_priority,
					   uip_time_slice);
	if (!uip_thread) {
		vmm_panic("Creation of uip thread failed.\n");
	}

	vmm_threads_start(uip_thread);

	vmm_netstack_register(&uip_stack);

	return VMM_OK;
}

static void daemon_uip_exit(void)
{
	vmm_threads_stop(uip_thread);

	vmm_threads_destroy(uip_thread);
}

VMM_DECLARE_MODULE(MODULE_VARID, 
		   MODULE_NAME, 
		   MODULE_AUTHOR, 
		   MODULE_IPRIORITY, 
		   MODULE_INIT, 
		   MODULE_EXIT);
/*---------------------------------------------------------------------------*/

