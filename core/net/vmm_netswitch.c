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
	struct vmm_completion bh_cmpl;
	vmm_spinlock_t bh_list_lock;
	struct dlist mbuf_list;
	struct dlist lazy_list;
};

static DEFINE_PER_CPU(struct vmm_netswitch_bh_ctrl, nbctrl);

static DEFINE_MUTEX(policy_list_lock);
static LIST_HEAD(policy_list);

static void __init netswitch_bh_init(struct vmm_netswitch_bh_ctrl *nbp)
{
	INIT_COMPLETION(&nbp->bh_cmpl);
	INIT_SPIN_LOCK(&nbp->bh_list_lock);
	INIT_LIST_HEAD(&nbp->mbuf_list);
	INIT_LIST_HEAD(&nbp->lazy_list);
}

static int netswitch_bh_enqueue(struct vmm_netswitch_bh_ctrl *nbp,
				struct vmm_mbuf *mbuf,
				struct vmm_netport_lazy *lazy)
{
	irq_flags_t flags;

	if (!nbp || (!mbuf && !lazy)) {
		return VMM_EINVALID;
	}

	vmm_spin_lock_irqsave_lite(&nbp->bh_list_lock, flags);
	if (mbuf) {
		list_add_tail(&mbuf->m_list, &nbp->mbuf_list);
	}
	if (lazy) {
		list_add_tail(&lazy->head, &nbp->lazy_list);
	}
	vmm_spin_unlock_irqrestore_lite(&nbp->bh_list_lock, flags);

	vmm_completion_complete_once(&nbp->bh_cmpl);

	return VMM_OK;
}

static int netswitch_bh_dequeue(struct vmm_netswitch_bh_ctrl *nbp,
				struct vmm_mbuf **mbufp,
				struct vmm_netport_lazy **lazyp)
{
	irq_flags_t flags;

	if (!nbp || !mbufp || !lazyp) {
		return VMM_EINVALID;
	}

	vmm_spin_lock_irqsave_lite(&nbp->bh_list_lock, flags);

	while (list_empty(&nbp->mbuf_list) && list_empty(&nbp->lazy_list)) {
		vmm_spin_unlock_irqrestore_lite(&nbp->bh_list_lock, flags);
		vmm_completion_wait(&nbp->bh_cmpl);
		vmm_spin_lock_irqsave_lite(&nbp->bh_list_lock, flags);
	}

	if (!list_empty(&nbp->mbuf_list)) {
		*mbufp = list_entry(list_pop(&nbp->mbuf_list),
				    struct vmm_mbuf, m_list);
	}

	if (!list_empty(&nbp->lazy_list)) {
		*lazyp = list_entry(list_pop(&nbp->lazy_list),
				    struct vmm_netport_lazy, head);
	}

	vmm_spin_unlock_irqrestore_lite(&nbp->bh_list_lock, flags);

	return VMM_OK;
}

static void netswitch_bh_port_flush(struct vmm_netswitch_bh_ctrl *nbp,
					 struct vmm_netport *port)
{
	irq_flags_t flags;
	struct vmm_mbuf *mbuf, *nmbuf;
	struct vmm_netport_lazy *lazy, *nlazy;

	vmm_spin_lock_irqsave_lite(&nbp->bh_list_lock, flags);

	list_for_each_entry_safe(mbuf, nmbuf, &nbp->mbuf_list, m_list) {
		if (mbuf->m_list_priv == port) {
			list_del(&mbuf->m_list);
			mbuf->m_list_priv = NULL;
			m_freem(mbuf);
		}
	}

	list_for_each_entry_safe(lazy, nlazy, &nbp->lazy_list, head) {
		if (lazy->port == port) {
			list_del(&lazy->head);
		}
	}

	vmm_spin_unlock_irqrestore_lite(&nbp->bh_list_lock, flags);
}

