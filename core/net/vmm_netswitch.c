/**
 * Copyright (c) 2012 Pranav Sawargaonkar.
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
 * @file vmm_netswitch.c
 * @author Pranav Sawargaonkar <pranav.sawargaonkar@gmail.com>
 * @author Sukanto Ghosh <sukantoghosh@gmail.com>
 * @brief NetSwitch framework.
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_string.h>
#include <vmm_stdio.h>
#include <vmm_modules.h>
#include <vmm_devdrv.h>
#include <vmm_spinlocks.h>
#include <vmm_threads.h>
#include <vmm_completion.h>
#include <list.h>
#include <net/vmm_mbuf.h>
#include <net/vmm_protocol.h>
#include <net/vmm_netswitch.h>
#include <net/vmm_netport.h>

#undef DEBUG

#ifdef DEBUG
#define DPRINTF(fmt, ...) \
	do { vmm_printf(fmt , ## __VA_ARGS__); } while (0)

#define DUMP_NETSWITCH_PKT(mbuf) 					\
do{									\
	char tname[30];							\
	const u8 *srcmac = ether_srcmac(mtod(mbuf, u8 *));		\
	const u8 *dstmac = ether_dstmac(mtod(mbuf, u8 *));		\
									\
	DPRINTF("%s: got pkt with srcaddr[%s]", __func__,		\
			ethaddr_to_str(tname, srcmac));			\
	DPRINTF(", dstaddr[%s]", ethaddr_to_str(tname, dstmac));	\
	DPRINTF(", ethertype: 0x%04X\n", ether_type(mtod(mbuf, u8 *)));	\
	if(ether_type(mtod(mbuf, u8 *)) == 0x0806	/* ARP */) {	\
		DPRINTF("\tARP-HType: 0x%04X\n", 			\
			arp_htype(ether_payload(mtod(mbuf, u8 *))));	\
		DPRINTF("\tARP-PType: 0x%04X\n", 			\
			arp_ptype(ether_payload(mtod(mbuf, u8 *))));	\
		DPRINTF("\tARP-Hlen: 0x%02X\n",  			\
			arp_hlen(ether_payload(mtod(mbuf, u8 *))));	\
		DPRINTF("\tARP-Plen: 0x%02X\n",  			\
			arp_plen(ether_payload(mtod(mbuf, u8 *))));	\
		DPRINTF("\tARP-Oper: 0x%04X\n",  			\
			arp_oper(ether_payload(mtod(mbuf, u8 *))));	\
		DPRINTF("\tARP-SHA: %s\n", ethaddr_to_str(tname, 	\
			arp_sha(ether_payload((mtod(mbuf, u8 *))))));	\
		DPRINTF("\tARP-SPA: %s\n", ip4addr_to_str(tname, 	\
			arp_spa(ether_payload((mtod(mbuf, u8 *))))));	\
		DPRINTF("\tARP-THA: %s\n", ethaddr_to_str(tname, 	\
			arp_tha(ether_payload((mtod(mbuf, u8 *))))));	\
		DPRINTF("\tARP-TPA: %s\n", ip4addr_to_str(tname, 	\
			arp_tpa(ether_payload((mtod(mbuf, u8 *))))));	\
	} else if(ether_type(mtod(mbuf, u8 *)) == 0x0800/* IPv4 */) {	\
		DPRINTF("\tIP-SRC: %s\n", ip4addr_to_str(tname, 	\
			ip_srcaddr(ether_payload((mtod(mbuf, u8 *))))));\
		DPRINTF("\tIP-DST: %s\n", ip4addr_to_str(tname, 	\
			ip_dstaddr(ether_payload((mtod(mbuf, u8 *))))));\
		DPRINTF("\tIP-LEN: %d\n", 				\
			ip_len(ether_payload((mtod(mbuf, u8 *)))));	\
		DPRINTF("\tIP-TTL: %d\n", 				\
			ip_ttl(ether_payload((mtod(mbuf, u8 *)))));	\
		DPRINTF("\tIP-CHKSUM: 0x%04X\n",			\
			ip_chksum(ether_payload((mtod(mbuf, u8 *)))));	\
		DPRINTF("\tIP-PROTOCOL: %d\n",				\
			ip_protocol(ether_payload((mtod(mbuf, u8 *)))));\
	}								\
}while(0)
#else
#define DPRINTF(fmt, ...) do {} while(0)
#define DUMP_NETSWITCH_PKT(mbuf)
#endif

