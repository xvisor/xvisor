/**
 * Copyright (c) 2013 Anup Patel.
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
 * @file netstack.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief netstack APIs using lwIP library
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_stdio.h>
#include <vmm_timer.h>
#include <vmm_devtree.h>
#include <vmm_modules.h>
#include <net/vmm_mbuf.h>
#include <net/vmm_net.h>
#include <net/vmm_netport.h>
#include <net/vmm_netswitch.h>
#include <libs/netstack.h>

#include "lwip/opt.h"
#include "lwip/debug.h"
#include "lwip/def.h"
#include "lwip/mem.h"
#include "lwip/pbuf.h"
#include "lwip/stats.h"
#include "lwip/raw.h"
#include "lwip/icmp.h"
#include "lwip/tcpip.h"
#include "lwip/netif.h"
#include "lwip/ip_addr.h"
#include "lwip/sockets.h"
#include "lwip/inet.h"
#include "lwip/inet_chksum.h"

#include "netif/etharp.h"

#define MODULE_DESC			"lwIP Network Stack"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(VMM_NET_CLASS_IPRIORITY + 1)
#define	MODULE_INIT			lwip_netstack_init
#define	MODULE_EXIT			lwip_netstack_exit

#define IFNAME0 'e'
#define IFNAME1 'n'

#define MAX_FRAME_LEN   1518

#undef PING_USE_SOCKETS

/** ping receive timeout - in milliseconds */
#ifndef PING_RCV_TIMEO
#define PING_RCV_TIMEO 1000
#endif

/** ping delay - in milliseconds */
#ifndef PING_DELAY
#define PING_DELAY     1000
#endif

/** ping identifier - must fit on a u16_t */
#ifndef PING_ID
#define PING_ID        0xAFAF
#endif

struct lwip_netstack {
	struct netif nif;
	ip_addr_t ipaddr, netmask, gw;
	struct vmm_netport *port;
#if !defined(PING_USE_SOCKETS)
	struct vmm_mutex ping_lock;
	ip_addr_t ping_addr;
	u16 ping_seq_num;
	u64 ping_tstamp;
	bool ping_timedout;
	struct raw_pcb *ping_pcb;
	struct netstack_echo_reply *ping_reply;
	struct vmm_completion ping_done;
#endif
};

static struct lwip_netstack lns;

char *netstack_get_name(void)
{
	return "lwIP";
}
VMM_EXPORT_SYMBOL(netstack_get_name);

int netstack_set_ipaddr(u8 *addr)
{
	IP4_ADDR(&lns.ipaddr, addr[0],addr[1],addr[2],addr[3]);
	netif_set_ipaddr(&lns.nif, &lns.ipaddr);
	return VMM_OK;
}
VMM_EXPORT_SYMBOL(netstack_set_ipaddr);

int netstack_get_ipaddr(u8 *addr)
{
	addr[0] = ip4_addr1(&lns.ipaddr);
	addr[1] = ip4_addr2(&lns.ipaddr);
	addr[2] = ip4_addr3(&lns.ipaddr);
	addr[3] = ip4_addr4(&lns.ipaddr);
	return VMM_OK;
}
VMM_EXPORT_SYMBOL(netstack_get_ipaddr);

int netstack_set_ipmask(u8 *addr)
{
	IP4_ADDR(&lns.netmask, addr[0],addr[1],addr[2],addr[3]);
	netif_set_netmask(&lns.nif, &lns.netmask);
	return VMM_OK;
}
VMM_EXPORT_SYMBOL(netstack_set_ipmask);

int netstack_get_ipmask(u8 *addr)
{
	addr[0] = ip4_addr1(&lns.netmask);
	addr[1] = ip4_addr2(&lns.netmask);
	addr[2] = ip4_addr3(&lns.netmask);
	addr[3] = ip4_addr4(&lns.netmask);
	return VMM_OK;
}
VMM_EXPORT_SYMBOL(netstack_get_ipmask);

