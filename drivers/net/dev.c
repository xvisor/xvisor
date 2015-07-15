/**
 * Copyright (C) 2015 Institut de Recherche Technologique SystemX and OpenWide.
 * All rights reserved.
 * Adapted from Linux kernel version 3.18 net/core/dev.c file
 * by Jimmy Durand Wesolowski <jimmy.durand-wesolowski@openwide.fr>
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
 * @file dev.c
 * @author Jimmy Durand Wesolowski <jimmy.durand-wesolowski@openwide.fr>
 * @brief NET3 Protocol independent device support routines.
 */

#include <linux/netdevice.h>
#include <linux/phy.h>

int netdev_budget __read_mostly = 300;

static void lazy_xfer2napi_poll(struct vmm_netport *port, void *arg, int budget)
{
	struct napi_struct *napi = arg;

	napi->poll(napi, budget);
}

void netif_napi_add(struct net_device *dev, struct napi_struct *napi,
		    int (*poll)(struct napi_struct *, int), int weight)
{
	struct vmm_netport *port = dev->nsw_priv;

	if (weight > NAPI_POLL_WEIGHT)
		pr_err_once("netif_napi_add() called with weight %d on "
			    "device %s\n", weight, dev->name);
	napi->dev = dev;
	napi->poll = poll;
	INIT_LIST_HEAD(&napi->xfer.head);
	napi->xfer.port = port;
	napi->xfer.type = VMM_NETPORT_XFER_LAZY;
	napi->xfer.mbuf = NULL;
	napi->xfer.lazy_budget = netdev_budget;
	napi->xfer.lazy_arg = NULL;
	napi->xfer.lazy_xfer = lazy_xfer2napi_poll;
}
EXPORT_SYMBOL(netif_napi_add);

void napi_disable(struct napi_struct *n)
{
#if 0
	might_sleep();
	set_bit(NAPI_STATE_DISABLE, &n->state);
	while (test_and_set_bit(NAPI_STATE_SCHED, &n->state))
		msleep(1);
	clear_bit(NAPI_STATE_DISABLE, &n->state);
#endif
}

void napi_enable(struct napi_struct *n)
{
#if 0
	BUG_ON(!test_bit(NAPI_STATE_SCHED, &n->state));
	smp_mb__before_clear_bit();
	clear_bit(NAPI_STATE_SCHED, &n->state);
#endif
}

void napi_schedule(struct napi_struct *n)
{
	struct vmm_netport *port = n->dev->nsw_priv;

	if (!port) {
		vmm_printf("%s Net dev %s has no switch attached\n",
			   __func__, n->dev->name);
		return;
	}

	vmm_port2switch_xfer_lazy(port, lazy_xfer2napi_poll, n, n->xfer.lazy_budget);
}

void netif_napi_del(struct napi_struct *napi)
{
	list_del_init(&napi->xfer.head);
}
EXPORT_SYMBOL(netif_napi_del);

void __napi_complete(struct napi_struct *n)
{
#if 0
	BUG_ON(!test_bit(NAPI_STATE_SCHED, &n->state));
	BUG_ON(n->gro_list);

	list_del(&n->poll_list);
	smp_mb__before_clear_bit();
	clear_bit(NAPI_STATE_SCHED, &n->state);
#endif
}
EXPORT_SYMBOL(__napi_complete);

void napi_complete(struct napi_struct *n)
{
#if 0
	unsigned long flags;

	/*
	 * don't let napi dequeue from the cpu poll list
	 * just in case its running on a different cpu
	 */
	if (unlikely(test_bit(NAPI_STATE_NPSVC, &n->state)))
		return;

	napi_gro_flush(n, false);
	local_irq_save(flags);
	__napi_complete(n);
	local_irq_restore(flags);
#endif
}
EXPORT_SYMBOL(napi_complete);

/**
 * __napi_schedule - schedule for receive
 * @n: entry to schedule
 *
 * The entry's receive function will be scheduled to run
 */
void __napi_schedule(struct napi_struct *n)
{
#if 0
	unsigned long flags;

	local_irq_save(flags);
	____napi_schedule(&__get_cpu_var(softnet_data), n);
	local_irq_restore(flags);
#endif
}
EXPORT_SYMBOL(__napi_schedule);
