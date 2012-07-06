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
 * @file net_bridge.c
 * @author Sukanto Ghosh <sukantoghosh@gmail.com>
 * @brief Default packet switch implementation.
 */

#include <vmm_heap.h>
#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_string.h>
#include <vmm_devdrv.h>
#include <vmm_threads.h>
#include <vmm_modules.h>
#include <list.h>
#include <net/vmm_protocol.h>
#include <net/vmm_mbuf.h>
#include <net/vmm_netswitch.h>
#include <net/vmm_netport.h>

#undef DEBUG_NETBRIDGE

#ifdef DEBUG_NETBRIDGE
#define DPRINTF(fmt, ...) \
	do { vmm_printf(fmt , ## __VA_ARGS__); } while (0)
#else
#define DPRINTF(fmt, ...) do {} while(0)
#endif

#define VMM_NETBRIDGE_RXQ_LEN	20

/**
 *  Thread body responsible for sending the RX buffer packets
 *  to the destination port(s)
 */
static int vmm_netbridge_rx_handler(struct vmm_netport *src_port,
				    struct vmm_mbuf *mbuf)
{
	struct dlist *l;
	bool broadcast;
	const u8 *srcmac, *dstmac;
	struct vmm_netport *dst_port;
	struct vmm_netswitch *nsw;

	broadcast = TRUE;

	nsw = src_port->nsw;
	srcmac = ether_srcmac(mtod(mbuf, u8 *));
	dstmac = ether_dstmac(mtod(mbuf, u8 *));

	/* TODO: Learning not required as it is assumed that every port notifies
	 * its macaddr change, multiple macs will still work because of
	 * broadcast (but need to optimize such case)  */

	/* Find if the frame should be unicast because it satisfies both of these:
	 * Case 1: It is not a broadcast MAC address, and
	 * Case 2: We do have the MAC->port mapping
	 */
	if(!is_broadcast_ether_addr(dstmac)) {
		list_for_each(l, &nsw->port_list) {
			   if(!compare_ether_addr(list_port(l)->macaddr, dstmac)) {
				DPRINTF("%s: port->macaddr[%s]\n", __func__,
					ethaddr_to_str(tname, list_port(l)->macaddr));
				dst_port = list_port(l);
				broadcast = FALSE;
				break;
			   }
		}
	}		

	if(broadcast) {
		DPRINTF("%s: broadcasting\n", __func__);
		list_for_each(l, &nsw->port_list) {
			   if(compare_ether_addr(list_port(l)->macaddr, srcmac)) {
				if(list_port(l)->can_receive &&
				   !list_port(l)->can_receive(list_port(l))) {
				   continue;
				}
				MADDREFERENCE(mbuf);
				MCLADDREFERENCE(mbuf);
				list_port(l)->switch2port_xfer(list_port(l), mbuf);
			   }
		}
		m_freem(mbuf);
	} else {
		DPRINTF("%s: unicasting to \"%s\"\n", __func__, dst_port->name);
		if(!dst_port->can_receive || dst_port->can_receive(dst_port)) {
			dst_port->switch2port_xfer(dst_port, mbuf);
		} else {
			/* Free the mbuf if destination cannot do rx */
			m_freem(mbuf);
		}
	}

	return VMM_OK;
}

static int vmm_netbridge_probe(struct vmm_device *dev,
			       const struct vmm_devid *devid)
{
	struct vmm_netswitch *nsw = NULL;
	int rc = VMM_OK;

	nsw = vmm_netswitch_alloc(dev->node->name,
		       		  VMM_NETBRIDGE_RXQ_LEN,	
				  VMM_THREAD_DEF_PRIORITY, 
				  VMM_THREAD_DEF_TIME_SLICE);
	if(!nsw) {
		rc = VMM_EFAIL;
		goto vmm_netbridge_probe_failed;
	}
	nsw->port2switch_xfer = vmm_netbridge_rx_handler;

	dev->priv = nsw;

	if(vmm_netswitch_register(nsw, dev, NULL) == VMM_OK) {
		goto vmm_netbridge_probe_done;
	}

vmm_netbridge_probe_failed:
	if(nsw) {
		vmm_netswitch_free(nsw);
	}
vmm_netbridge_probe_done:
	return rc;
}

static int vmm_netbridge_remove(struct vmm_device *dev)
{
	struct vmm_netswitch *nsw;
        nsw = dev->priv;
	if(nsw) {
		vmm_netswitch_unregister(nsw);
		vmm_netswitch_free(nsw);
	}
	return VMM_OK;
}

static struct vmm_devid def_netswitch_devid_table[] = {
	{.type = "netswitch",.compatible = "bridge"},
	{ /* end of list */ },
};

static struct vmm_driver net_bridge = {
	.name = "netbridge",
	.match_table = def_netswitch_devid_table,
	.probe = vmm_netbridge_probe,
	.remove = vmm_netbridge_remove,
};

int __init vmm_netbridge_init(void)
{
	return vmm_devdrv_register_driver(&net_bridge);
}

void vmm_netbridge_exit(void)
{
	vmm_devdrv_unregister_driver(&net_bridge);
}