static int netswitch_bh_main(void *param)
{
	int rc;
	struct vmm_netport *port;
	struct vmm_netswitch *nsw;
	struct vmm_mbuf *mbuf;
	struct vmm_netport_lazy *lazy;
	struct vmm_netswitch_bh_ctrl *nbp = param;

	while (1) {
		/* Try to get next request from list or block if empty */
		lazy = NULL;
		mbuf = NULL;
		rc = netswitch_bh_dequeue(nbp, &mbuf, &lazy);
		if (rc) {
			continue;
		}

		/* Process mbuf request */
		if (mbuf) {
			/* Extract port from mbuf */
			port = mbuf->m_list_priv;
			nsw = port->nsw;
			mbuf->m_list_priv = NULL;

			/* Port might have been removed from netswitch */
			if (!port || !nsw) {
				if (mbuf) {
					m_freem(mbuf);
				}
				continue;
			}

			/* Print debug info */
			DPRINTF("%s: nsw=%s port=%s mbuf\n", __func__,
				nsw->name, port->name);

			/* Dump packet */
			DUMP_NETSWITCH_PKT(mbuf);

			/* Call the rx function of net switch */
			nsw->port2switch_xfer(nsw, port, mbuf);

			/* Free mbuf */
			m_freem(mbuf);
		}

		/* Process lazy request */
		if (lazy) {
			/* Extract info from lazy request */
			port = lazy->port;
			nsw = port->nsw;

			/* Print debug info */
			DPRINTF("%s: nsw=%s port=%s lazy\n", __func__,
				nsw->name, port->name);

			/* Call lazy xfer function */
			lazy->xfer(port, lazy->arg, lazy->budget);

			/* Add back to netswitch bh queue if required */
			if (arch_atomic_sub_return(&lazy->sched_count, 1) > 0) {
				/* Enqueue lazy request */
				rc = netswitch_bh_enqueue(nbp, NULL, lazy);
				if (rc) {
					vmm_printf("%s: nsw=%s src=%s lazy bh "
					   "enqueue failed.\n", __func__,
					   nsw->name, port->name);
				}
			}
		}
	}

	return VMM_OK;
}

int vmm_port2switch_xfer_mbuf(struct vmm_netport *src, struct vmm_mbuf *mbuf)
{
	int rc;
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

	/* Save port in mbuf */
	mbuf->m_list_priv = src;

	/* Add mbuf bh queue */
	rc = netswitch_bh_enqueue(nbp, mbuf, NULL);
	if (rc) {
		vmm_printf("%s: nsw=%s src=%s mbuf bh enqueue failed.\n",
			   __func__, nsw->name, src->name);
		mbuf->m_list_priv = NULL;
	}

	return rc;
}
VMM_EXPORT_SYMBOL(vmm_port2switch_xfer_mbuf);

