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
#include <vmm_completion.h>

#define MODULE_DESC			"uIP Network Daemon"
#define MODULE_AUTHOR			"Sukanto Ghosh"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(VMM_NET_CLASS_IPRIORITY + 1)
#define	MODULE_INIT			daemon_uip_init
#define	MODULE_EXIT			daemon_uip_exit

#define BUF ((struct uip_eth_hdr *)&uip_buf[0])

u8 *uip_buf;

struct vmm_thread *uip_thread;

#undef UIP_NO_ROUTING

static int uip_loop(void *data)
{
	int i;
	struct timer periodic_timer, arp_timer;

	timer_set(&periodic_timer, CLOCK_SECOND / 2);
	timer_set(&arp_timer, CLOCK_SECOND * 10);

	uip_netport_init();
	uip_init();

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

int uip_netstack_init(void);

/*---------------------------------------------------------------------------*/
static int __init daemon_uip_init(void)
{
	u8 uip_priority;
	u32 uip_time_slice;
	struct vmm_devtree_node * node;
	const char * attrval;
	uip_ipaddr_t ipaddr;
	u8 ip_def[] = {192, 168, 0, 1};
	u8 ip_set[] = {192, 168, 0, 1};
	u8 def_mask[4];

	node = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPARATOR_STRING
				   VMM_DEVTREE_VMMINFO_NODE_NAME
				   VMM_DEVTREE_PATH_SEPARATOR_STRING
				   VMM_DEVTREE_VMMNET_NODE_NAME
				   VMM_DEVTREE_PATH_SEPARATOR_STRING
				   VMM_DEVTREE_NETSTACK_NODE_NAME);
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

	attrval = vmm_devtree_attrval(node, "ipaddr");
	if (attrval) {
		/* Read ip address from DTS */
		vmm_str_to_ipaddr(ip_set, attrval);
	}
	if(ipv4_class_netmask(ip_set, def_mask) == -1) {
		vmm_printf("uIP: Bad IP address in DTS reverting to default IP\n");
		uip_ipaddr(ipaddr, ip_def[0],ip_def[1],ip_def[2],ip_def[3]);
	} else {
		uip_ipaddr(ipaddr, ip_set[0],ip_set[1],ip_set[2],ip_set[3]);
	}
	uip_sethostaddr(ipaddr);

	attrval = vmm_devtree_attrval(node, "netmask");
	if (attrval) {
		/* Read mask address from DTS */
		vmm_str_to_ipaddr(ip_set, attrval);
		uip_ipaddr(ipaddr, ip_set[0],ip_set[1],ip_set[2],ip_set[3]);
	} else {
		/* Apply default netmask as per the IP class */
		uip_ipaddr(ipaddr, def_mask[0],def_mask[1],def_mask[2],def_mask[3]);
	}
	uip_setnetmask(ipaddr);

	uip_netstack_init();

	uip_thread = vmm_threads_create("uip", 
					&uip_loop, 
					NULL, 
					uip_priority,
					uip_time_slice);
	if (!uip_thread) {
		vmm_panic("Creation of uip thread failed.\n");
	}

	vmm_threads_start(uip_thread);

	return VMM_OK;
}

static void __exit daemon_uip_exit(void)
{
	vmm_threads_stop(uip_thread);

	vmm_threads_destroy(uip_thread);
}

VMM_DECLARE_MODULE(MODULE_DESC, 
			MODULE_AUTHOR, 
			MODULE_LICENSE, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
/*---------------------------------------------------------------------------*/