int netstack_get_hwaddr(u8 *addr)
{
	memcpy(addr, &lns.port->macaddr, 6);
	return VMM_OK;
}
VMM_EXPORT_SYMBOL(netstack_get_hwaddr);

#if defined(PING_USE_SOCKETS)

int netstack_send_echo(u8 *ripaddr, u16 size, u16 seqno, 
			struct netstack_echo_reply *reply)
{
	u64 ts;
	int s, i, err;
	char buf[64];
	size_t fromlen, off, len = sizeof(struct icmp_echo_hdr) + size;
	ip_addr_t to_addr, from_addr;
	struct sockaddr_in sock;
	struct ip_hdr *iphdr;
	struct icmp_echo_hdr *iecho;

	LWIP_ASSERT("ping_size is too big\n", len <= 0xffff);

	/* Prepare target address */
	IP4_ADDR(&to_addr, ripaddr[0],ripaddr[1],ripaddr[2],ripaddr[3]);

	/* Open RAW socket */
	if ((s = lwip_socket(AF_INET, SOCK_RAW, IP_PROTO_ICMP)) < 0) {
		vmm_printf("%s: failed to open ICMP socket\n", __func__);
		return VMM_EFAIL;
	}

	/* Set socket option */
	i = PING_RCV_TIMEO;
	lwip_setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &i, sizeof(i));

	/* Prepare socket address */
	sock.sin_len = sizeof(sock);
	sock.sin_family = AF_INET;
	inet_addr_from_ipaddr(&sock.sin_addr, &to_addr);

	/* Prepare ECHO request */
	iecho = (struct icmp_echo_hdr *)vmm_zalloc(len);
	if (!iecho) {
		return VMM_ENOMEM;
	}
	ICMPH_TYPE_SET(iecho, ICMP_ECHO);
	ICMPH_CODE_SET(iecho, 0);
	iecho->chksum = 0;
	iecho->id     = PING_ID;
	iecho->seqno  = htons(seqno);
	for (i = 0; i < size; i++) {
		((char*)iecho)[sizeof(struct icmp_echo_hdr) + i] = (char)i;
	}
	iecho->chksum = inet_chksum(iecho, len);

	/* Send ECHO request */
	err = lwip_sendto(s, iecho, len, 0, 
				(struct sockaddr*)&sock, sizeof(sock));
	vmm_free(iecho);
	if (!err) {
		return VMM_EFAIL;
	}

	/* Get reference timestamp */
	ts = vmm_timer_timestamp();

	/* Wait for ECHO reply */
	err = VMM_EFAIL;
	off = lwip_recvfrom(s, buf, sizeof(buf), 0, 
			    (struct sockaddr*)&sock, (socklen_t*)&fromlen);
	if (off >= (sizeof(struct ip_hdr) + sizeof(struct icmp_echo_hdr))) {
		inet_addr_to_ipaddr(&from_addr, &sock.sin_addr);
		iphdr = (struct ip_hdr *)buf;
		iecho = (struct icmp_echo_hdr *)(buf + (IPH_HL(iphdr) * 4));
		if ((iecho->id == PING_ID) && 
		    (iecho->seqno == htons(seqno))) {
			reply->ripaddr[0] = ip4_addr1(&from_addr);
			reply->ripaddr[1] = ip4_addr2(&from_addr);
			reply->ripaddr[2] = ip4_addr3(&from_addr);
			reply->ripaddr[3] = ip4_addr4(&from_addr);
			reply->ttl = IPH_TTL(iphdr);
			reply->len = len;
			reply->seqno = seqno;
			reply->rtt = 
				udiv64(vmm_timer_timestamp() - ts, 1000);
			err = VMM_OK;
		}
	}
	while (off < len) {
		off = lwip_recvfrom(s, buf, sizeof(buf), 0, 
			(struct sockaddr*)&sock, (socklen_t*)&fromlen);
	}

	/* Close RAW socket */
	lwip_close(s);

	return err;
}
VMM_EXPORT_SYMBOL(netstack_send_echo);

