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
 * @file vmm_bridge.c
 * @author Sukanto Ghosh <sukantoghosh@gmail.com>
 * @brief software bridge as netswitch.
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_stdio.h>
#include <vmm_timer.h>
#include <vmm_devdrv.h>
#include <vmm_modules.h>
#include <net/vmm_protocol.h>
#include <net/vmm_mbuf.h>
#include <net/vmm_netswitch.h>
#include <net/vmm_netport.h>
#include <libs/list.h>
#include <libs/stringlib.h>

#undef DEBUG_BRIDGE

#ifdef DEBUG_BRIDGE
#define DPRINTF(fmt, ...) \
	do { vmm_printf(fmt , ## __VA_ARGS__); } while (0)
#else
#define DPRINTF(fmt, ...) do {} while(0)
#endif

#define BRIDGE_MAC_EXPIRY	60000000000LLU

/* We maintain a table of learned mac addresses 
 * (please note that the mac of the immediate netports are not 
 * kept in this table) */
struct bridge_mac_entry {
	struct dlist head;
	u8 macaddr[6];
	struct vmm_netport *port;
	u64 timestamp;
};

#define list_mac_entry(l) list_entry(l, struct bridge_mac_entry, head)

struct bridge_ctrl {
	vmm_spinlock_t mac_table_lock;
	struct dlist mac_table;
};

static void bridge_mactable_learn(struct bridge_ctrl *br,
				  const u8 *mac, struct vmm_netport *port)
{
	bool learn;
	u64 tstamp;
	irq_flags_t f;
	struct dlist *l, *l1;
	struct bridge_mac_entry *m;

	/* Retrive current timestamp */
	learn = TRUE;
	tstamp = vmm_timer_timestamp();

	/* Learn the (mac, port) mapping */
	vmm_spin_lock_irqsave_lite(&br->mac_table_lock, f);
	list_for_each_safe(l, l1, &br->mac_table) {
		m = list_mac_entry(l);

		/* Match mac */
		if (!compare_ether_addr(m->macaddr, mac)) {
			m->port = port;
			m->timestamp = tstamp;
			learn = FALSE;
			break;
		/* Purge this entry if too old */
		} else if ((tstamp - m->timestamp) > BRIDGE_MAC_EXPIRY) {
			l = m->head.next;
			list_del(&m->head);
			vmm_free(m);
		}
	}
	if (learn) {
		m = vmm_malloc(sizeof(struct bridge_mac_entry));
		if (m) {
			m->port = port;
			memcpy(m->macaddr, mac, 6);
			m->timestamp = tstamp;
			list_add_tail(&m->head, &br->mac_table);
		} else {
			DPRINTF("%s: allocation failure\n", __func__);
		}
	}
	vmm_spin_unlock_irqrestore_lite(&br->mac_table_lock, f);
}

static struct vmm_netport *bridge_mactable_find_port(struct bridge_ctrl *br,
						     const u8 *mac)
{
	irq_flags_t f;
	struct dlist *l, *l1;
	struct vmm_netport *port = NULL;
	struct bridge_mac_entry *m;

	vmm_spin_lock_irqsave_lite(&br->mac_table_lock, f);
	list_for_each_safe(l, l1, &br->mac_table) {
		m = list_mac_entry(l);
		if (!compare_ether_addr(m->macaddr, mac)) {
			port = m->port;
			break;
		}
	}
	vmm_spin_unlock_irqrestore_lite(&br->mac_table_lock, f);

	return port;
}

static void bridge_mactable_cleanup_port(struct bridge_ctrl *br,
					 struct vmm_netport *port)
{
	irq_flags_t f;
	struct dlist *l, *l1;
	struct bridge_mac_entry *m;

	vmm_spin_lock_irqsave_lite(&br->mac_table_lock, f);
	list_for_each_safe(l, l1, &br->mac_table) {
		m = list_mac_entry(l);
		if (m->port == port) {
			list_del(&m->head);
			vmm_free(m);
		}
	}
	vmm_spin_unlock_irqrestore_lite(&br->mac_table_lock, f);
}

static void bridge_mactable_flush(struct bridge_ctrl *br)
{
	irq_flags_t f;
	struct dlist *l;

	vmm_spin_lock_irqsave_lite(&br->mac_table_lock, f);
	while (!list_empty(&br->mac_table)) {
		l = list_pop(&br->mac_table);
		vmm_free(l);
	}
	vmm_spin_unlock_irqrestore_lite(&br->mac_table_lock, f);
}

/**
 *  Thread body responsible for sending the RX buffer packets
 *  to the destination port(s)
 */
static int bridge_rx_handler(struct vmm_netswitch *nsw,
			     struct vmm_netport *src,
			     struct vmm_mbuf *mbuf)
{
#ifdef DEBUG_BRIDGE
	char tname[30];
#endif
	irq_flags_t f;
	const u8 *srcmac, *dstmac;
	bool broadcast = TRUE;
	struct dlist *l, *l1;
	struct vmm_netport *dst = NULL, *port;
	struct bridge_ctrl *br = nsw->priv;

	/* Get source and destination mac addresses */
	srcmac = ether_srcmac(mtod(mbuf, u8 *));
	dstmac = ether_dstmac(mtod(mbuf, u8 *));

	/* Learn source mac address */
	bridge_mactable_learn(br, srcmac, src);

	/* Find if the frame should be unicast because it satisfies 
	 * both of these:
	 * Case 1: It is not a broadcast MAC address, and
	 * Case 2: We do have the MAC->port mapping
	 */
	if (!is_broadcast_ether_addr(dstmac)) {
		/* Find port fordestination mac address */
		dst = bridge_mactable_find_port(br, dstmac);
		if (dst) {
			broadcast = FALSE;
		}
	}

	/* Transfer mbuf to appropriate ports */
	if (broadcast) {
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
	} else {
		DPRINTF("%s: unicasting to \"%s\"\n", __func__, dst->name);
		vmm_switch2port_xfer_mbuf(nsw, dst, mbuf);
	}

	return VMM_OK;
}

static int bridge_port_add(struct vmm_netswitch *nsw, 
			   struct vmm_netport *port)
{
	/* For now nothing to do here. */
	return VMM_OK;
}

static int bridge_port_remove(struct vmm_netswitch *nsw, 
			      struct vmm_netport *port)
{
	struct bridge_ctrl *br = nsw->priv;

	/* Cleanup mactable enteries for this port */
	bridge_mactable_cleanup_port(br, port);

	return VMM_OK;
}

static int bridge_probe(struct vmm_device *dev,
			const struct vmm_devtree_nodeid *nid)
{
	int rc = VMM_OK;
	struct bridge_ctrl *br;
	struct vmm_netswitch *nsw = NULL;

	nsw = vmm_netswitch_alloc(dev->node->name);
	if (!nsw) {
		rc = VMM_ENOMEM;
		goto bridge_netswitch_alloc_failed;
	}
	nsw->port2switch_xfer = bridge_rx_handler;
	nsw->port_add = bridge_port_add;
	nsw->port_remove = bridge_port_remove;

	dev->priv = nsw;

	br = vmm_zalloc(sizeof(struct bridge_ctrl));
	if (!br) {
		rc = VMM_ENOMEM;
		goto bridge_alloc_failed;
	}

	INIT_SPIN_LOCK(&br->mac_table_lock);
	INIT_LIST_HEAD(&br->mac_table);

	rc = vmm_netswitch_register(nsw, dev, br);
	if (rc) {
		goto bridge_netswitch_register_fail;
	}

	return VMM_OK;

bridge_netswitch_register_fail:
	vmm_free(br);
bridge_alloc_failed:
	vmm_netswitch_free(nsw);
bridge_netswitch_alloc_failed:
	return rc;
}

static int bridge_remove(struct vmm_device *dev)
{
	struct bridge_ctrl *br;
	struct vmm_netswitch *nsw = dev->priv;

	if (!nsw || !nsw->priv) {
		return VMM_ENODEV;
	}
	br = nsw->priv;

	bridge_mactable_flush(br);

	vmm_netswitch_unregister(nsw);

	vmm_free(br);

	vmm_netswitch_free(nsw);

	return VMM_OK;
}

static struct vmm_devtree_nodeid bridge_id_table[] = {
	{.type = "netswitch",.compatible = "bridge"},
	{ /* end of list */ },
};

static struct vmm_driver bridge = {
	.name = "bridge",
	.match_table = bridge_id_table,
	.probe = bridge_probe,
	.remove = bridge_remove,
};

int __init vmm_bridge_init(void)
{
	return vmm_devdrv_register_driver(&bridge);
}

void __exit vmm_bridge_exit(void)
{
	vmm_devdrv_unregister_driver(&bridge);
}

