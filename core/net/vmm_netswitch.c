/**
 * Copyright (c) 2012 Pranav Sawargaonkar.
 * Copyright (c) 2012 Sukanto Ghosh.
 * Copyright (c) 2014 Anup Patel.
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
 * @author Anup Patel <anup@brainfault.org>
 * @brief Generic netswitch implementation.
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_smp.h>
#include <vmm_percpu.h>
#include <vmm_stdio.h>
#include <vmm_modules.h>
#include <vmm_threads.h>
#include <vmm_completion.h>
#include <net/vmm_mbuf.h>
#include <net/vmm_protocol.h>
#include <net/vmm_netswitch.h>
#include <net/vmm_netport.h>
#include <libs/list.h>
#include <libs/mathlib.h>
#include <libs/stringlib.h>

#undef DEBUG

#ifdef DEBUG
#define DPRINTF(fmt, ...) \
	do { vmm_printf(fmt , ## __VA_ARGS__); } while (0)

#define DUMP_NETSWITCH_PKT(mbuf) 					\
do {									\
char tname[30];								\
const u8 *srcmac = ether_srcmac(mtod(mbuf, u8 *));			\
const u8 *dstmac = ether_dstmac(mtod(mbuf, u8 *));			\
const u8 *ip_frame, *icmp_frame, *tcp_frame;				\
									\
DPRINTF("%s: got pkt with srcaddr[%s]", __func__,			\
		ethaddr_to_str(tname, srcmac));				\
DPRINTF(", dstaddr[%s]", ethaddr_to_str(tname, dstmac));		\
DPRINTF(", ethertype: 0x%04X\n", ether_type(mtod(mbuf, u8 *)));		\
if (ether_type(mtod(mbuf, u8 *)) == 0x0806	/* ARP */) {		\
	DPRINTF("\tARP-HType: 0x%04X\n", 				\
		arp_htype(ether_payload(mtod(mbuf, u8 *))));		\
	DPRINTF("\tARP-PType: 0x%04X\n", 				\
		arp_ptype(ether_payload(mtod(mbuf, u8 *))));		\
	DPRINTF("\tARP-Hlen: 0x%02X\n",  				\
		arp_hlen(ether_payload(mtod(mbuf, u8 *))));		\
	DPRINTF("\tARP-Plen: 0x%02X\n",  				\
		arp_plen(ether_payload(mtod(mbuf, u8 *))));		\
	DPRINTF("\tARP-Oper: 0x%04X\n",  				\
		arp_oper(ether_payload(mtod(mbuf, u8 *))));		\
	DPRINTF("\tARP-SHA: %s\n", ethaddr_to_str(tname, 		\
		arp_sha(ether_payload((mtod(mbuf, u8 *))))));		\
	DPRINTF("\tARP-SPA: %s\n", ip4addr_to_str(tname, 		\
		arp_spa(ether_payload((mtod(mbuf, u8 *))))));		\
	DPRINTF("\tARP-THA: %s\n", ethaddr_to_str(tname, 		\
		arp_tha(ether_payload((mtod(mbuf, u8 *))))));		\
	DPRINTF("\tARP-TPA: %s\n", ip4addr_to_str(tname, 		\
		arp_tpa(ether_payload((mtod(mbuf, u8 *))))));		\
} else if (ether_type(mtod(mbuf, u8 *)) == 0x0800/* IPv4 */) {		\
	ip_frame = ether_payload(mtod(mbuf, u8 *));			\
	DPRINTF("\tIP-SRC: %s\n", ip4addr_to_str(tname, 		\
		ip_srcaddr(ip_frame)));					\
	DPRINTF("\tIP-DST: %s\n", ip4addr_to_str(tname, 		\
		ip_dstaddr(ip_frame)));					\
	DPRINTF("\tIP-LEN: %d\n", 					\
		ip_len(ip_frame));					\
	DPRINTF("\tIP-TTL: %d\n", 					\
		ip_ttl(ip_frame));					\
	DPRINTF("\tIP-CHKSUM: 0x%04X\n",				\
		ip_chksum(ip_frame));					\
	DPRINTF("\tIP-PROTOCOL: %d\n",					\
		ip_protocol(ip_frame));					\
	if (ip_protocol(ip_frame) == 0x01/* ICMP */) {			\
		icmp_frame = ip_payload(ip_frame); 			\
		DPRINTF("\t\tICMP-TYPE: 0x%x\n", 			\
			icmp_type(icmp_frame));				\
		DPRINTF("\t\tICMP-CODE: 0x%x\n", 			\
			icmp_code(icmp_frame));				\
		DPRINTF("\t\tICMP-CHECKSUM: 0x%x\n", 			\
			icmp_checksum(icmp_frame));			\
		DPRINTF("\t\tICMP-ID: 0x%x\n", 				\
			icmp_id(icmp_frame));				\
		DPRINTF("\t\tICMP-SEQUENCE: 0x%x\n", 			\
			icmp_sequence(icmp_frame));			\
	} else if (ip_protocol(ip_frame) == 0x06/* TCP */) {		\
		tcp_frame = ip_payload(ip_frame); 			\
		DPRINTF("\t\tTCP-SRCPORT: %d\n", 			\
			tcp_srcport(tcp_frame));			\
		DPRINTF("\t\tTCP-DSTPORT: %d\n", 			\
			tcp_dstport(tcp_frame));			\
		DPRINTF("\t\tTCP-SEQUENCE: 0x%x\n", 			\
			tcp_sequence(tcp_frame));			\
		DPRINTF("\t\tTCP-ACKNUMBER: 0x%x\n", 			\
			tcp_acknumber(tcp_frame));			\
		DPRINTF("\t\tTCP-FLAGS: 0x%x\n", 			\
			tcp_flags(tcp_frame));				\
		DPRINTF("\t\tTCP-CHECKSUM: 0x%x\n", 			\
			tcp_checksum(tcp_frame));			\
		DPRINTF("\t\tTCP-URGENT: 0x%x\n", 			\
			tcp_urgent(tcp_frame));				\
	}								\
}									\
} while(0)
#else
#define DPRINTF(fmt, ...) do {} while(0)
#define DUMP_NETSWITCH_PKT(mbuf)
#endif