int vmm_port2switch_xfer_lazy(struct vmm_netport_lazy *lazy)
{
	int rc = VMM_EBUSY;
	long sched_count;

	if (!lazy || !lazy->xfer || !lazy->port || !lazy->port->nsw) {
		vmm_printf("%s: invalid lazy instance.\n", __func__);
		return VMM_EINVALID;
	}

	/* Print debug info */
	DPRINTF("%s: nsw=%s port=%s xfer lazy\n",
		__func__, lazy->port->nsw->name, lazy->port->name);

	sched_count = arch_atomic_add_return(&lazy->sched_count, 1);
	if (sched_count == 1) {
		/* Print debug info */
		DPRINTF("%s: nsw=%s port=%s bh enqueue\n",
			__func__, lazy->port->nsw->name, lazy->port->name);

		/* Add xfer request to xfer ring */
		rc = netswitch_bh_enqueue(&this_cpu(nbctrl), NULL, lazy);
		if (rc) {
			vmm_printf("%s: nsw=%s port=%s lazy bh "
				   "enqueue failed.\n", __func__,
				   lazy->port->nsw->name, lazy->port->name);
		} else {
			rc = VMM_OK;
		}
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

struct vmm_netswitch *vmm_netswitch_alloc(struct vmm_netswitch_policy *nsp,
					  const char *name)
{
	struct vmm_netswitch *nsw = NULL;

	if (!nsp || !name) {
		goto vmm_netswitch_alloc_done;
	}

	nsw = vmm_zalloc(sizeof(struct vmm_netswitch));
	if (!nsw) {
		vmm_printf("%s Failed to allocate net switch\n", __func__);
		goto vmm_netswitch_alloc_failed;
	}

	nsw->policy = nsp;
	strncpy(nsw->name, name, VMM_FIELD_NAME_SIZE);

	INIT_RW_LOCK(&nsw->port_list_lock);
	INIT_LIST_HEAD(&nsw->port_list);

	goto vmm_netswitch_alloc_done;

vmm_netswitch_alloc_failed:
	if (nsw) {
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

	if (!nsw || !nsw->policy) {
		return VMM_EINVALID;
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

int vmm_netswitch_policy_register(struct vmm_netswitch_policy *nsp)
{
	struct vmm_netswitch_policy *nsp1;

	if (!nsp || !nsp->create || !nsp->destroy) {
		return VMM_EINVALID;
	}

	vmm_mutex_lock(&policy_list_lock);

	list_for_each_entry(nsp1, &policy_list, head) {
		if (strcmp(nsp1->name, nsp->name) == 0) {
			vmm_mutex_unlock(&policy_list_lock);
			return VMM_EEXIST;
		}
	}

	INIT_LIST_HEAD(&nsp->head);
	list_add_tail(&nsp->head, &policy_list);

	vmm_mutex_unlock(&policy_list_lock);

	return VMM_OK;
}

struct netswitch_policy_unregister_priv {
	struct vmm_netswitch_policy *nsp;
	struct vmm_netswitch *nsw;
};

int netswitch_policy_unregister_find(struct vmm_netswitch *nsw, void *data)
{
	struct netswitch_policy_unregister_priv *priv = data;

	if (nsw->policy == priv->nsp) {
		priv->nsw = nsw;
	}

	return VMM_OK;
}

void vmm_netswitch_policy_unregister(struct vmm_netswitch_policy *nsp)
{
	int ret;
	struct netswitch_policy_unregister_priv priv;

	if (!nsp) {
		return;
	}

	vmm_mutex_lock(&policy_list_lock);

	do {
		priv.nsw = NULL;
		priv.nsp = nsp;
		ret = vmm_netswitch_iterate(NULL, &priv,
				      netswitch_policy_unregister_find);
		if (ret || !priv.nsw) {
			break;
		}

		nsp->destroy(nsp, priv.nsw);
	} while (1);

	list_del(&nsp->head);

	vmm_mutex_unlock(&policy_list_lock);
}
VMM_EXPORT_SYMBOL(vmm_netswitch_policy_unregister);

int vmm_netswitch_policy_iterate(struct vmm_netswitch_policy *start,
		void *data, int (*fn)(struct vmm_netswitch_policy *, void *))
{
	int ret = VMM_OK;
	bool found_start = (start) ? FALSE : TRUE;
	struct vmm_netswitch_policy *nsp;

	vmm_mutex_lock(&policy_list_lock);

	list_for_each_entry(nsp, &policy_list, head) {
		if (start == nsp)
			found_start = TRUE;
		if (found_start) {
			ret = fn(nsp, data);
			if (ret) {
				vmm_mutex_unlock(&policy_list_lock);
				return ret;
			}
		}
	}

	vmm_mutex_unlock(&policy_list_lock);

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(vmm_netswitch_policy_iterate);

struct netswitch_policy_find_priv {
	const char *name;
	struct vmm_netswitch_policy *nsp;
};

static int netswitch_policy_find(struct vmm_netswitch_policy *nsp, void *data)
{
	struct netswitch_policy_find_priv *priv = data;

	if (strcmp(priv->name, nsp->name) == 0) {
		priv->nsp = nsp;
	}

	return VMM_OK;
}

struct vmm_netswitch_policy *vmm_netswitch_policy_find(const char *name)
{
	int ret;
	struct netswitch_policy_find_priv priv = { .name = name, .nsp = NULL };

	ret = vmm_netswitch_policy_iterate(NULL, &priv, netswitch_policy_find);

	return (ret) ? NULL : priv.nsp;
}
VMM_EXPORT_SYMBOL(vmm_netswitch_policy_find);

static int netswitch_policy_count(struct vmm_netswitch_policy *nsp, void *data)
{
	u32 *ret = data;

	(*ret)++;

	return VMM_OK;
}

u32 vmm_netswitch_policy_count(void)
{
	u32 ret = 0;

	vmm_netswitch_policy_iterate(NULL, &ret, netswitch_policy_count);

	return ret;
}
VMM_EXPORT_SYMBOL(vmm_netswitch_policy_count);

int vmm_netswitch_policy_create_switch(const char *policy_name,
				       const char *switch_name,
				       int argc, char **argv)
{
	int ret = VMM_OK;
	struct vmm_netswitch *nsw = NULL;
	struct vmm_netswitch_policy *nsp;

	if (!policy_name || !switch_name || ((argc > 0) && !argv)) {
		return VMM_EINVALID;
	}

	vmm_mutex_lock(&policy_list_lock);

	list_for_each_entry(nsp, &policy_list, head) {
		if (strcmp(nsp->name, policy_name) != 0)
			continue;
		nsw = nsp->create(nsp, switch_name, argc, argv);
		if (!nsw) {
			ret = VMM_EFAIL;
			goto done_unlock;
		}

		break;
	}

	if (!nsw) {
		ret = VMM_EINVALID;
	}

done_unlock:
	vmm_mutex_unlock(&policy_list_lock);

	return ret;
}
VMM_EXPORT_SYMBOL(vmm_netswitch_policy_create_switch);

int vmm_netswitch_policy_destroy_switch(struct vmm_netswitch *nsw)
{
	if (!nsw || !nsw->policy) {
		return VMM_EINVALID;
	}

	vmm_mutex_lock(&policy_list_lock);
	nsw->policy->destroy(nsw->policy, nsw);
	vmm_mutex_unlock(&policy_list_lock);

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(vmm_netswitch_policy_destroy_switch);

static void vmm_netswitch_percpu_init(void *a1, void *a2, void *a3)
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

	vmm_init_printf("network switch framework\n");

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
