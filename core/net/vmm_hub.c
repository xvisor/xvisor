/**
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
 * @file vmm_hub.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief software hub as netswitch.
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_devdrv.h>
#include <net/vmm_mbuf.h>
#include <net/vmm_netswitch.h>
#include <net/vmm_netport.h>

#undef DEBUG_HUB

#ifdef DEBUG_HUB
#define DPRINTF(fmt, ...) \
	do { vmm_printf(fmt , ## __VA_ARGS__); } while (0)
#else
#define DPRINTF(fmt, ...) do {} while(0)
#endif

/**
 *  Thread body responsible for sending the RX buffer packets
 *  to the destination port(s)
 */
static int hub_rx_handler(struct vmm_netswitch *nsw,
			     struct vmm_netport *src,
			     struct vmm_mbuf *mbuf)
{
	irq_flags_t f;
	struct dlist *l, *l1;
	struct vmm_netport *port;

	/* Broadcast mbuf to all ports except source port */
	DPRINTF("%s: broadcasting\n", __func__);
	vmm_read_lock_irqsave_lite(&nsw->port_list_lock, f);
	list_for_each_safe(l, l1, &nsw->port_list) {
		port = list_port(l);
		if (port == src) {
			continue;
		}
		vmm_read_unlock_irqrestore_lite(&nsw->port_list_lock, f);
		vmm_switch2port_xfer_mbuf(nsw, port, mbuf);
		vmm_read_lock_irqsave_lite(&nsw->port_list_lock, f);
	}
	vmm_read_unlock_irqrestore_lite(&nsw->port_list_lock, f);

	return VMM_OK;
}

static int hub_port_add(struct vmm_netswitch *nsw, 
			struct vmm_netport *port)
{
	/* For now nothing to do here. */
	return VMM_OK;
}

static int hub_port_remove(struct vmm_netswitch *nsw, 
			   struct vmm_netport *port)
{
	/* For now nothing to do here. */
	return VMM_OK;
}

static int hub_probe(struct vmm_device *dev,
		     const struct vmm_devtree_nodeid *nid)
{
	int rc = VMM_OK;
	struct vmm_netswitch *nsw = NULL;

	nsw = vmm_netswitch_alloc(dev->name);
	if (!nsw) {
		rc = VMM_ENOMEM;
		goto hub_netswitch_alloc_failed;
	}
	nsw->port2switch_xfer = hub_rx_handler;
	nsw->port_add = hub_port_add;
	nsw->port_remove = hub_port_remove;

	dev->priv = nsw;

	rc = vmm_netswitch_register(nsw, dev, NULL);
	if (rc) {
		goto hub_netswitch_register_fail;
	}

	return VMM_OK;

hub_netswitch_register_fail:
	vmm_netswitch_free(nsw);
hub_netswitch_alloc_failed:
	return rc;
}

static int hub_remove(struct vmm_device *dev)
{
	struct vmm_netswitch *nsw = dev->priv;

	if (!nsw) {
		return VMM_ENODEV;
	}

	vmm_netswitch_unregister(nsw);

	vmm_netswitch_free(nsw);

	return VMM_OK;
}

static struct vmm_devtree_nodeid hub_id_table[] = {
	{.type = "netswitch",.compatible = "hub"},
	{ /* end of list */ },
};

static struct vmm_driver hub = {
	.name = "hub",
	.match_table = hub_id_table,
	.probe = hub_probe,
	.remove = hub_remove,
};

int __init vmm_hub_init(void)
{
	return vmm_devdrv_register_driver(&hub);
}

void __exit vmm_hub_exit(void)
{
	vmm_devdrv_unregister_driver(&hub);
}