struct vmm_netswitch_bh_ctrl {
	struct vmm_thread *thread;
	struct vmm_completion xfer_cmpl;
	vmm_spinlock_t xfer_list_lock;
	struct dlist xfer_list;
};

static DEFINE_PER_CPU(struct vmm_netswitch_bh_ctrl, nbctrl);

static void __init netswitch_bh_init(struct vmm_netswitch_bh_ctrl *nbp)
{
	INIT_COMPLETION(&nbp->xfer_cmpl);
	INIT_SPIN_LOCK(&nbp->xfer_list_lock);
	INIT_LIST_HEAD(&nbp->xfer_list);
}

static int netswitch_bh_enqueue(struct vmm_netswitch_bh_ctrl *nbp,
				     struct vmm_netport_xfer *xfer)
{
	irq_flags_t flags;

	vmm_spin_lock_irqsave_lite(&nbp->xfer_list_lock, flags);
	list_add_tail(&xfer->head, &nbp->xfer_list);
	vmm_spin_unlock_irqrestore_lite(&nbp->xfer_list_lock, flags);

	vmm_completion_complete_once(&nbp->xfer_cmpl);

	return VMM_OK;
}

static struct vmm_netport_xfer *netswitch_bh_dequeue(
				struct vmm_netswitch_bh_ctrl *nbp)
{
	irq_flags_t flags;
	struct vmm_netport_xfer *xfer;

	vmm_spin_lock_irqsave_lite(&nbp->xfer_list_lock, flags);

	while (list_empty(&nbp->xfer_list)) {
		vmm_spin_unlock_irqrestore_lite(&nbp->xfer_list_lock, flags);
		vmm_completion_wait(&nbp->xfer_cmpl);
		vmm_spin_lock_irqsave_lite(&nbp->xfer_list_lock, flags);
	}

	xfer = list_entry(list_pop(&nbp->xfer_list),
			  struct vmm_netport_xfer, head);

	vmm_spin_unlock_irqrestore_lite(&nbp->xfer_list_lock, flags);

	return xfer;
}

