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
#include <vmm_stdio.h>
#include <vmm_modules.h>
#include <vmm_devdrv.h>
#include <vmm_spinlocks.h>
#include <vmm_scheduler.h>
#include <vmm_threads.h>
#include <vmm_completion.h>
#include <net/vmm_mbuf.h>
#include <net/vmm_protocol.h>
#include <net/vmm_netswitch.h>
#include <net/vmm_netport.h>
#include <libs/list.h>
#include <libs/stringlib.h>

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
	irq_flags_t flags;
	struct vmm_netport_xfer *xfer;
	struct vmm_netswitch *nsw;

	nsw = param;

	while (1) {
		/* Try to wait for xfer request */
		vmm_completion_wait(&nsw->rx_not_empty);

		/* Try to get xfer request from rx_list */
		vmm_spin_lock_irqsave(&nsw->rx_list_lock, flags);
		if (list_empty(&nsw->rx_list)) {
			vmm_spin_unlock_irqrestore(&nsw->rx_list_lock, flags);
			continue;
		}
		l = list_pop(&nsw->rx_list);
		nsw->rx_count--;
		vmm_spin_unlock_irqrestore(&nsw->rx_list_lock, flags);

		/* Extract info from xfer request */
		xfer = list_entry(l, struct vmm_netport_xfer, head);

		DUMP_NETSWITCH_PKT(xfer->mbuf);

		/* Call the rx function of net switch */
		vmm_mutex_lock(&nsw->lock);
		nsw->port2switch_xfer(xfer->port, xfer->mbuf);
		vmm_mutex_unlock(&nsw->lock);

		/* Free netport xfer request */
		vmm_netport_free_xfer(xfer->port, xfer);
	}

	return VMM_OK;
}