static int vmm_netswitch_rx_handler(void *param)
{
	struct dlist *l;
	struct vmm_netport *src_port;
	struct vmm_mbuf *mbuf;
	struct vmm_netswitch_xfer *xfer;
	struct vmm_netswitch *nsw;
	unsigned long flags;

	nsw = param;

	while(1) {
		vmm_spin_lock_irqsave(&nsw->rx_list_lock, flags);
		while(list_empty(&nsw->rx_list)) {
			vmm_spin_unlock_irqrestore(&nsw->rx_list_lock, flags);
			vmm_completion_wait(&nsw->rx_not_empty);
			vmm_spin_lock_irqsave(&nsw->rx_list_lock, flags);
		}
		l = list_pop(&nsw->rx_list);
		vmm_spin_unlock_irqrestore(&nsw->rx_list_lock, flags);

		xfer = list_entry(l, struct vmm_netswitch_xfer, head);

		mbuf = xfer->mbuf;
		src_port = xfer->src_port;

		/* Return the node back to free list */
		vmm_spin_lock_irqsave(&nsw->free_list_lock, flags);
		list_add_tail(&nsw->free_list, &xfer->head);
		vmm_spin_unlock_irqrestore(&nsw->free_list_lock, flags);

		DUMP_NETSWITCH_PKT(mbuf);

		/* Call the rx function of corresponding switch */
		nsw->port2switch_xfer(src_port, mbuf);
	}

	return VMM_OK;
}

int vmm_netswitch_port2switch(struct vmm_netport *src, struct vmm_mbuf *mbuf)
{
	struct dlist *l;
	struct vmm_netswitch_xfer *xfer;
	unsigned long flags;
	struct vmm_netswitch *nsw;

	if(!src || !src->nsw) {
		return VMM_EFAIL;
	}
	nsw = src->nsw;
	/* Get a free vmm_netswitch_xfer instance from free_list */
	vmm_spin_lock_irqsave(&nsw->free_list_lock, flags);
	if(list_empty(&nsw->free_list)) {
		vmm_spin_unlock_irqrestore(&nsw->free_list_lock,
					   flags);
		m_freem(mbuf);
		return VMM_EFAIL;
	}
	l = list_pop(&nsw->free_list);
	vmm_spin_unlock_irqrestore(&nsw->free_list_lock, flags);

	xfer = list_entry(l, struct vmm_netswitch_xfer, head);

	/* Fillup the vmm_netswitch_xfer */
	xfer->src_port = src;
	xfer->mbuf = mbuf;

	/* Add this new xfer to the rx_list */
	vmm_spin_lock_irqsave(&nsw->rx_list_lock, flags);
	list_add_tail(&nsw->rx_list, &xfer->head);
	vmm_spin_unlock_irqrestore(&nsw->rx_list_lock, flags);

	vmm_completion_complete(&nsw->rx_not_empty);

	return VMM_OK;
}

