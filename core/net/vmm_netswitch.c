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
#include <vmm_waitqueue.h>
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

#define NETSWITCH_BH_XFER_RING_SZ	(1 << 10)
#define NETSWITCH_BH_XFER_RING_MASK	(NETSWITCH_BH_XFER_RING_SZ - 1)

struct vmm_netswitch_bh_ctrl {
	struct vmm_thread *thread;
	struct vmm_waitqueue wq;
	unsigned long produce;
	unsigned long consume;
	struct vmm_netport_xfer *ring[NETSWITCH_BH_XFER_RING_SZ];
};

static DEFINE_PER_CPU(struct vmm_netswitch_bh_ctrl, nbctrl);

static bool netswitch_bh_ring_produceop(unsigned long *produce,
					unsigned long consume,
					unsigned long limit,
					unsigned long *oldproduce)
{
	bool ret = FALSE;
	irq_flags_t flags;

	arch_cpu_irq_save(flags);

	if (*produce < (consume + limit)) {
		*oldproduce = *produce;
		*produce = *produce + 1;
		ret = TRUE;
	}

	arch_cpu_irq_restore(flags);

	return ret;
}

static bool netswitch_bh_ring_consumeop(unsigned long *consume,
					unsigned long produce,
					unsigned long *oldconsume)
{
	bool ret = FALSE;
	irq_flags_t flags;

	arch_cpu_irq_save(flags);

	if (*consume < produce) {
		*oldconsume = *consume;
		*consume = *consume + 1;
		ret = TRUE;
	}

	arch_cpu_irq_restore(flags);

	return ret;
}

static void __init netswitch_bh_ring_init(struct vmm_netswitch_bh_ctrl *nbp)
{
	nbp->produce = 0;
	nbp->consume = 0;
	memset(&nbp->ring, 0, sizeof(nbp->ring));
}

static int netswitch_bh_ring_enqueue(struct vmm_netswitch_bh_ctrl *nbp,
				     struct vmm_netport_xfer *xfer)
{
	u32 try, index;
	unsigned long produce = 0;

	try = 10;
	while (try && !netswitch_bh_ring_produceop(&nbp->produce,
						   nbp->consume,
						   NETSWITCH_BH_XFER_RING_SZ,
						   &produce)) {
		vmm_waitqueue_wakeall(&nbp->wq);
		try--;
	}
	if (!try) {
		return VMM_ENOSPC;
	}

	index = produce & NETSWITCH_BH_XFER_RING_MASK;
	nbp->ring[index] = xfer;

	vmm_waitqueue_wakeall(&nbp->wq);

	return VMM_OK;
}

static struct vmm_netport_xfer *netswitch_bh_ring_dequeue(
				struct vmm_netswitch_bh_ctrl *nbp)
{
#define NETSWITCH_TRIES_PER_PHASE_BITS	4
#define NETSWITCH_TRIES_PER_PHASE	(1 << NETSWITCH_TRIES_PER_PHASE_BITS)
#define NETSWITCH_PHASE_COUNT		100
	u32 index, try, phase;
	u64 timeout;
	unsigned long consume;
	struct vmm_netport_xfer *xfer;

	try = 0;
	while (!netswitch_bh_ring_consumeop(&nbp->consume,
					    nbp->produce,
					    &consume)) {
		phase = try >> NETSWITCH_TRIES_PER_PHASE_BITS;
		if (phase == 0) {
			vmm_scheduler_yield();
		} else {
			timeout = CONFIG_NET_BH_TIMEOUT_SECS;
			timeout = timeout * 1000000000ULL;
			if (phase < NETSWITCH_PHASE_COUNT) {
				timeout = udiv64(timeout,
					NETSWITCH_PHASE_COUNT - (phase - 1));
			}
			vmm_waitqueue_sleep_timeout(&nbp->wq, &timeout);
		}
		try++;
	}

	index = consume & NETSWITCH_BH_XFER_RING_MASK;
	xfer = nbp->ring[index];
	nbp->ring[index] = NULL;

	return xfer;
}

static void netswitch_bh_ring_port_flush(struct vmm_netswitch_bh_ctrl *nbp,
					 struct vmm_netport *port)
{
	u32 index;
	struct vmm_netport_xfer *xfer;

	for (index = 0; index < NETSWITCH_BH_XFER_RING_SZ; index++) {
		xfer = nbp->ring[index];
		if (xfer && xfer->port == port) {
			nbp->ring[index] = NULL;
			if (xfer->mbuf) {
				m_freem(xfer->mbuf);
			}
			vmm_netport_free_xfer(xfer->port, xfer);
		}
	}
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
		xfer = netswitch_bh_ring_dequeue(nbp);
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
	rc = netswitch_bh_ring_enqueue(nbp, xfer);
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
	rc = netswitch_bh_ring_enqueue(nbp, xfer);
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
		netswitch_bh_ring_port_flush(nbp, port);
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

struct vmm_netswitch *vmm_netswitch_get(int num)
{
	struct vmm_device *dev;

	dev = vmm_devdrv_class_device(&nsw_class, num);
	if (!dev) {
		return NULL;
	}

	return vmm_devdrv_get_data(dev);
}
VMM_EXPORT_SYMBOL(vmm_netswitch_get);

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

	INIT_WAITQUEUE(&nbp->wq, NULL);
	netswitch_bh_ring_init(nbp);

	vmm_threads_start(nbp->thread);
}

int __init vmm_netswitch_init(void)
{
	int rc;

	vmm_printf("Initialize Network Switch Framework\n");

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
