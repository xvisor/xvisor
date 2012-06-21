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

#include "uip-fw.h"
#include <vmm_stdio.h>
#include <vmm_string.h>
#include <vmm_heap.h>
#include <vmm_error.h>
#include <vmm_string.h>
#include <vmm_spinlocks.h>
#include <list.h>
#include <net/ethernet.h>
#include <net/vmm_mbuf.h>
#include <net/vmm_netport.h>

#define UIP_DEBUG

#ifdef UIP_DEBUG
#define DPRINTF(fmt, ...)	do { vmm_printf(fmt, ## __VA_ARGS__); } while(0)
#else
#define DPRINTF(fmt, ...)	do {} while(0)
#endif

/* Right now we implement a single rx_buffer */
static struct uip_port_state {
	bool link_down;
	vmm_spinlock_t lock;
	struct vmm_mbuf *mbuf;
	struct vmm_netport *port;
	struct uip_fw_netif *netif;
} uip_port_state;

static void uip_set_link(struct vmm_netport *port)
{
	struct uip_port_state *s = &uip_port_state;
	s->link_down = !(port->flags & VMM_NETPORT_LINK_UP);
	return;
}

static int uip_can_receive(struct vmm_netport *port)
{
	struct uip_port_state *s = &uip_port_state;
	int rc = VMM_OK;
	vmm_spin_lock(&s->lock);
	if(s->mbuf == NULL)
		rc = VMM_EFAIL;
	vmm_spin_unlock(&s->lock);
	return rc;
}

void uip_netport_send(void)
{
	struct vmm_mbuf *mbuf;
	struct uip_port_state *s = &uip_port_state;
	struct vmm_netport *port = s->port;
	if(!s->link_down && (uip_len > 0)) {
		/* Create a mbuf out of the uip_buf and uip_len */
		MGETHDR(mbuf, 0, 0);
		MEXTADD(mbuf, uip_buf, uip_len, NULL, NULL);
		/* Add our MAC address to the outgoing frame */
		vmm_memcpy(ether_srcmac(mtod(mbuf, u8 *)), port->macaddr, 6);
		/* send this mbuf out of the port->nsw */
		vmm_port2switch_xfer(port, mbuf);
		/* Allocate a new uip_buf and uip_len to replace sent one */
		uip_buf = vmm_malloc(UIP_BUFSIZE + 2);
	}
	uip_len = 0;
}

static u8 uip_netport_output(void *priv)
{
	uip_netport_send();
	return 0;
}

static int uip_switch2port_xfer(struct vmm_netport *port,
			 	struct vmm_mbuf *mbuf)
{
	struct uip_port_state *s = &uip_port_state;
	int rc = VMM_OK, i;
	u8 *srcmac = ether_srcmac(mtod(mbuf, u8 *));
	/* do not accept frames which do not have either 
	 * our MAC or broadcast MAC */
	DPRINTF("UIP received frame with MAC");
	for (i = 0; i < 6; i++) {
		DPRINTF(":%02x", srcmac[i]);
	}
	if(!compare_ether_addr(srcmac, port->macaddr)
		|| is_broadcast_ether_addr(srcmac)) {
		DPRINTF("  and rejected \n");
		return VMM_EFAIL;
	} else {
		DPRINTF("  and accepted \n");
	}	
	vmm_spin_lock(&s->lock);
	if(s->mbuf != NULL)
		rc = VMM_EFAIL;
	else
		s->mbuf = mbuf;
	vmm_spin_unlock(&s->lock);
	return rc;
}

int uip_netport_read(void)
{
	struct vmm_mbuf *mbuf;
	struct uip_port_state *s = &uip_port_state;

	vmm_spin_lock(&s->lock);
	if(s->mbuf) {
		mbuf = s->mbuf;
		s->mbuf = NULL;
	} else {
		vmm_spin_unlock(&s->lock);
		return 0;
	}
	vmm_spin_unlock(&s->lock);
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
	s->mbuf = NULL;
	INIT_SPIN_LOCK(&s->lock);
	vmm_netport_register(s->port);
	/* Attach with the netswitch */
	vmm_netswitch_port_add(nsw, s->port);
	/* Generate an IP address */
	uip_ipaddr(ipaddr, 192,168,0,0);
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

