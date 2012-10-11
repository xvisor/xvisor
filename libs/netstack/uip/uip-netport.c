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
 * @file uip-netport.c
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief source file of netport interface of uip
 */

#include <vmm_stdio.h>
#include <vmm_heap.h>
#include <vmm_error.h>
#include <vmm_spinlocks.h>
#include <vmm_completion.h>
#include <net/vmm_protocol.h>
#include <net/vmm_mbuf.h>
#include <net/vmm_netport.h>
#include <libs/list.h>
#include <libs/stringlib.h>

#include "uip.h"
#include "uip-fw.h"
#include "uip-arp.h"

#undef UIP_DEBUG

#ifdef UIP_DEBUG
#define DPRINTF(fmt, ...)	do { vmm_printf(fmt, ## __VA_ARGS__); } while(0)
#else
#define DPRINTF(fmt, ...)	do {} while(0)
#endif

/* Right now we implement a single rx_buffer */
static struct uip_port_state {
	vmm_spinlock_t lock;
	struct dlist rxbuf;
	struct vmm_completion rx_possible;
	struct vmm_netport *port;
	struct uip_fw_netif *netif;
	bool link_down;
} uip_port_state;

static void uip_set_link(struct vmm_netport *port)
{
	struct uip_port_state *s = &uip_port_state;
	s->link_down = !(port->flags & VMM_NETPORT_LINK_UP);
	return;
}

static int uip_can_receive(struct vmm_netport *port)
{
	return TRUE;
}

static int uip_switch2port_xfer(struct vmm_netport *port,
			 	struct vmm_mbuf *mbuf)
{
	struct uip_port_state *s = &uip_port_state;
	int rc = VMM_OK;
	unsigned long flags;
#ifdef UIP_DEBUG
	char tname[30];
#endif
	u8 *dstmac = ether_dstmac(mtod(mbuf, u8 *));
	/* do not accept frames which do not have either 
	 * our MAC or broadcast MAC */
	DPRINTF("UIP received frame with MAC[%s]",
			ethaddr_to_str(tname, srcmac));
	if(compare_ether_addr(dstmac, port->macaddr)
		&& !is_broadcast_ether_addr(dstmac)) {
		/* Reject packets addressed for someone else */
		DPRINTF("  and rejected \n");
		return VMM_EFAIL;
	} else {
		DPRINTF("  and accepted \n");
	}	
	vmm_spin_lock_irqsave(&s->lock, flags);
	list_add_tail(&mbuf->m_list, &s->rxbuf);
	vmm_spin_unlock_irqrestore(&s->lock, flags);
	vmm_completion_complete(&s->rx_possible);

	return rc;
}

int uip_netport_loopback_send(struct vmm_mbuf *mbuf)
{
	struct uip_port_state *s = &uip_port_state;
	if(mbuf == NULL)
		vmm_panic("%s: mbuf NULL\n", __func__);
	u8 *dstmac = ether_dstmac(mtod(mbuf, u8 *));
	u8 *srcmac = ether_srcmac(mtod(mbuf, u8 *));

	memcpy(dstmac, s->port->macaddr, 6);
	memcpy(srcmac, s->port->macaddr, 6);
	return uip_switch2port_xfer(s->port, mbuf);
}

/**
 *  uIP-daemon calls this directly to send out the frame 
 *  present in uip_buf 
 */
void uip_netport_send(void)
{
	struct vmm_mbuf *mbuf;
	struct uip_port_state *s = &uip_port_state;
	struct vmm_netport *port = s->port;
	if(!s->link_down && (uip_len > 0)) {
		/* Create a mbuf out of the uip_buf and uip_len */
		MGETHDR(mbuf, 0, 0);
		MEXTADD(mbuf, uip_buf, UIP_BUFSIZE + 2, NULL, NULL);
		mbuf->m_len = mbuf->m_pktlen = uip_len;

		if(memcmp(ether_dstmac(uip_buf), uip_ethaddr.addr, 6)) {
			/* send this mbuf to the netswitch if it is
			 * not addressed to us */
			vmm_port2switch_xfer(port, mbuf);
		} else {
			uip_netport_loopback_send(mbuf);
		}
		/* Allocate a new replacement uip_buf */
		uip_buf = vmm_malloc(UIP_BUFSIZE + 2);
	}
	/* Do we need the following ? perhaps not */
	/* uip_len = 0; */
}

