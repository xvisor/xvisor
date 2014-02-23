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
#include <net/vmm_protocol.h>
#include <net/vmm_mbuf.h>
#include <net/vmm_netswitch.h>
#include <net/vmm_netport.h>
#include <libs/stringlib.h>

#undef DEBUG_BRIDGE

#ifdef DEBUG_BRIDGE
#define DPRINTF(fmt, ...) \
	do { vmm_printf(fmt , ## __VA_ARGS__); } while (0)
#else
#define DPRINTF(fmt, ...) do {} while(0)
#endif

#define BRIDGE_MAC_TABLE_SZ	32
#define BRIDGE_MAC_EXPIRY	30000000000LLU

/* We maintain a table of learned mac addresses 
 * (please note that the mac of the immediate netports are not 
 * kept in this table) */
struct bridge_mac_entry {
	struct vmm_netport *port;
	u8 macaddr[6];
	u64 timestamp;
};

struct bridge_ctrl {
	struct vmm_netswitch *nsw;
	struct vmm_timer_event ev;
	vmm_rwlock_t mac_table_lock;
	u32 mac_table_sz;
	struct bridge_mac_entry *mac_table;
};

static void bridge_mactable_cleanup_port(struct bridge_ctrl *br,
					 struct vmm_netport *port)
{
	u32 m;
	irq_flags_t f;

	vmm_write_lock_irqsave_lite(&br->mac_table_lock, f);
	for (m = 0; m < br->mac_table_sz; m++) {
		if (br->mac_table[m].port == port) {
			br->mac_table[m].port = NULL;
		}
	}
	vmm_write_unlock_irqrestore_lite(&br->mac_table_lock, f);
}

static struct vmm_netport *bridge_mactable_learn_find(struct bridge_ctrl *br,
						      const u8 *dstmac,
						      const u8 *srcmac,
						      struct vmm_netport *src)
{
	u32 i;
	u64 tstamp;
	irq_flags_t f;
	bool learn, update, found;
	struct vmm_netport *dst;
	struct bridge_mac_entry *m;

	/* Acquire read lock */
	vmm_read_lock_irqsave_lite(&br->mac_table_lock, f);

	/* Check for for dstmac and whether we need
	 * to Learn (srcmac, src) mapping ??
	 */
	learn = TRUE;
	dst = NULL;
	for (i = 0; i < br->mac_table_sz; i++) {
		m = &br->mac_table[i];
		/* If mac table entry is unused then continue */
		if (!m->port) {
			continue;
		}
		/* Match (srcmac, srcport) */
		if (learn &&
		    !compare_ether_addr(m->macaddr, srcmac) &&
		    (m->port == src)) {
			learn = FALSE;
		}
		/* Match (dstmac) */
		if (!dst &&
		    !compare_ether_addr(m->macaddr, dstmac)) {
			dst = m->port;
		}
		/* If no need to learn and found
		 * destination port then break
		 */
		if (!learn && dst) {
			break;
		}
	}

	/* Release read lock */
	vmm_read_unlock_irqrestore_lite(&br->mac_table_lock, f);

	/* If leaning required then update mac table */
	if (learn) {
		/* Retrive current timestamp */
		tstamp = vmm_timer_timestamp();

		/* Acquire write lock */
		vmm_write_lock_irqsave_lite(&br->mac_table_lock, f);

		/* If mac entry already exist then
		 * update only port and timestamp
		 */
		update = TRUE;
		for (i = 0; i < br->mac_table_sz; i++) {
			m = &br->mac_table[i];
			if (!compare_ether_addr(m->macaddr, srcmac)) {
				m->port = src;
				m->timestamp = tstamp;
				update = FALSE;
				break;
			}
		}

		/* If mac entry does not exist then
		 * save (mac, port, timestamp) in a
		 * free mac table entry.
		 */
		if (update) {
			found = FALSE;
			for (i = 0; i < br->mac_table_sz; i++) {
				m = &br->mac_table[i];
				if (m->port == NULL) {
					found = TRUE;
					break;
				}
			}
			if (found) {
				m->port = src;
				memcpy(m->macaddr, srcmac, 6);
				m->timestamp = tstamp;
			}
		}

		/* Release write lock */
		vmm_write_unlock_irqrestore_lite(&br->mac_table_lock, f);
	}

	return dst;
}

static void bridge_timer_event(struct vmm_timer_event *ev)
{
	u32 i;
	u64 tstamp;
	irq_flags_t f;
	struct bridge_ctrl *br = ev->priv;
	struct bridge_mac_entry *m;

	DPRINTF("%s: bridge expiry event nsw=%s\n",
		__func__, br->nsw->name);

	/* Retrive current timestamp */
	tstamp = vmm_timer_timestamp();

	/* Acquire write lock */
	vmm_write_lock_irqsave_lite(&br->mac_table_lock, f);

	/* Purge old enteries */
	for (i = 0; i < br->mac_table_sz; i++) {
		m = &br->mac_table[i];
		if (m->port &&
		    ((m->timestamp - tstamp) > BRIDGE_MAC_EXPIRY)) {
			DPRINTF("%s: purge port=%s\n",
				__func__, m->port->name);
			m->port = NULL;
			memset(m->macaddr, 0, 6);
			m->timestamp = 0;
		}
	}

	/* Release write lock */
	vmm_write_unlock_irqrestore_lite(&br->mac_table_lock, f);

	/* Again start the bridge timer event */
	vmm_timer_event_start(&br->ev, BRIDGE_MAC_EXPIRY);
}

/**
 *  Thread body responsible for sending the RX buffer packets
 *  to the destination port(s)
 */
static int bridge_rx_handler(struct vmm_netswitch *nsw,
			     struct vmm_netport *src,
			     struct vmm_mbuf *mbuf)
{
	irq_flags_t f;
	const u8 *srcmac, *dstmac;
	bool broadcast = TRUE;
	struct dlist *l, *l1;
	struct vmm_netport *dst, *port;
	struct bridge_ctrl *br = nsw->priv;

	/* Get source and destination mac addresses */
	srcmac = ether_srcmac(mtod(mbuf, u8 *));
	dstmac = ether_dstmac(mtod(mbuf, u8 *));

	/* Learn source mac address and find port
	 * matching destination mac address
	 */
	dst = bridge_mactable_learn_find(br, dstmac, srcmac, src);

	/* If the frame below cases then it should be unicast.
	 * 
	 * Case 1: destination MAC address is not broadcast address
	 * Case 2: We found port matching destination mac address
	 */
	if (!is_broadcast_ether_addr(dstmac) && dst) {
		/* Find port fordestination mac address */
		broadcast = FALSE;
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

	nsw = vmm_netswitch_alloc(dev->name);
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

	br->nsw = nsw;
	INIT_TIMER_EVENT(&br->ev, bridge_timer_event, br);
	INIT_RW_LOCK(&br->mac_table_lock);
	br->mac_table_sz = BRIDGE_MAC_TABLE_SZ;
	br->mac_table = vmm_zalloc(sizeof(struct bridge_mac_entry) *
				   br->mac_table_sz);
	if (!br->mac_table) {
		rc = VMM_ENOMEM;
		goto bridge_alloc_mac_table_fail;
	}

	rc = vmm_netswitch_register(nsw, dev, br);
	if (rc) {
		goto bridge_netswitch_register_fail;
	}

	vmm_timer_event_start(&br->ev, BRIDGE_MAC_EXPIRY);

	return VMM_OK;

bridge_netswitch_register_fail:
bridge_alloc_mac_table_fail:
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

	vmm_timer_event_stop(&br->ev);

	vmm_netswitch_unregister(nsw);

	vmm_free(br->mac_table);
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