static void netswitch_bh_port_flush(struct vmm_netswitch_bh_ctrl *nbp,
					 struct vmm_netport *port)
{
	irq_flags_t flags;
	struct vmm_netport_xfer *xfer, *nxfer;

	vmm_spin_lock_irqsave_lite(&nbp->xfer_list_lock, flags);

	list_for_each_entry_safe(xfer, nxfer, &nbp->xfer_list, head) {
		if (xfer->port == port) {
			list_del(&xfer->head);
			if (xfer->mbuf) {
				m_freem(xfer->mbuf);
			}
			vmm_netport_free_xfer(xfer->port, xfer);
		}
	}

	vmm_spin_unlock_irqrestore_lite(&nbp->xfer_list_lock, flags);
}

static int netswitch_bh_main(void *param)
{
	struct vmm_netport *xfer_port;
	struct vmm_netswitch *xfer_nsw;
	enum vmm_netport_xfer_type xfer_type;
	struct vmm_mbuf *xfer_mbuf;
	int xfer_lazy_budget;
	void *xfer_lazy_arg;
	void (*xfer_lazy_xfer)(struct vmm_netport *, void *, int);
	struct vmm_netport_xfer *xfer;
	struct vmm_netswitch_bh_ctrl *nbp = param;

	while (1) {
		/* Try to get xfer request from xfer ring */
		xfer = netswitch_bh_dequeue(nbp);
		if (!xfer) {
			continue;
		}

		/* Extract info from xfer request */
		xfer_port = xfer->port;
		xfer_nsw = xfer->port->nsw;
		xfer_type = xfer->type;
		xfer_mbuf = xfer->mbuf;
		xfer_lazy_budget = xfer->lazy_budget;
		xfer_lazy_arg = xfer->lazy_arg;
		xfer_lazy_xfer = xfer->lazy_xfer;

		/* Free netport xfer request */
		vmm_netport_free_xfer(xfer->port, xfer);

		/* Port might have been removed from netswitch */
		if (!xfer_port || !xfer_nsw) {
			if (xfer_mbuf) {
				m_freem(xfer_mbuf);
			}
			continue;
		}

		/* Print debug info */
		DPRINTF("%s: nsw=%s xfer_type=%d\n", __func__,
			xfer_nsw->name, xfer_type);

		/* Process xfer request */
		switch (xfer_type) {
		case VMM_NETPORT_XFER_LAZY:
			/* Call lazy xfer function */
			xfer_lazy_xfer(xfer_port,
					xfer_lazy_arg,
					xfer_lazy_budget);

			break;
		case VMM_NETPORT_XFER_MBUF:
			/* Dump packet */
			DUMP_NETSWITCH_PKT(xfer_mbuf);

			/* Call the rx function of net switch */
			xfer_nsw->port2switch_xfer(xfer_nsw, xfer_port, xfer_mbuf);

			/* Free mbuf in xfer request */
			m_freem(xfer_mbuf);

			break;
		default:
			break;
		};
	}

	return VMM_OK;
}

int vmm_port2switch_xfer_mbuf(struct vmm_netport *src, struct vmm_mbuf *mbuf)
{
	int rc;
	struct vmm_netport_xfer *xfer;
	struct vmm_netswitch *nsw;
	struct vmm_netswitch_bh_ctrl *nbp;

	if (!mbuf) {
		return VMM_EFAIL;
	}
	if (!src || !src->nsw) {
		vmm_printf("%s: invalid source port.\n", __func__);
		m_freem(mbuf);
		return VMM_EFAIL;
	}
	nsw = src->nsw;
	nbp = &this_cpu(nbctrl);

	/* Print debug info */
	DPRINTF("%s: nsw=%s src=%s\n", __func__, nsw->name, src->name);

	/* Alloc netport xfer request */
	xfer = vmm_netport_alloc_xfer(src);
	if (!xfer) {
		vmm_printf("%s: nsw=%s src=%s xfer alloc failed.\n",
			   __func__, nsw->name, src->name);
		m_freem(mbuf);
		return VMM_ENOMEM;
	}

	/* Fill-up xfer request */
	xfer->port = src;
	xfer->type = VMM_NETPORT_XFER_MBUF;
	xfer->mbuf = mbuf;

	/* Add xfer request to xfer ring */
	rc = netswitch_bh_enqueue(nbp, xfer);
	if (rc) {
		vmm_printf("%s: nsw=%s src=%s xfer bh enqueue failed.\n",
			   __func__, nsw->name, src->name);
		vmm_netport_free_xfer(xfer->port, xfer);
	}

	return rc;
}
VMM_EXPORT_SYMBOL(vmm_port2switch_xfer_mbuf);

