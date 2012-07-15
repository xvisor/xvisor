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
 * @file vmm_netdev.h
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @author Pranav Sawargaonkar <pranav.sawargaonkar@gmail.com>
 * @brief Network Device framework header
 */

#ifndef __VMM_NETDEV_H_
#define __VMM_NETDEV_H_

#include <vmm_types.h>
#include <vmm_spinlocks.h>
#include <vmm_devdrv.h>
#include <vmm_error.h>
#include <net/vmm_mbuf.h>
#include <net/vmm_netport.h>

#define VMM_NETDEV_CLASS_NAME                  "netdev"

#define MAX_VMM_NETDEV_NAME_LEN			32
#define MAX_VMM_NDEV_HW_ADDRESS			32

#define VMM_ETH_ALEN        6               /* Octets in one ethernet addr   */
#define VMM_ETH_HLEN        14              /* Total octets in header.       */
#define VMM_ETH_ZLEN        60              /* Min. octets in frame sans FCS */
#define VMM_ETH_DATA_LEN    1500            /* Max. octets in payload        */
#define VMM_ETH_FRAME_LEN   1514            /* Max. octets in frame sans FCS */
#define VMM_ETH_FCS_LEN     4               /* Octets in the FCS             */

enum vmm_netdev_status {
	VMM_NETDEV_UNINITIALIZED = 0x1,
	VMM_NETDEV_REGISTERED = 0x2,
	VMM_NETDEV_TX_ALLOWED = 0x4,
};

enum vmm_netdev_link_state {
	VMM_NETDEV_STATE_NOCARRIER = 0,
	VMM_NETDEV_LINK_STATE_PRESENT,
};

struct vmm_netdev;

struct vmm_netdev_ops {
	int (*ndev_init) (struct vmm_netdev *ndev);
	int (*ndev_open) (struct vmm_netdev *ndev);
	int (*ndev_close) (struct vmm_netdev *ndev);
	int (*ndev_xmit) (struct vmm_mbuf *buf, struct vmm_netdev *ndev);
};

struct vmm_netdev_stats {
        unsigned long   rx_packets;             /* total packets received       */
        unsigned long   tx_packets;             /* total packets transmitted    */
        unsigned long   rx_bytes;               /* total bytes received         */
        unsigned long   tx_bytes;               /* total bytes transmitted      */
        unsigned long   rx_errors;              /* bad packets received         */
        unsigned long   tx_errors;              /* packet transmit problems     */
        unsigned long   rx_dropped;             /* no space in linux buffers    */
        unsigned long   tx_dropped;             /* no space available in linux  */
        unsigned long   multicast;              /* multicast packets received   */
        unsigned long   collisions;

        /* detailed rx_errors: */
        unsigned long   rx_length_errors;
        unsigned long   rx_over_errors;         /* receiver ring buff overflow  */
        unsigned long   rx_crc_errors;          /* recved pkt with crc error    */
        unsigned long   rx_frame_errors;        /* recv'd frame alignment error */
        unsigned long   rx_fifo_errors;         /* recv'r fifo overrun          */
        unsigned long   rx_missed_errors;       /* receiver missed packet       */

        /* detailed tx_errors */
        unsigned long   tx_aborted_errors;
        unsigned long   tx_carrier_errors;
        unsigned long   tx_fifo_errors;
        unsigned long   tx_heartbeat_errors;
        unsigned long   tx_window_errors;

        /* for cslip etc */
        unsigned long   rx_compressed;
        unsigned long   tx_compressed;
};

struct vmm_netdev {
	char name[MAX_VMM_NETDEV_NAME_LEN];
	struct vmm_device *dev;
	struct vmm_netdev_ops *dev_ops;
	unsigned int state;
	unsigned int link_state;
	void *priv;		/* Driver specific private data */
	void *nsw_priv;		/* VMM virtual packet switching layer
				 * specific private data.
				 */
	void *net_priv;		/* VMM specific private data -
				 * Usecase is currently undefined
				 */
	unsigned char dev_addr[MAX_VMM_NDEV_HW_ADDRESS];
	unsigned int hw_addr_len;
	unsigned int mtu;
	int irq;
	physical_addr_t base_addr;
	struct vmm_netdev_stats stats;
};

static inline int vmm_netif_carrier_ok(const struct vmm_netdev *dev)
{
	if (dev->link_state == VMM_NETDEV_LINK_STATE_PRESENT)
		return 1;
	else
		return 0;
}

static inline void vmm_netif_carrier_on(struct vmm_netdev *dev)
{
	dev->link_state = VMM_NETDEV_LINK_STATE_PRESENT;
}

static inline void vmm_netif_carrier_off(struct vmm_netdev *dev)
{
	dev->link_state = VMM_NETDEV_STATE_NOCARRIER;
}

static inline void vmm_netif_start_queue(struct vmm_netdev *dev)
{
	dev->state |= VMM_NETDEV_TX_ALLOWED;
}

static inline void vmm_netif_stop_queue(struct vmm_netdev *dev)
{
	dev->state &= ~VMM_NETDEV_TX_ALLOWED;
}

static inline void vmm_netif_wake_queue(struct vmm_netdev *dev)
{
	dev->state |= VMM_NETDEV_TX_ALLOWED;
}

static inline int vmm_netif_queue_stopped(struct vmm_netdev *dev)
{
	if (dev->state & VMM_NETDEV_TX_ALLOWED)
		return 0;
	else
		return 1;
}

static inline void vmm_ether_setup(struct vmm_netdev *dev)
{
	dev->hw_addr_len = VMM_ETH_ALEN;
	dev->mtu = VMM_ETH_DATA_LEN;
}

static inline void vmm_netdev_set_priv(struct vmm_netdev *ndev, void *priv)
{
	if (ndev && priv)
		ndev->priv = priv;
}

static inline void *vmm_netdev_get_priv(struct vmm_netdev *ndev)
{
	if (ndev)
		return ndev->priv;

	return NULL;
}

static inline int vmm_netif_rx(struct vmm_mbuf *mb, struct vmm_netdev *dev)
{
	struct vmm_netport *port = dev->nsw_priv;

	if (!port) {
		vmm_printf("%s Net dev %s has no switch attached\n",
				__func__, dev->name);
		m_freem(mb);
		return VMM_EINVALID;
	}

	vmm_port2switch_xfer(port, mb);

	return VMM_OK;
}

/** Allocate new network device */
struct vmm_netdev *vmm_netdev_alloc(const char *name);

/** Register network device to device driver framework */
int vmm_netdev_register(struct vmm_netdev *ndev);

/** Unregister network device from device driver framework */
int vmm_netdev_unregister(struct vmm_netdev * ndev);

/** Find a network device in device driver framework */
struct vmm_netdev *vmm_netdev_find(const char *name);

/** Get network device with given number */
struct vmm_netdev *vmm_netdev_get(int num);

/** Count number of network devices */
u32 vmm_netdev_count(void);

void vmm_netdev_set_link(struct vmm_netport *port);
int vmm_netdev_can_receive(struct vmm_netport *port);
int vmm_netdev_switch2port_xfer(struct vmm_netport *port,
			struct vmm_mbuf *mbuf);


#endif /* __VMM_NETDEV_H_ */