#else

static u8_t ping_recv(void *arg, struct raw_pcb *pcb, 
			struct pbuf *p, ip_addr_t *addr)
{
	struct ip_hdr *iphdr;
	struct icmp_echo_hdr *iecho;
	LWIP_UNUSED_ARG(arg);
	LWIP_UNUSED_ARG(pcb);
	LWIP_UNUSED_ARG(addr);

	LWIP_ASSERT("p != NULL", p != NULL);

	if ((p->tot_len >= (PBUF_IP_HLEN + sizeof(struct icmp_echo_hdr)))) {
		iphdr = (struct ip_hdr *)p->payload;
		iecho = (struct icmp_echo_hdr *)(p->payload + (IPH_HL(iphdr) * 4));
		if ((lns.ping_reply != NULL) &&
		    (iecho->id == PING_ID) && 
		    (iecho->seqno == htons(lns.ping_seq_num))) {
			lns.ping_reply->ripaddr[0] = ip4_addr1(&lns.ping_addr);
			lns.ping_reply->ripaddr[1] = ip4_addr2(&lns.ping_addr);
			lns.ping_reply->ripaddr[2] = ip4_addr3(&lns.ping_addr);
			lns.ping_reply->ripaddr[3] = ip4_addr4(&lns.ping_addr);
			lns.ping_reply->ttl = IPH_TTL(iphdr);
			lns.ping_reply->len = p->tot_len - (IPH_HL(iphdr) * 4);
			lns.ping_reply->seqno = lns.ping_seq_num;
			lns.ping_reply->rtt = 
				udiv64(vmm_timer_timestamp() - lns.ping_tstamp, 1000);

			lns.ping_timedout = FALSE;
			lns.ping_reply = NULL;
			vmm_completion_complete(&lns.ping_done);
		}
	}

	/* Don't eat the packet to let lwIP process it. */
	return 0;
}

static void ping_timeout(void *arg)
{
	struct raw_pcb *pcb = (struct raw_pcb*)arg;
  
	LWIP_ASSERT("ping_timeout: no pcb given!", pcb != NULL);

	if (lns.ping_reply != NULL) {
		lns.ping_timedout = TRUE;
		lns.ping_reply = NULL;
		vmm_completion_complete(&lns.ping_done);
	}
}

static void ping_raw_init(void)
{
	INIT_MUTEX(&lns.ping_lock);

	lns.ping_seq_num = 0;
	lns.ping_tstamp = 0;
	lns.ping_timedout = FALSE;

	lns.ping_pcb = raw_new(IP_PROTO_ICMP);
	LWIP_ASSERT("ping_pcb != NULL", lns.ping_pcb != NULL);
	raw_recv(lns.ping_pcb, ping_recv, NULL);
	raw_bind(lns.ping_pcb, IP_ADDR_ANY);
	sys_timeout(PING_DELAY, ping_timeout, lns.ping_pcb);

	lns.ping_reply = NULL;

	INIT_COMPLETION(&lns.ping_done);
}

int netstack_send_echo(u8 *ripaddr, u16 size, u16 seqno, 
			struct netstack_echo_reply *reply)
{
	int i, rc;
	struct pbuf *p;
	struct icmp_echo_hdr *iecho;
	size_t len = sizeof(struct icmp_echo_hdr) + size;

	LWIP_ASSERT("ping_size <= 0xffff", len <= 0xffff);

	/* Lock ping context for atomicity */
	vmm_mutex_lock(&lns.ping_lock);

	/* Alloc ping pbuf */
	p = pbuf_alloc(PBUF_IP, (u16_t)len, PBUF_RAM);
	if (!p) {
		vmm_mutex_unlock(&lns.ping_lock);
		return VMM_ENOMEM;
	}
	if ((p->len != p->tot_len) || (p->next != NULL)) {
		pbuf_free(p);
		vmm_mutex_unlock(&lns.ping_lock);
		return VMM_EFAIL;
	}