int vmm_port2switch_xfer_lazy(struct vmm_netport *src,
			 void (*lazy_xfer)(struct vmm_netport *, void *, int),
			 void *lazy_arg, int lazy_budget)
{
	int rc;
	struct vmm_netport_xfer *xfer;
	struct vmm_netswitch *nsw;
	struct vmm_netswitch_bh_ctrl *nbp;

	if (!lazy_xfer || !src || !src->nsw) {
		vmm_printf("%s: invalid source port or xfer callback.\n",
			   __func__);
		return VMM_EFAIL;
	}
	nsw = src->nsw;
	nbp = &this_cpu(nbctrl);

	/* Print debug info */
	DPRINTF("%s: nsw=%s src=%s\n", __func__, nsw->name, src->name);

	/* Alloc netport xfer request */
	xfer = vmm_netport_alloc_xfer(src);
	if (!xfer) {
		vmm_printf("%s: nsw=%s src=%s xfer alloc failed.\n",
			   __func__, nsw->name, src->name);
		return VMM_ENOMEM;
	}

	/* Fill-up xfer request */
	xfer->port = src;
	xfer->type = VMM_NETPORT_XFER_LAZY;
	xfer->lazy_arg = lazy_arg;
	xfer->lazy_budget = lazy_budget;
	xfer->lazy_xfer = lazy_xfer;

	/* Add xfer request to xfer ring */
	rc = netswitch_bh_enqueue(nbp, xfer);
	if (rc) {
		vmm_printf("%s: nsw=%s src=%s xfer bh enqueue failed.\n",
			   __func__, nsw->name, src->name);
		vmm_netport_free_xfer(xfer->port, xfer);
	}

	return rc;
}
VMM_EXPORT_SYMBOL(vmm_port2switch_xfer_lazy);

int vmm_switch2port_xfer_mbuf(struct vmm_netswitch *nsw,
			      struct vmm_netport *dst,
			      struct vmm_mbuf *mbuf)
{
	int rc;
	irq_flags_t f;

	if (!nsw || !dst || !mbuf) {
		return VMM_EFAIL;
	}

	/* Print debug info */
	DPRINTF("%s: nsw=%s dst=%s\n", __func__, nsw->name, dst->name);

	if (dst->can_receive && !dst->can_receive(dst)) {
		return VMM_OK;
	}

	MADDREFERENCE(mbuf);
	MCLADDREFERENCE(mbuf);

	vmm_spin_lock_irqsave_lite(&dst->switch2port_xfer_lock, f);
	rc = dst->switch2port_xfer(dst, mbuf);
	vmm_spin_unlock_irqrestore_lite(&dst->switch2port_xfer_lock, f);

	return rc;
}
VMM_EXPORT_SYMBOL(vmm_switch2port_xfer_mbuf);

struct vmm_netswitch *vmm_netswitch_alloc(char *name)
{
	struct vmm_netswitch *nsw;

	nsw = vmm_zalloc(sizeof(struct vmm_netswitch));
	if (!nsw) {
		vmm_printf("%s Failed to allocate net switch\n", __func__);
		goto vmm_netswitch_alloc_failed;
	}

	strncpy(nsw->name, name, VMM_FIELD_NAME_SIZE);

	INIT_RW_LOCK(&nsw->port_list_lock);
	INIT_LIST_HEAD(&nsw->port_list);

	goto vmm_netswitch_alloc_done;

vmm_netswitch_alloc_failed:
	if(nsw) {
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
		vmm_free(nsw);
	}
}
VMM_EXPORT_SYMBOL(vmm_netswitch_free);

int vmm_netswitch_port_add(struct vmm_netswitch *nsw,
			   struct vmm_netport *port)
{
	int rc = VMM_OK;
	irq_flags_t f;

	if (!nsw || !port) {
		return VMM_EFAIL;
	}