struct vmm_netswitch *vmm_netswitch_alloc(char *name, u16 rxq_size,
					  u8 prio, u64 tslice)
{
	int i;
	struct dlist *tmp_node;
	struct vmm_netswitch *nsw;

	nsw = vmm_malloc(sizeof(struct vmm_netswitch));

	if (!nsw) {
		vmm_printf("%s Failed to allocate net switch\n", __func__);
		goto vmm_netswitch_alloc_failed;
	}

	vmm_memset(nsw, 0, sizeof(struct vmm_netswitch));
	nsw->name = vmm_malloc(vmm_strlen(name)+1);
	if (!nsw->name) {
		vmm_printf("%s Failed to allocate for net switch\n", __func__);
		goto vmm_netswitch_alloc_failed;
	}
	vmm_strcpy(nsw->name, name);

	nsw->xfer_pool = vmm_malloc(sizeof(struct vmm_netswitch_xfer) * rxq_size);
	if (!nsw->xfer_pool) {
		vmm_printf("%s Failed to allocate for net switch\n", __func__);
		goto vmm_netswitch_alloc_failed;
	}

	nsw->thread = vmm_threads_create(nsw->name, vmm_netswitch_rx_handler,
					 nsw, prio, tslice);
	if (!nsw->thread) {
		vmm_printf("%s Failed to create thread for net switch\n",
			   __func__);
		goto vmm_netswitch_alloc_failed;
	}

	INIT_COMPLETION(&nsw->rx_not_empty);
	INIT_SPIN_LOCK(&nsw->free_list_lock);
	INIT_LIST_HEAD(&nsw->free_list);
	INIT_SPIN_LOCK(&nsw->rx_list_lock);
	INIT_LIST_HEAD(&nsw->rx_list);
	INIT_LIST_HEAD(&nsw->port_list);

	/* Fill the free_list of vmm_netswitch_xfer */
	for(i = 0; i < rxq_size; i++) {
		tmp_node = &((nsw->xfer_pool + i)->head);
		list_add_tail(&nsw->free_list, tmp_node);
	}

	goto vmm_netswitch_alloc_done;

vmm_netswitch_alloc_failed:
	if(nsw) {
		if(nsw->name) {
			vmm_free(nsw->name);
		}
		if(nsw->xfer_pool) {
			vmm_free(nsw->xfer_pool);
		}
		if(nsw->thread) {
			vmm_threads_destroy(nsw->thread);
		}
		vmm_free(nsw);
		nsw = NULL;
	}
vmm_netswitch_alloc_done:
	return nsw;
}

void vmm_netswitch_free(struct vmm_netswitch *nsw)
{
	if(nsw) {
		if(nsw->name) {
			vmm_free(nsw->name);
		}
		if(nsw->xfer_pool) {
			vmm_free(nsw->xfer_pool);
		}
		if(nsw->thread) {
			vmm_threads_destroy(nsw->thread);
		}
		vmm_free(nsw);
	}
}

int vmm_netswitch_register(struct vmm_netswitch *nsw, struct vmm_device *dev,
			   void *priv)
{
	struct vmm_classdev *cd;
	int rc;

	if (nsw == NULL)
		return VMM_EFAIL;

	cd = vmm_malloc(sizeof(struct vmm_classdev));
	if (!cd) {
		rc = VMM_EFAIL;
		goto ret;
	}

	INIT_LIST_HEAD(&cd->head);
	vmm_strcpy(cd->name, nsw->name);
	cd->dev = nsw->dev;
	cd->priv = nsw;

	rc = vmm_devdrv_register_classdev(VMM_NETSWITCH_CLASS_NAME, cd);
	if (rc != VMM_OK) {
		vmm_printf("%s: Failed to class register network switch %s "
			   "with err 0x%x\n", __func__, nsw->name, rc);
		goto fail_nsw_reg;
	}

	nsw->dev = dev;
	nsw->priv = priv;

	vmm_threads_start(nsw->thread);

#ifdef CONFIG_VERBOSE_MODE
	vmm_printf("Successfully registered VMM net switch: %s\n", nsw->name);
#endif

	return rc;

fail_nsw_reg:
	cd->dev = NULL;
	cd->priv = NULL;
	vmm_free(cd);
ret:
	return rc;
}

int vmm_netswitch_unregister(struct vmm_netswitch *nsw)
{
	int rc;
	struct dlist *list;
	struct vmm_netport *port;
	struct vmm_classdev *cd;

	if (nsw == NULL)
		return VMM_EFAIL;

	cd = vmm_devdrv_find_classdev(VMM_NETSWITCH_CLASS_NAME, nsw->name);
	if (!cd)
		return VMM_EFAIL;

	vmm_threads_stop(nsw->thread);

	/* Remove any ports still attached to this nsw */
	list_for_each(list, &nsw->port_list) {
		port = list_port(list);
		vmm_netswitch_port_remove(port);
	}

	rc = vmm_devdrv_unregister_classdev(VMM_NETSWITCH_CLASS_NAME, cd);

	if (!rc)
		vmm_free(cd);

	return rc;
}