	/* Prepare ECHO request */
	iecho = (struct icmp_echo_hdr *)p->payload;
	ICMPH_TYPE_SET(iecho, ICMP_ECHO);
	ICMPH_CODE_SET(iecho, 0);
	iecho->chksum = 0;
	iecho->id     = PING_ID;
	iecho->seqno  = htons(seqno);
	for (i = 0; i < size; i++) {
		((char*)iecho)[sizeof(struct icmp_echo_hdr) + i] = (char)i;
	}
	iecho->chksum = inet_chksum(iecho, len);

	/* Prepare target address */
	IP4_ADDR(&lns.ping_addr, ripaddr[0],ripaddr[1],ripaddr[2],ripaddr[3]);

	/* Save ping info */
	lns.ping_seq_num = seqno;
	lns.ping_tstamp = vmm_timer_timestamp();
	lns.ping_timedout = FALSE;
	lns.ping_reply = reply;

	/* Send ping packet */
	raw_sendto(lns.ping_pcb, p, &lns.ping_addr);

	/* Waif for ping to complete */
	vmm_completion_wait(&lns.ping_done);

	/* Free ping pbuf */
	pbuf_free(p);

	/* Check ping timeout flag */
	if (lns.ping_timedout) {
		rc = VMM_ETIMEDOUT;
	} else {
		rc = VMM_OK;
	}

	/* Unloack ping context */
	vmm_mutex_unlock(&lns.ping_lock);

	return rc;
}
VMM_EXPORT_SYMBOL(netstack_send_echo);

#endif

void netstack_prefetch_arp_mapping(u8 *ipaddr)
{
	/* Nothing to do here. */
	/* lwIP does this automatically */
}
VMM_EXPORT_SYMBOL(netstack_prefetch_arp_mapping);

struct netstack_socket *netstack_socket_alloc(enum netstack_socket_type type)
{
	struct netstack_socket *sk;
	struct netconn *conn;

	sk = vmm_zalloc(sizeof(struct netstack_socket));
	if (!sk) {
		return NULL;
	}

	switch (type) {
	case NETSTACK_SOCKET_TCP:
		conn = netconn_new(NETCONN_TCP);
		break;
	case NETSTACK_SOCKET_UDP:
		conn = netconn_new(NETCONN_UDP);
		break;
	default:
		conn = NULL;
		break;
	};
	if (!conn) {
		vmm_free(sk);
		return NULL;
	}

	sk->priv = conn;

	return sk;
}
VMM_EXPORT_SYMBOL(netstack_socket_alloc);

void netstack_socket_free(struct netstack_socket *sk)
{
	if (!sk || !sk->priv) {
		return;
	}

	netconn_delete(sk->priv);
	vmm_free(sk);	
}
VMM_EXPORT_SYMBOL(netstack_socket_free);

