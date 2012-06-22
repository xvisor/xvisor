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
 * @file net_bridge.c
 * @author Sukanto Ghosh <sukantoghosh@gmail.com>
 * @brief Default packet switch implementation.
 */

#include <vmm_heap.h>
#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_string.h>
#include <vmm_devdrv.h>
#include <vmm_modules.h>
#include <vmm_spinlocks.h>
#include <vmm_workqueue.h>
#include <list.h>
#include <net/vmm_protocol.h>
#include <net/vmm_mbuf.h>
#include <net/vmm_netswitch.h>
#include <net/vmm_netport.h>

#undef DEBUG_NETBRIDGE

#ifdef DEBUG_NETBRIDGE
#define DPRINTF(fmt, ...) \
	do { vmm_printf(fmt , ## __VA_ARGS__); } while (0)
#else
#define DPRINTF(fmt, ...) do {} while(0)
#endif

#define VMM_NETBRIDGE_RX_BUFLEN		20

static struct vmm_netbridge_ctrl {
	int head;
	int tail;
	struct vmm_netport **src_port;
	struct vmm_mbuf **mbuf;
	struct vmm_work **work;
	struct vmm_workqueue *wqueue;
	vmm_spinlock_t lock;
} netbridge_ctrl;

#define rxbuf_head		(netbridge_ctrl.head)
#define rxbuf_tail		(netbridge_ctrl.tail)
#define rxbuf_empty(void) 	(netbridge_ctrl.head == -1)
#define rxbuf_full(void) 	(netbridge_ctrl.head ==	((netbridge_ctrl.tail+1)%VMM_NETBRIDGE_RX_BUFLEN))

/* Should be called before enqueue to determine the enqueue location
 *
 * Returns the location where to place the new data
 *
 * Assumptions:	- lock is held
 * 		- rxbuf_full() is FALSE
 */
static int rxbuf_enqueue(void)
{
	if(netbridge_ctrl.head == -1) {
		netbridge_ctrl.head = netbridge_ctrl.tail = 0;
	} else {
		netbridge_ctrl.tail = 
			(netbridge_ctrl.tail+1)%VMM_NETBRIDGE_RX_BUFLEN;
	}
	return netbridge_ctrl.tail;
}

/* Should be called after dequeue to update the head/tail pointers
 *
 * Assumptions: - lock is held
 * 		- rxbuf_empty() is false
 */
static void rxbuf_dequeue(void)
{
	if(netbridge_ctrl.head == netbridge_ctrl.tail) {
		netbridge_ctrl.head = netbridge_ctrl.tail = -1;
	} else {
		netbridge_ctrl.head = 
			(netbridge_ctrl.head+1)%VMM_NETBRIDGE_RX_BUFLEN;
	}
}

void vmm_netbridge_tx(struct vmm_work *work)
{
	struct dlist *l;
	bool broadcast = TRUE;
	struct vmm_mbuf *mbuf; 		
	struct vmm_netswitch *nsw; 	
	const u8 *srcmac, *dstmac;	 	
	struct vmm_netport *dst_port, *src_port;

	vmm_spin_lock(&netbridge_ctrl.lock);
	if(unlikely(rxbuf_empty())) {
		vmm_spin_unlock(&netbridge_ctrl.lock);
		return;
	} else {
		mbuf = netbridge_ctrl.mbuf[rxbuf_head];
		src_port = netbridge_ctrl.src_port[rxbuf_head];
		rxbuf_dequeue();
	}
	vmm_spin_unlock(&netbridge_ctrl.lock);

	nsw = src_port->nsw;
	srcmac = ether_srcmac(mtod(mbuf, u8 *));
	dstmac = ether_dstmac(mtod(mbuf, u8 *));

	/* TODO: Learning not required as it is assumed that every port notifies 
	 * its macaddr change, multiple macs will still work because of 
	 * broadcast (but need to optimize such case)  */

	/* Find if the frame should be unicast because it satisfies both of these:
	 * Case 1: It is not a broadcast MAC address, and
	 * Case 2: We do have the MAC->port mapping 
	 */
	if(!is_broadcast_ether_addr(dstmac)) {
		list_for_each(l, &nsw->port_list) {
			   if(!compare_ether_addr(list_port(l)->macaddr, dstmac)) {
				dst_port = list_port(l); 
				broadcast = FALSE;
				break;
			   }
		}
	}		

	if(broadcast) {
		DPRINTF("netbridge: broadcasting\n");
		list_for_each(l, &nsw->port_list) {
			   if(compare_ether_addr(list_port(l)->macaddr, srcmac)) {
				if(list_port(l)->can_receive && 
				   !list_port(l)->can_receive(list_port(l))) {
				   continue;
				}
 				MADDREFERENCE(mbuf);
				MCLADDREFERENCE(mbuf);
				list_port(l)->switch2port_xfer(list_port(l), mbuf);
			   }
		}
		m_freem(mbuf);
	} else {
		DPRINTF("netbridge: unicasting to \"%s\"\n", dst_port->name);
		if(!dst_port->can_receive || dst_port->can_receive(dst_port)) {
	 		dst_port->switch2port_xfer(dst_port, mbuf);
		} else {
			/* Free the mbuf if destination cannot do rx */
			m_freem(mbuf);
		}
	}
}	

int vmm_netbridge_rx_handler(struct vmm_netport *src_port, 
			      struct vmm_mbuf *mbuf)
{
	int index;
#ifdef DEBUG_NETBRIDGE
	const u8 *srcmac = ether_srcmac(mtod(mbuf, u8 *));
	const u8 *dstmac = ether_dstmac(mtod(mbuf, u8 *));
	int i;
	char tname[30];

	DPRINTF("netbridge: got pkt with srcaddr[%s]", ethaddr_to_str(tname, srcmac));
	DPRINTF(", dstaddr[%s]", ethaddr_to_str(tname, dstmac));
	DPRINTF(", ethertype: 0x%04X\n", ether_type(mtod(mbuf, u8 *)));
#ifdef DUMP_NETBRIDGE_PKT
	if(ether_type(mtod(mbuf, u8 *)) == 0x0806	/* ARP */) {
		DPRINTF("\tARP-HType: 0x%04X",	  arp_htype(ether_payload(mtod(mbuf, u8 *)));
		DPRINTF("\n\tARP-PType: 0x%04X",  arp_ptype(ether_payload(mtod(mbuf, u8 *)));
		DPRINTF("\n\tARP-Hlen: 0x%02X",   arp_hlen(ether_payload(mtod(mbuf, u8 *)));
		DPRINTF("\n\tARP-Plen: 0x%02X",   arp_plen(ether_payload(mtod(mbuf, u8 *)));
		DPRINTF("\n\tARP-Oper: 0x%04X",   arp_oper(ether_payload(mtod(mbuf, u8 *)));
		DPRINTF("\n\tARP-SHA: %s",  	  ethaddr_to_str(tname, arp_sha(ether_payload((mtod(mbuf, u8 *)))));
		DPRINTF("\n\tARP-SPA: %s",  	  ipaddr_to_str(tname, arp_spa(ether_payload((mtod(mbuf, u8 *)))));
		DPRINTF("\n\tARP-THA: %s",  	  ethaddr_to_str(tname, arp_tha(ether_payload((mtod(mbuf, u8 *)))));
		DPRINTF("\n\tARP-TPA: %s",  	  ipaddr_to_str(tname, arp_tpa(ether_payload((mtod(mbuf, u8 *)))));
		DPRINTF("\n");
	}
#endif
#endif
	vmm_spin_lock(&netbridge_ctrl.lock);
	if(rxbuf_full()) {
		vmm_spin_unlock(&netbridge_ctrl.lock);
		return VMM_EFAIL;
	}
	index = rxbuf_enqueue();
	vmm_spin_unlock(&netbridge_ctrl.lock);
	netbridge_ctrl.src_port[index] = src_port;
	netbridge_ctrl.mbuf[index] = mbuf;
	INIT_WORK(netbridge_ctrl.work[index], vmm_netbridge_tx, (void *)index);

	vmm_workqueue_schedule_work(netbridge_ctrl.wqueue, netbridge_ctrl.work[index]);	

	/* Forward the skb to the target port if we know exactly */
	return VMM_OK;
}

int vmm_netbridge_enable_port(struct vmm_netport *port)
{
	/* Notify the port about the link-status change */
	port->flags |= VMM_NETPORT_LINK_UP;
	port->link_changed(port);

	return VMM_OK;
}

int vmm_netbridge_disable_port(struct vmm_netport *port)
{
	/* Notify the port about the link-status change */
	port->flags &= ~VMM_NETPORT_LINK_UP;
	port->link_changed(port);

	return VMM_OK;
}

int vmm_netbridge_probe(struct vmm_device *dev,
		     const struct vmm_devid *devid)
{
	struct vmm_netswitch *nsw;
	int rc = VMM_OK;
	int i;

	nsw = vmm_netswitch_alloc(dev->node->name);
	if(!nsw) {
		rc = VMM_EFAIL;
		goto vmm_netbridge_probe_failed;
	}
	nsw->port2switch_xfer = vmm_netbridge_rx_handler;
	nsw->enable_port = vmm_netbridge_enable_port;
	nsw->disable_port = vmm_netbridge_disable_port;

	netbridge_ctrl.head = -1;
	netbridge_ctrl.tail = -1;
	netbridge_ctrl.src_port = vmm_malloc(sizeof(struct vmm_netport *) 
			* VMM_NETBRIDGE_RX_BUFLEN);
	netbridge_ctrl.mbuf = vmm_malloc(sizeof(struct vmm_mbuf *) 
			* VMM_NETBRIDGE_RX_BUFLEN);
	netbridge_ctrl.work = vmm_malloc(sizeof(struct vmm_work *) 
			* VMM_NETBRIDGE_RX_BUFLEN);
	netbridge_ctrl.wqueue = vmm_workqueue_create(nsw->name, VMM_THREAD_DEF_PRIORITY);
	if(!netbridge_ctrl.wqueue ||
	   !netbridge_ctrl.src_port ||
	   !netbridge_ctrl.mbuf ||
	   !netbridge_ctrl.work) {
		vmm_panic("netbridge: allocations failed");
	}

	for(i=0; i<VMM_NETBRIDGE_RX_BUFLEN; i++) {
		netbridge_ctrl.work[i] = vmm_malloc(sizeof(struct vmm_work));
		if(!netbridge_ctrl.work[i]) {
			vmm_panic("netbridge: allocations failed");
		}
	}

	INIT_SPIN_LOCK(&netbridge_ctrl.lock);

	vmm_netswitch_register(nsw);

vmm_netbridge_probe_failed:
	return rc;
}

static int vmm_netbridge_remove(struct vmm_device *dev)
{
	struct vmm_netswitch *nsw = dev->priv;
	vmm_netswitch_unregister(nsw);
	return VMM_OK;
}

static struct vmm_devid def_netswitch_devid_table[] = {
	{.type = "netswitch",.compatible = "bridge"},
	{ /* end of list */ },
};

static struct vmm_driver net_bridge = {
	.name = "netbridge",
	.match_table = def_netswitch_devid_table,
	.probe = vmm_netbridge_probe,
	.remove = vmm_netbridge_remove,
};

int __init vmm_netbridge_init(void)
{
	return vmm_devdrv_register_driver(&net_bridge);
}

void vmm_netbridge_exit(void)
{
	vmm_devdrv_unregister_driver(&net_bridge);
}

