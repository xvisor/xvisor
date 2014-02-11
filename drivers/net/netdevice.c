/**
 * Copyright (c) 2010 Himanshu Chauhan.
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
 * @file netdevice.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @author Pranav Sawargaonkar <pranav.sawargaonkar@gmail.com>
 * @brief Network Device framework source
 */

#include <linux/netdevice.h>

struct net_device *netdev_alloc(const char *name)
{
	struct net_device *ndev;

	ndev = vmm_zalloc(sizeof(struct net_device));

	if (!ndev) {
		vmm_printf("%s Failed to allocate net device\n", __func__);
		return NULL;
	}

	if (strlcpy(ndev->name, name, sizeof(ndev->name)) >=
	    sizeof(ndev->name)) {
		vmm_free(ndev);
		return NULL;
	}

	ndev->state = NETDEV_UNINITIALIZED;

	return ndev;
}

static int netdev_register_port(struct net_device *ndev)
{
        struct vmm_netport *port;
        const char *attr;
        struct vmm_netswitch *nsw;
	struct vmm_device *dev = ndev->vmm_dev;

        port = vmm_netport_alloc(ndev->name, VMM_NETPORT_DEF_QUEUE_SIZE);

        if (!port) {
                vmm_printf("Failed to allocate netport for %s\n", ndev->name);
                return VMM_ENOMEM;
        }

	port->dev.parent = dev;
        port->mtu = ndev->mtu;
        port->link_changed = netdev_set_link;
        port->can_receive = netdev_can_receive;
        port->switch2port_xfer = netdev_switch2port_xfer;
        port->priv = ndev;
        memcpy(port->macaddr, ndev->dev_addr, ETH_ALEN);

        ndev->nsw_priv = port;

        vmm_netport_register(port);

	if (dev) {
		if (vmm_devtree_read_string(dev->node,
					"switch", &attr) == VMM_OK) {
			nsw = vmm_netswitch_find(attr);
			if (!nsw) {
				vmm_panic("%s: Cannot find netswitch \"%s\"\n",
						ndev->name, attr);
			}
			vmm_netswitch_port_add(nsw, port);
		}
	}

        return VMM_OK;
}


int register_netdev(struct net_device *ndev)
{
	int rc = VMM_OK;

	if (ndev == NULL) {
		return VMM_EFAIL;
	}

	if (ndev->netdev_ops && ndev->netdev_ops->ndo_init) {
		rc = ndev->netdev_ops->ndo_init(ndev);
		if (rc != VMM_OK) {
			vmm_printf("%s: Device %s Failed during initializaion"
				   "with err %d!!!!\n", __func__ ,
				   ndev->name, rc);
			goto fail_ndev_reg;
		}
	}

	ndev->state &= ~NETDEV_UNINITIALIZED;
	ndev->state |= NETDEV_REGISTERED;

	rc = netdev_register_port(ndev);

	return rc;

fail_ndev_reg:
	return rc;
}

int netdev_unregister(struct net_device *ndev)
{
	int rc = VMM_OK;

	if (ndev == NULL) {
		return VMM_EFAIL;
	}

	ndev->state &= ~(NETDEV_REGISTERED | NETDEV_TX_ALLOWED);

	return rc;
}

void netdev_set_link(struct vmm_netport *port)
{
	struct net_device *dev = (struct net_device *)port->priv;

	if (port->flags & VMM_NETPORT_LINK_UP) {
		dev->netdev_ops->ndo_open(dev);
	} else {
		dev->netdev_ops->ndo_stop(dev);
	}
}

int netdev_can_receive(struct vmm_netport *port)
{
	struct net_device *dev = (struct net_device *) port->priv;

	if (netif_queue_stopped(dev))
		return 0;

	return 1;
}

int netdev_switch2port_xfer(struct vmm_netport *port,
		struct vmm_mbuf *mbuf)
{
	int rc = VMM_OK;
	struct net_device *dev = (struct net_device *) port->priv;
	char *buf;
	int len;

	if(mbuf->m_next) {
		/* Cannot avoid a copy in case of fragmented mbuf data */
		len = min(dev->mtu, (unsigned int)mbuf->m_pktlen);
		buf = vmm_malloc(len);
		m_copydata(mbuf, 0, len, buf);
		m_freem(mbuf);
		MGETHDR(mbuf, 0, 0);
		MEXTADD(mbuf, buf, len, 0, 0);
	}

	dev->netdev_ops->ndo_start_xmit(mbuf, dev);

	return rc;
}

struct net_device *alloc_etherdev(int sizeof_priv)
{
	struct net_device *ndev;

	ndev = vmm_zalloc(sizeof(struct net_device));

	if (!ndev) {
		vmm_printf("%s Failed to allocate net device\n", __func__);
		return NULL;
	}

	ndev->priv = (void *) vmm_zalloc(sizeof_priv);
	if (!ndev->priv) {
		vmm_printf("%s Failed to allocate ndev->priv of size %d\n",
				__func__, sizeof_priv);
		vmm_free(ndev);
		return NULL;
	}

	ndev->state = NETDEV_UNINITIALIZED;

	return ndev;
}