int netstack_socket_connect(struct netstack_socket *sk, u8 *ipaddr, u16 port)
{
	err_t err;
	ip_addr_t addr;

	if (!sk || !sk->priv || !ipaddr) {
		return VMM_EINVALID;
	}

	IP4_ADDR(&addr, ipaddr[0],ipaddr[1],ipaddr[2],ipaddr[3]);
	err = netconn_connect(sk->priv, &addr, port);
	if (err != ERR_OK) {
		return VMM_EFAIL;
	}

	sk->ipaddr[0] = ipaddr[0];
	sk->ipaddr[1] = ipaddr[1];
	sk->ipaddr[2] = ipaddr[2];
	sk->ipaddr[3] = ipaddr[3];
	sk->port = port;

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(netstack_socket_connect);

int netstack_socket_disconnect(struct netstack_socket *sk)
{
	err_t err;

	if (!sk || !sk->priv) {
		return VMM_EINVALID;
	}

	err = netconn_disconnect(sk->priv);
	if (err != ERR_OK) {
		return VMM_EFAIL;
	}

	sk->ipaddr[0] = 0;
	sk->ipaddr[1] = 0;
	sk->ipaddr[2] = 0;
	sk->ipaddr[3] = 0;
	sk->port = 0;

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(netstack_socket_disconnect);

int netstack_socket_bind(struct netstack_socket *sk, u8 *ipaddr, u16 port)
{
	err_t err;
	ip_addr_t addr;

	if (!sk || !sk->priv) {
		return VMM_EINVALID;
	}

	if (!ipaddr) {
		err = netconn_bind(sk->priv, NULL, port);
	} else {
		IP4_ADDR(&addr, ipaddr[0],ipaddr[1],ipaddr[2],ipaddr[3]);
		err = netconn_bind(sk->priv, &addr, port);
	}

	if (err != ERR_OK) {
		return VMM_EFAIL;
	}

	if (!ipaddr) {
		sk->ipaddr[0] = 0;
		sk->ipaddr[1] = 0;
		sk->ipaddr[2] = 0;
		sk->ipaddr[3] = 0;
	} else {
		sk->ipaddr[0] = ipaddr[0];
		sk->ipaddr[1] = ipaddr[1];
		sk->ipaddr[2] = ipaddr[2];
		sk->ipaddr[3] = ipaddr[3];
	}
	sk->port = port;

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(netstack_socket_bind);

int netstack_socket_listen(struct netstack_socket *sk)
{
	err_t err;

	if (!sk || !sk->priv) {
		return VMM_EINVALID;
	}

	err = netconn_listen(sk->priv);
	if (err != ERR_OK) {
		return VMM_EFAIL;
	}

	return VMM_OK;	
}
VMM_EXPORT_SYMBOL(netstack_socket_listen);

int netstack_socket_accept(struct netstack_socket *sk, 
			   struct netstack_socket **new_sk)
{
	err_t err;
	struct netconn *newconn;
	struct netstack_socket *tsk;

	if (!sk || !sk->priv || !new_sk) {
		return VMM_EINVALID;
	}

	tsk = vmm_zalloc(sizeof(struct netstack_socket));
	if (!tsk) {
		return VMM_ENOMEM;
	}
	
	memcpy(tsk, sk, sizeof(struct netstack_socket));

	tsk->priv = NULL;

	err = netconn_accept(sk->priv, &newconn);
	if (err != ERR_OK) {
		vmm_free(tsk);
		return VMM_EFAIL;
	}

	tsk->priv = newconn;

	*new_sk = tsk;

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(netstack_socket_accept);

int netstack_socket_close(struct netstack_socket *sk)
{
	err_t err;

	if (!sk || !sk->priv) {
		return VMM_EINVALID;
	}

	err = netconn_close(sk->priv);
	if (err != ERR_OK) {
		return VMM_EFAIL;
	}

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(netstack_socket_close);

int netstack_socket_recv(struct netstack_socket *sk, 
			   struct netstack_socket_buf *buf)
{
	err_t err;
	struct netbuf *nb;

	if (!sk || !sk->priv || !buf) {
		return VMM_EINVALID;
	}

	err = netconn_recv(sk->priv, &nb);
	if (err != ERR_OK) {
		return VMM_EFAIL;
	}

	netbuf_data(nb, &buf->data, &buf->len);
	buf->priv = nb;

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(netstack_socket_recv);

int netstack_socket_nextbuf(struct netstack_socket_buf *buf)
{
	s8_t err;
	struct netbuf *nb;

	if (!buf || !buf->priv) {
		return VMM_EINVALID;
	}

	nb = buf->priv;

	err = netbuf_next(nb);
	if (err != 0 && err != 1) {
		return VMM_ENOENT;
	}

	netbuf_data(nb, &buf->data, &buf->len);
	buf->priv = nb;

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(netstack_socket_nextbuf);

void netstack_socket_freebuf(struct netstack_socket_buf *buf)
{
	if (!buf || !buf->priv) {
		return;
	}

	buf->data = NULL;
	buf->len = 0;
	netbuf_delete(buf->priv);
}
VMM_EXPORT_SYMBOL(netstack_socket_freebuf);

int netstack_socket_write(struct netstack_socket *sk, void *data, u16 len)
{
	err_t err;

	if (!sk || !sk->priv || !data) {
		return VMM_EINVALID;
	}

	err = netconn_write(sk->priv, data, len, NETCONN_COPY);
	if (err != ERR_OK) {
		return VMM_EFAIL;
	}

	return VMM_OK;	
}
VMM_EXPORT_SYMBOL(netstack_socket_write);

static void lwip_set_link(struct vmm_netport *port)
{
	struct lwip_netstack *lns = port->priv;

	if (port->flags & VMM_NETPORT_LINK_UP) {
		netif_set_up(&lns->nif);
	} else {
		netif_set_down(&lns->nif);
	}
}

static int lwip_can_receive(struct vmm_netport *port)
{
	if (port->flags & VMM_NETPORT_LINK_UP) {
		return TRUE;
	}
	return FALSE;
}

static int lwip_switch2port_xfer(struct vmm_netport *port,
			 	 struct vmm_mbuf *mbuf)
{
	u32 pbuf_len;
	struct eth_hdr *ethhdr;
	struct pbuf *p;
	struct lwip_netstack *lns = port->priv;

	/* Move received packet into a new pbuf */
	pbuf_len = min(MAX_FRAME_LEN, mbuf->m_pktlen);
	p = pbuf_alloc(PBUF_LINK, pbuf_len, PBUF_POOL);
	if (!p) {
		return VMM_ENOMEM;
	}
	m_copydata(mbuf, 0, pbuf_len, p->payload);

	/* Points to packet ethernet header */
	ethhdr = p->payload;

	/* IP or ARP packet? */
	switch (htons(ethhdr->type)) {
	case ETHTYPE_IP:
	case ETHTYPE_ARP:
		if (lns->nif.input(p, &lns->nif) != ERR_OK) {
			pbuf_free(p);
			p = NULL;
		}
		break;
	default:
		pbuf_free(p);
		p = NULL;
		break;
	}

	/* Free the mbuf */
	m_freem(mbuf);

	/* Return success */
	return VMM_OK;
}

static err_t lwip_netstack_output(struct netif *netif, struct pbuf *p)
{
	void *tbuf;
	struct vmm_mbuf *mbuf;
	struct pbuf *q;
	struct lwip_netstack *lns = netif->state;

	if (p->tot_len > MAX_FRAME_LEN) {
		/* Frame too long, drop it */
		return ERR_MEM;
	}

	/* Create a mbuf */
	MGETHDR(mbuf, 0, 0);

	/* Add payload from pbuf to mbuf */
	q = p;
	while (q != NULL) {
		tbuf = vmm_zalloc(q->len);
		if (!tbuf) {
			return ERR_MEM;
		}
		memcpy(tbuf, q->payload, q->len);
		MEXTADD(mbuf, tbuf, q->len, NULL, NULL);
		q = q->next;
	}

	/* Setup mbuf len */
	mbuf->m_len = mbuf->m_pktlen = p->tot_len;

	/* Send mbuf to the netswitch */
	vmm_port2switch_xfer(lns->port, mbuf);

	/* Return success */
	return ERR_OK;
}

static err_t lwip_netstack_netif_init(struct netif *netif)
{
	struct lwip_netstack *lns = netif->state;

	netif->name[0] = IFNAME0;
	netif->name[1] = IFNAME1;
	/* We directly use etharp_output() here to save a function call.
	 * You can instead declare your own function an call etharp_output()
	 * from it if you have to do some checks before sending (e.g. if link
	 * is available...)
	 */
	netif->output = etharp_output;
	netif->linkoutput = lwip_netstack_output;
	netif->mtu = 1500;
	netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;
	netif->hwaddr_len = ETHARP_HWADDR_LEN;
	memcpy(netif->hwaddr, lns->port->macaddr, ETHARP_HWADDR_LEN);

	return 0;
}

static int __init lwip_netstack_init(void)
{
	int rc;
	struct vmm_netswitch *nsw;
	struct vmm_devtree_node *node;
	const char *attrval;
	u8 ip[] = {192, 168, 0, 1};
	u8 mask[] = {255, 255, 255, 0};

	/* Clear lwIP state */
	memset(&lns, 0, sizeof(lns));

	/* Get netstack device tree node if available */
	node = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPARATOR_STRING
				   VMM_DEVTREE_VMMINFO_NODE_NAME
				   VMM_DEVTREE_PATH_SEPARATOR_STRING
				   VMM_DEVTREE_VMMNET_NODE_NAME
				   VMM_DEVTREE_PATH_SEPARATOR_STRING
				   VMM_DEVTREE_NETSTACK_NODE_NAME);

	/* Retrive preferred IP address */
	attrval = vmm_devtree_attrval(node, "ipaddr");
	if (attrval) {
		/* Read ip address from netstack node */
		str2ipaddr(ip, attrval);
	}

	/* Retrive preferred IP address */
	attrval = vmm_devtree_attrval(node, "netmask");
	if (attrval) {
		/* Read network mask from netstack node */
		str2ipaddr(mask, attrval);
	}

	/* Retrive preferred netswitch */
	attrval = vmm_devtree_attrval(node, "netswitch");
	if (attrval) {
		/* Find netswitch with given name */
		nsw = vmm_netswitch_find(attrval);
	} else {
		/* Get the first netswitch */
		nsw = vmm_netswitch_get(0);
	}
	if (!nsw) {
		vmm_panic("No netswitch found\n");
	}

	/* Allocate a netport */
	lns.port = vmm_netport_alloc("lwip-netport", VMM_NETPORT_DEF_QUEUE_SIZE);
	if (!lns.port) {
		vmm_printf("lwIP netport_alloc() failed\n");
		rc = VMM_EFAIL;
		goto fail;
	}

	/* Setup a netport */
	lns.port->mtu = 1500;
	lns.port->link_changed = lwip_set_link;
	lns.port->can_receive = lwip_can_receive;
	lns.port->switch2port_xfer = lwip_switch2port_xfer;
	lns.port->priv = &lns;

	/* Register a netport */
	rc = vmm_netport_register(lns.port);
	if (rc) {
		goto fail1;
	}

	/* Initialize lwIP + TCP/IP APIs */
	tcpip_init(NULL, NULL);

	/* Add netif */
	IP4_ADDR(&lns.ipaddr, ip[0],ip[1],ip[2],ip[3]);
	IP4_ADDR(&lns.netmask, mask[0],mask[1],mask[2],mask[3]);
	IP4_ADDR(&lns.gw, ip[0],ip[1],ip[2],ip[3]);
	netif_add(&lns.nif, &lns.ipaddr, &lns.netmask, &lns.gw, &lns, 
		  lwip_netstack_netif_init, ethernet_input);

	/* Set default netif */
	netif_set_default(&lns.nif);

	/* Attach netport with netswitch 
	 * Note: This will cause netport link_change()
	 */
	rc = vmm_netswitch_port_add(nsw, lns.port);
	if (rc) {
		goto fail2;
	}

#if !defined(PING_USE_SOCKETS)
	/* Initalize RAW PCB for ping */
	ping_raw_init();
#endif

	return VMM_OK;

fail2:
	vmm_netport_unregister(lns.port);
fail1:
	vmm_netport_free(lns.port);
fail:
	return rc;
}

static void __exit lwip_netstack_exit(void)
{
	/* FIXME: */
}

VMM_DECLARE_MODULE(MODULE_DESC, 
			MODULE_AUTHOR, 
			MODULE_LICENSE, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);