int vmm_netswitch_port2switch(struct vmm_netport *src, struct vmm_mbuf *mbuf)
{
	irq_flags_t flags;
	struct vmm_netport_xfer *xfer;
	struct vmm_netswitch *nsw;

	if (!mbuf) {
		return VMM_EFAIL;
	}
	if (!src || !src->nsw) {
		m_freem(mbuf);
		return VMM_EFAIL;
	}
	nsw = src->nsw;

	/* Alloc netport xfer request */
	xfer = vmm_netport_alloc_xfer(src);
	if (!xfer) {
		m_freem(mbuf);
		return VMM_EFAIL;
	}

	/* Fill-up xfer request */
	xfer->port = src;
	xfer->mbuf = mbuf;

	/* Add xfer request to rx_list */
	vmm_spin_lock_irqsave(&nsw->rx_list_lock, flags);
	list_add_tail(&xfer->head, &nsw->rx_list);
	nsw->rx_count++;
	vmm_spin_unlock_irqrestore(&nsw->rx_list_lock, flags);

	/* Signal completion to net switch bottom-half thread */
	vmm_completion_complete(&nsw->rx_not_empty);

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(vmm_netswitch_port2switch);

struct vmm_netswitch *vmm_netswitch_alloc(char *name, u32 thread_prio)
{
	struct vmm_netswitch *nsw;

	nsw = vmm_malloc(sizeof(struct vmm_netswitch));

	if (!nsw) {
		vmm_printf("%s Failed to allocate net switch\n", __func__);
		goto vmm_netswitch_alloc_failed;
	}

	memset(nsw, 0, sizeof(struct vmm_netswitch));
	nsw->name = vmm_malloc(strlen(name)+1);
	if (!nsw->name) {
		vmm_printf("%s Failed to allocate for net switch\n", __func__);
		goto vmm_netswitch_alloc_failed;
	}
	strcpy(nsw->name, name);

	INIT_MUTEX(&nsw->lock);
	INIT_LIST_HEAD(&nsw->port_list);

	nsw->thread = vmm_threads_create(nsw->name, vmm_netswitch_rx_handler,
					 nsw, thread_prio, 
					 VMM_THREAD_DEF_TIME_SLICE);
	if (!nsw->thread) {
		vmm_printf("%s Failed to create thread for net switch\n",
			   __func__);
		goto vmm_netswitch_alloc_failed;
	}

	INIT_COMPLETION(&nsw->rx_not_empty);
	nsw->rx_count = 0;
	INIT_SPIN_LOCK(&nsw->rx_list_lock);
	INIT_LIST_HEAD(&nsw->rx_list);

	goto vmm_netswitch_alloc_done;

vmm_netswitch_alloc_failed:
	if(nsw) {
		if(nsw->name) {
			vmm_free(nsw->name);
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
VMM_EXPORT_SYMBOL(vmm_netswitch_alloc);

void vmm_netswitch_free(struct vmm_netswitch *nsw)
{
	if (nsw) {
		if (nsw->name) {
			vmm_free(nsw->name);
		}
		if(nsw->thread) {
			vmm_threads_destroy(nsw->thread);
		}
		vmm_free(nsw);
	}
}
VMM_EXPORT_SYMBOL(vmm_netswitch_free);

int vmm_netswitch_port_add(struct vmm_netswitch *nsw,
			   struct vmm_netport *port)
{
	int rc = VMM_OK;

	if (!nsw || !port) {
		return VMM_EFAIL;
	}

	port->nsw = nsw;

	vmm_mutex_lock(&nsw->lock);

	/* Add the port to the port_list */
	list_add_tail(&port->head, &nsw->port_list);

	/* Call the netswitch's port_add callback */
	if(nsw->port_add) {
		rc = nsw->port_add(nsw, port);
	}

	vmm_mutex_unlock(&nsw->lock);

	if (rc == VMM_OK) {
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
VMM_EXPORT_SYMBOL(vmm_netswitch_port_add);

static void __netswitch_port_remove(struct vmm_netswitch *nsw,
				    struct vmm_netport *port)
{
	/* Call the netswitch's port_remove handler */
	if (nsw->port_remove) {
		nsw->port_remove(port);
	}

	/* Remove the port from port_list */
	port->nsw = NULL;
	list_del(&port->head);
}

int vmm_netswitch_port_remove(struct vmm_netport *port)
{
	struct vmm_netswitch *nsw;

	if (!port || !(port->nsw)) {
		return VMM_EFAIL;
	}
	nsw = port->nsw;

#ifdef CONFIG_VERBOSE_MODE
	vmm_printf("NET: Port(\"%s\") removed from Switch(\"%s\")\n",
			port->name, port->nsw->name);
#endif

	/* Notify the port about the link-status change */
	port->flags &= ~VMM_NETPORT_LINK_UP;
	port->link_changed(port);

	vmm_mutex_lock(&nsw->lock);
	__netswitch_port_remove(nsw, port);
	vmm_mutex_unlock(&nsw->lock);

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(vmm_netswitch_port_remove);

int vmm_netswitch_register(struct vmm_netswitch *nsw, 
			   struct vmm_device *dev,
			   void *priv)
{
	int rc;
	struct vmm_classdev *cd;

	if (!nsw) {
		return VMM_EFAIL;
	}

	cd = vmm_malloc(sizeof(struct vmm_classdev));
	if (!cd) {
		rc = VMM_EFAIL;
		goto ret;
	}

	INIT_LIST_HEAD(&cd->head);
	strcpy(cd->name, nsw->name);
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
VMM_EXPORT_SYMBOL(vmm_netswitch_register);

int vmm_netswitch_unregister(struct vmm_netswitch *nsw)
{
	int rc;
	struct dlist *list;
	struct vmm_netport *port;
	struct vmm_classdev *cd;

	if (!nsw) {
		return VMM_EFAIL;
	}

	cd = vmm_devdrv_find_classdev(VMM_NETSWITCH_CLASS_NAME, nsw->name);
	if (!cd) {
		return VMM_EFAIL;
	}

	vmm_threads_stop(nsw->thread);

	vmm_mutex_lock(&nsw->lock);

	/* Remove any ports still attached to this nsw */
	list_for_each(list, &nsw->port_list) {
		port = list_port(list);
		__netswitch_port_remove(nsw, port);
	}

	vmm_mutex_unlock(&nsw->lock);

	rc = vmm_devdrv_unregister_classdev(VMM_NETSWITCH_CLASS_NAME, cd);
	if (!rc) {
		vmm_free(cd);
	}

	return rc;
}
VMM_EXPORT_SYMBOL(vmm_netswitch_unregister);

struct vmm_netswitch *vmm_netswitch_find(const char *name)
{
	struct vmm_classdev *cd;

	cd = vmm_devdrv_find_classdev(VMM_NETSWITCH_CLASS_NAME, name);
	if (!cd) {
		return NULL;
	}

	return cd->priv;
}
VMM_EXPORT_SYMBOL(vmm_netswitch_find);

struct vmm_netswitch *vmm_netswitch_get(int num)
{
	struct vmm_classdev *cd;

	cd = vmm_devdrv_classdev(VMM_NETSWITCH_CLASS_NAME, num);
	if (!cd) {
		return NULL;
	}

	return cd->priv;
}
VMM_EXPORT_SYMBOL(vmm_netswitch_get);

u32 vmm_netswitch_count(void)
{
	return vmm_devdrv_classdev_count(VMM_NETSWITCH_CLASS_NAME);
}
VMM_EXPORT_SYMBOL(vmm_netswitch_count);

int __init vmm_netswitch_init(void)
{
	int rc;
	struct vmm_class *c;

	vmm_printf("Initialize Network Switch Framework\n");

	c = vmm_malloc(sizeof(struct vmm_class));
	if (!c) {
		return VMM_EFAIL;
	}

	INIT_LIST_HEAD(&c->head);
	strcpy(c->name, VMM_NETSWITCH_CLASS_NAME);
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

void __exit vmm_netswitch_exit(void)
{
	int rc;
	struct vmm_class *c;

	c = vmm_devdrv_find_class(VMM_NETSWITCH_CLASS_NAME);
	if (!c) {
		return;
	}

	rc = vmm_devdrv_unregister_class(c);
	if (rc) {
		vmm_printf("Failed to unregister %s class",
			VMM_NETSWITCH_CLASS_NAME);
		return;
	}

	vmm_free(c);

	return;
}