int vmm_netswitch_port_add(struct vmm_netswitch *nsw,
			   struct vmm_netport *port)
{
	int rc = VMM_OK;

	if(!nsw || !port) {
		return VMM_EFAIL;
	}
	/* If port has invalid mac, assign a random one */
	if(!is_valid_ether_addr(port->macaddr)) {
		random_ether_addr(port->macaddr);
	}
	/* Add the port to the port_list */
	list_add_tail(&nsw->port_list, &port->head);
	port->nsw = nsw;
	/* Call the netswitch's port_add callback */
	if(nsw->port_add) {
		rc = nsw->port_add(nsw, port);
	}
	if(rc == VMM_OK) {
		/* Notify the port about the link-status change */
		port->flags |= VMM_NETPORT_LINK_UP;
		port->link_changed(port);
	}
#ifdef CONFIG_VERBOSE_MODE
	{
		char tname[30];
		vmm_printf("NET: Port(\"%s\") added to Switch(\"%s\"), " \
			   "MAC[%s]\n", port->name, nsw->name,
			   ethaddr_to_str(tname, port->macaddr));
	}
#endif
	return rc;
}

int vmm_netswitch_port_remove(struct vmm_netport *port)
{
	if(!port || !(port->nsw)) {
		return VMM_EFAIL;
	}
#ifdef CONFIG_VERBOSE_MODE
	vmm_printf("NET: Port(\"%s\") removed from Switch(\"%s\")\n",
			port->name, port->nsw->name);
#endif
	/* Notify the port about the link-status change */
	port->flags &= ~VMM_NETPORT_LINK_UP;
	port->link_changed(port);
	/* Call the netswitch's port_remove handler */
	if(port->nsw->port_remove) {
		port->nsw->port_remove(port);
	}
	/* Remove the port from port_list */
	port->nsw = NULL;
	list_del(&port->head);

	return VMM_OK;
}

struct vmm_netswitch *vmm_netswitch_find(const char *name)
{
	struct vmm_classdev *cd;

	cd = vmm_devdrv_find_classdev(VMM_NETSWITCH_CLASS_NAME, name);

	if (!cd)
		return NULL;

	return cd->priv;
}

struct vmm_netswitch *vmm_netswitch_get(int num)
{
	struct vmm_classdev *cd;

	cd = vmm_devdrv_classdev(VMM_NETSWITCH_CLASS_NAME, num);

	if (!cd)
		return NULL;

	return cd->priv;
}

u32 vmm_netswitch_count(void)
{
	return vmm_devdrv_classdev_count(VMM_NETSWITCH_CLASS_NAME);
}

int vmm_netswitch_init(void)
{
	int rc;
	struct vmm_class *c;

	vmm_printf("Initialize Network Switch Framework\n");

	c = vmm_malloc(sizeof(struct vmm_class));
	if (!c)
		return VMM_EFAIL;

	INIT_LIST_HEAD(&c->head);
	vmm_strcpy(c->name, VMM_NETSWITCH_CLASS_NAME);
	INIT_LIST_HEAD(&c->classdev_list);

	rc = vmm_devdrv_register_class(c);
	if (rc) {
		vmm_printf("Failed to register %s class\n",
			VMM_NETSWITCH_CLASS_NAME);
		vmm_free(c);
		return rc;
	}

	return VMM_OK;
}

void vmm_netswitch_exit(void)
{
	int rc;
	struct vmm_class *c;

	c = vmm_devdrv_find_class(VMM_NETSWITCH_CLASS_NAME);
	if (!c)
		return;

	rc = vmm_devdrv_unregister_class(c);
	if (rc) {
		vmm_printf("Failed to unregister %s class",
			VMM_NETSWITCH_CLASS_NAME);
		return;
	}

	vmm_free(c);

	return;
}