	/* Call the netswitch's port_add callback */
	if (nsw->port_add) {
		rc = nsw->port_add(nsw, port);
	}

	if (rc == VMM_OK) {
		/* Add the port to the port_list */
		vmm_write_lock_irqsave_lite(&nsw->port_list_lock, f);
		list_add_tail(&port->head, &nsw->port_list);
		vmm_write_unlock_irqrestore_lite(&nsw->port_list_lock, f);

		/* Mark this port to belong to the netswitch */
		port->nsw = nsw;

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

static void netswitch_port_remove(struct vmm_netswitch *nsw,
				  struct vmm_netport *port)
{
	u32 c;
	irq_flags_t f;
	struct vmm_netswitch_bh_ctrl *nbp;

	/* Notify the port about the link-status change */
	port->flags &= ~VMM_NETPORT_LINK_UP;
	port->link_changed(port);

	/* Mark the port to belong to NULL netswitch */
	port->nsw = NULL;

	/* Flush all xfer request related to this port */
	for_each_online_cpu(c) {
		nbp = &per_cpu(nbctrl, c);
		netswitch_bh_port_flush(nbp, port);
	}

	/* Remove the port from port_list */
	vmm_write_lock_irqsave_lite(&nsw->port_list_lock, f);
	list_del(&port->head);
	vmm_write_unlock_irqrestore_lite(&nsw->port_list_lock, f);

	/* Call the netswitch's port_remove handler */
	if (nsw->port_remove) {
		nsw->port_remove(nsw, port);
	}
}

int vmm_netswitch_port_remove(struct vmm_netport *port)
{
	if (!port) {
		return VMM_EFAIL;
	}

	if (!port->nsw) {
		return VMM_OK;
	}

#ifdef CONFIG_VERBOSE_MODE
	vmm_printf("NET: Port(\"%s\") removed from Switch(\"%s\")\n",
			port->name, port->nsw->name);
#endif

	netswitch_port_remove(port->nsw, port);

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(vmm_netswitch_port_remove);

static struct vmm_class nsw_class = {
	.name = VMM_NETSWITCH_CLASS_NAME,
};

int vmm_netswitch_register(struct vmm_netswitch *nsw,
			   struct vmm_device *parent,
			   void *priv)
{
	int rc;

	if (!nsw) {
		return VMM_EFAIL;
	}

	vmm_devdrv_initialize_device(&nsw->dev);
	if (strlcpy(nsw->dev.name, nsw->name, sizeof(nsw->dev.name)) >=
	    sizeof(nsw->dev.name)) {
		return VMM_EOVERFLOW;
	}
	nsw->dev.parent = parent;
	nsw->dev.class = &nsw_class;
	vmm_devdrv_set_data(&nsw->dev, nsw);

	rc = vmm_devdrv_register_device(&nsw->dev);
	if (rc != VMM_OK) {
		vmm_printf("%s: Failed to class register network switch %s "
			   "with err 0x%x\n", __func__, nsw->name, rc);
		return rc;
	}

	nsw->priv = priv;

#ifdef CONFIG_VERBOSE_MODE
	vmm_printf("Successfully registered VMM net switch: %s\n", nsw->name);
#endif

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(vmm_netswitch_register);

int vmm_netswitch_unregister(struct vmm_netswitch *nsw)
{
	irq_flags_t f;
	struct vmm_netport *port;

	if (!nsw) {
		return VMM_EFAIL;
	}

	vmm_read_lock_irqsave_lite(&nsw->port_list_lock, f);

	/* Remove any ports still attached to this nsw */
	while (!list_empty(&nsw->port_list)) {
		port = list_port(list_first(&nsw->port_list));
		vmm_read_unlock_irqrestore_lite(&nsw->port_list_lock, f);
		netswitch_port_remove(nsw, port);
		vmm_read_lock_irqsave_lite(&nsw->port_list_lock, f);
	}

	vmm_read_unlock_irqrestore_lite(&nsw->port_list_lock, f);

	return vmm_devdrv_unregister_device(&nsw->dev);
}
VMM_EXPORT_SYMBOL(vmm_netswitch_unregister);

struct vmm_netswitch *vmm_netswitch_find(const char *name)
{
	struct vmm_device *dev;

	dev = vmm_devdrv_class_find_device_by_name(&nsw_class, name);
	if (!dev) {
		return NULL;
	}

	return vmm_devdrv_get_data(dev);
}
VMM_EXPORT_SYMBOL(vmm_netswitch_find);

struct netswitch_iterate_priv {
	void *data;
	int (*fn)(struct vmm_netswitch *nsw, void *data);
};

static int netswitch_iterate(struct vmm_device *dev, void *data)
{
	struct netswitch_iterate_priv *p = data;
	struct vmm_netswitch *nsw = vmm_devdrv_get_data(dev);

	return p->fn(nsw, p->data);
}

int vmm_netswitch_iterate(struct vmm_netswitch *start, void *data,
			  int (*fn)(struct vmm_netswitch *nsw, void *data))
{
	struct vmm_device *st = (start) ? &start->dev : NULL;
	struct netswitch_iterate_priv p;

	if (!fn) {
		return VMM_EINVALID;
	}

	p.data = data;
	p.fn = fn;

	return vmm_devdrv_class_device_iterate(&nsw_class, st,
						&p, netswitch_iterate);
}
VMM_EXPORT_SYMBOL(vmm_netswitch_iterate);

static int netswitch_default_iterate(struct vmm_netswitch *nsw,
				     void *data)
{
	struct vmm_netswitch **out_nsw = data;

	if (!(*out_nsw)) {
		*out_nsw = nsw;
	}

	return VMM_OK;
}

struct vmm_netswitch *vmm_netswitch_default(void)
{
	struct vmm_netswitch *nsw = NULL;

	vmm_netswitch_iterate(NULL, &nsw, netswitch_default_iterate);

	return nsw;
}
VMM_EXPORT_SYMBOL(vmm_netswitch_default);

u32 vmm_netswitch_count(void)
{
	return vmm_devdrv_class_device_count(&nsw_class);
}
VMM_EXPORT_SYMBOL(vmm_netswitch_count);

static void __init vmm_netswitch_percpu_init(void *a1, void *a2, void *a3)
{
	char name[VMM_FIELD_NAME_SIZE];
	u32 cpu = vmm_smp_processor_id();
	struct vmm_netswitch_bh_ctrl *nbp = &per_cpu(nbctrl, cpu);

	vmm_snprintf(name, sizeof(name), "%s/%d",
		     VMM_NETSWITCH_CLASS_NAME, cpu);

	nbp->thread = vmm_threads_create(name, netswitch_bh_main,
					 nbp, VMM_THREAD_DEF_PRIORITY,
					 VMM_THREAD_DEF_TIME_SLICE);
	if (!nbp->thread) {
		vmm_printf("%s: CPU%d: Failed to create thread\n",
			   __func__, cpu);
		return;
	}

	if (vmm_threads_set_affinity(nbp->thread, vmm_cpumask_of(cpu))) {
		vmm_printf("%s: CPU%d: Failed to set thread affinity\n",
			   __func__, cpu);
		vmm_threads_destroy(nbp->thread);
		return;
	}

	netswitch_bh_init(nbp);

	vmm_threads_start(nbp->thread);
}

int __init vmm_netswitch_init(void)
{
	int rc;

	vmm_printf("init: network switch framework\n");

	rc = vmm_devdrv_register_class(&nsw_class);
	if (rc) {
		vmm_printf("Failed to register %s class\n",
			VMM_NETSWITCH_CLASS_NAME);
		return rc;
	}

	vmm_smp_ipi_async_call(cpu_online_mask,
			       vmm_netswitch_percpu_init,
			       NULL, NULL, NULL);

	return VMM_OK;
}

static void __exit vmm_netswitch_percpu_exit(void *a1, void *a2, void *a3)
{
	struct vmm_netswitch_bh_ctrl *nbp = &this_cpu(nbctrl);

	vmm_threads_stop(nbp->thread);

	vmm_threads_destroy(nbp->thread);
}

void __exit vmm_netswitch_exit(void)
{
	int rc;
	struct vmm_class *c;

	vmm_smp_ipi_sync_call(cpu_online_mask, 1000,
			      vmm_netswitch_percpu_exit,
			      NULL, NULL, NULL);

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