/**
 *  TX hook for the netport interface (netif) as required by uip_fw
 */
static u8 uip_netport_output(void *priv)
{
	uip_netport_send();
	return 0;
}

/**
 *  Fills the uip_buf with packet from RX queue. In case RX queue is
 *  empty, we wait for sometime.
 */
int uip_netport_read(void)
{
	struct vmm_mbuf *mbuf;
	struct dlist *node;
	unsigned long flags;
	u64 timeout = 50000000;
	struct uip_port_state *s = &uip_port_state;

	/* Keep trying till RX buf is not empty */
	vmm_spin_lock_irqsave(&s->lock, flags);
	while(list_empty(&s->rxbuf)) {
		vmm_spin_unlock_irqrestore(&s->lock, flags);
		if(timeout) {
			/* Still time left for timeout so we wait */
			vmm_completion_wait_timeout(&s->rx_possible, &timeout);
		} else {
			/* We timed-out and buffer is still empty, so return */
			uip_len = 0;
			return uip_len;
		}
		vmm_spin_lock_irqsave(&s->lock, flags);
	}
	/* At this point we are sure rxbuf is non-empty, so we just
	 * dequeue a packet */
	node = list_pop(&s->rxbuf);
	mbuf = m_list_entry(node);
	vmm_spin_unlock_irqrestore(&s->lock, flags);
	if(mbuf == NULL) {
		vmm_panic("%s: mbuf is null\n", __func__);
	}
	if(!uip_buf) {
		vmm_panic("%s: uip_buf is null\n", __func__);
	}
	/* Copy the data from mbuf to uip_buf */
	uip_len = min(UIP_BUFSIZE, mbuf->m_pktlen);
	m_copydata(mbuf, 0, uip_len, uip_buf);
	/* Free the mbuf */
	m_freem(mbuf);
	return uip_len;
}

int uip_netport_init(void)
{
	struct vmm_netswitch *nsw;
	struct uip_port_state *s = &uip_port_state;
	struct uip_fw_netif *netif;
	uip_ipaddr_t ipaddr;
	char tname[64];

	uip_buf = vmm_malloc(UIP_BUFSIZE + 2);
	if(!uip_buf) {
		vmm_panic("%s: uip_buf alloc failed\n", __func__);
	}

	INIT_SPIN_LOCK(&s->lock);
	INIT_LIST_HEAD(&s->rxbuf);
	INIT_COMPLETION(&s->rx_possible);

	/* Get the first netswitch */
	nsw = vmm_netswitch_get(0);
	if(!nsw) {
		vmm_panic("No netswitch found\n");
	}
	/* Create a port-name */
	vmm_sprintf(tname, "%s-uip", nsw->name); 
	/* Allocate a netport for this netswitch */
	s->port = vmm_netport_alloc(tname);
	if(!s->port) {
		vmm_printf("UIP->netport alloc failed\n");
		return VMM_EFAIL;
	}
	/* Allocate a uip_fw_netif */ 
	netif = vmm_malloc(sizeof(struct uip_fw_netif));
	if(!netif) {
		vmm_printf("UIP->netif alloc failed\n");
		return VMM_EFAIL;
	}
	/* Register the netport */
	s->port->mtu = UIP_BUFSIZE;
	s->port->link_changed = uip_set_link;
	s->port->can_receive = uip_can_receive;
	s->port->switch2port_xfer = uip_switch2port_xfer;
	s->port->priv = s;
	s->netif = netif;

	vmm_netport_register(s->port);
	/* Attach with the netswitch */
	vmm_netswitch_port_add(nsw, s->port);
	/* Notify our ethernet address */
	uip_setethaddr(((struct uip_eth_addr *)(s->port->macaddr)));
	/* Generate an IP address */
	uip_ipaddr(ipaddr, 192,168,0,1);
	uip_fw_setipaddr(netif, ipaddr);
	uip_ipaddr(ipaddr, 255,255,255,0);
	uip_fw_setnetmask(netif, ipaddr);
	/* Register the netif with uip stack */
	netif->output = &uip_netport_output;
	netif->priv = s;
	uip_fw_register(netif);
	/* Set this interface as default one */
	uip_fw_default(netif);
	return 0;
}

