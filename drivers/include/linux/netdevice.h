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
 * @file netdevice.h
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @author Pranav Sawargaonkar <pranav.sawargaonkar@gmail.com>
 * @brief Network Device framework header
 */

#ifndef __LINUX_NETDEVICE_H_
#define __LINUX_NETDEVICE_H_

#include <vmm_types.h>
#include <vmm_devdrv.h>
#include <vmm_error.h>
#include <net/vmm_protocol.h>
#include <net/vmm_net.h>
#include <net/vmm_netport.h>
#include <net/vmm_netswitch.h>
#include <linux/interrupt.h>
#include <linux/skbuff.h>

#define MAX_NETDEV_NAME_LEN			32
#define MAX_NDEV_HW_ADDRESS			32

#undef ETH_HLEN

#define ETH_ALEN	6	/* Octets in one ethernet addr	*/
#define ETH_HLEN	14	/* Total octets in header.	*/
#define ETH_ZLEN	60	/* Min. octets in frame sans FCS */
#define ETH_DATA_LEN	1500	/* Max. octets in payload	*/
#define ETH_FRAME_LEN	1514	/* Max. octets in frame sans FCS */
#define ETH_FCS_LEN	4	/* Octets in the FCS		*/

#define	net_ratelimit()	1
#define NETIF_MSG_LINK	0

enum netdev_status {
	NETDEV_UNINITIALIZED = 0x1,
	NETDEV_REGISTERED = 0x2,
	NETDEV_TX_ALLOWED = 0x4,
};

enum netdev_link_state {
	NETDEV_STATE_NOCARRIER = 0,
	NETDEV_LINK_STATE_PRESENT,
};

/* Driver transmit return codes */
enum netdev_tx {
	NETDEV_TX_OK = 0,	/* driver took care of packet */
	NETDEV_TX_BUSY,		/* driver tx path was busy*/
	NETDEV_TX_LOCKED = -1,  /* driver tx lock was already taken */
};

typedef enum netdev_tx netdev_tx_t;

struct net_device;

struct net_device_ops {
	int (*ndo_init) (struct net_device *ndev);
	int (*ndo_open) (struct net_device *ndev);
	int (*ndo_stop) (struct net_device *ndev);
	int (*ndo_start_xmit) (struct sk_buff *buf, struct net_device *ndev);
};

struct net_device_stats {
	unsigned long	rx_packets;		/* total packets received	*/
	unsigned long	tx_packets;		/* total packets transmitted	*/
	unsigned long	rx_bytes;		/* total bytes received		*/
	unsigned long	tx_bytes;		/* total bytes transmitted	*/
	unsigned long	rx_errors;		/* bad packets received		*/
	unsigned long	tx_errors;		/* packet transmit problems	*/
	unsigned long	rx_dropped;		/* no space in linux buffers	*/
	unsigned long	tx_dropped;		/* no space available in linux  */
	unsigned long	multicast;		/* multicast packets received	*/
	unsigned long	collisions;

	/* detailed rx_errors: */
	unsigned long	rx_length_errors;
	unsigned long	rx_over_errors;		/* receiver ring buff overflow  */
	unsigned long	rx_crc_errors;		/* recved pkt with crc error	*/
	unsigned long	rx_frame_errors;	/* recv'd frame alignment error */
	unsigned long	rx_fifo_errors;		/* recv'r fifo overrun		*/
	unsigned long	rx_missed_errors;	/* receiver missed packet	*/

	/* detailed tx_errors */
	unsigned long	tx_aborted_errors;
	unsigned long	tx_carrier_errors;
	unsigned long	tx_fifo_errors;
	unsigned long	tx_heartbeat_errors;
	unsigned long	tx_window_errors;

	/* for cslip etc */
	unsigned long	rx_compressed;
	unsigned long	tx_compressed;
};

struct net_device {
	char name[MAX_NETDEV_NAME_LEN];
	struct vmm_device *dev;
	const struct net_device_ops *netdev_ops;
	unsigned int state;
	unsigned int link_state;
	void *priv;		/* Driver specific private data */
	void *nsw_priv;		/* VMM virtual packet switching layer
				 * specific private data.
				 */
	void *net_priv;		/* VMM specific private data -
				 * Usecase is currently undefined
				 */
	unsigned char dev_addr[MAX_NDEV_HW_ADDRESS];
	unsigned int hw_addr_len;
	unsigned int mtu;
	int irq;
	physical_addr_t base_addr;
	unsigned char	dma;	/* DMA channel		*/
	struct net_device_stats stats;
	struct vmm_device *vmm_dev;
};

static inline int netif_carrier_ok(const struct net_device *dev)
{
	if (dev->link_state == NETDEV_LINK_STATE_PRESENT)
		return 1;
	else
		return 0;
}

static inline void netif_carrier_on(struct net_device *dev)
{
	dev->link_state = NETDEV_LINK_STATE_PRESENT;
}

static inline void netif_carrier_off(struct net_device *dev)
{
	dev->link_state = NETDEV_STATE_NOCARRIER;
}

static inline void netif_start_queue(struct net_device *dev)
{
	dev->state |= NETDEV_TX_ALLOWED;
}

static inline void netif_stop_queue(struct net_device *dev)
{
	dev->state &= ~NETDEV_TX_ALLOWED;
}

static inline void netif_wake_queue(struct net_device *dev)
{
	dev->state |= NETDEV_TX_ALLOWED;
}

static inline int netif_queue_stopped(struct net_device *dev)
{
	if (dev->state & NETDEV_TX_ALLOWED)
		return 0;
	else
		return 1;
}

static inline void ether_setup(struct net_device *dev)
{
	dev->hw_addr_len = ETH_ALEN;
	dev->mtu = ETH_DATA_LEN;
}

static inline void netdev_set_priv(struct net_device *ndev, void *priv)
{
	if (ndev && priv)
		ndev->priv = priv;
}

static inline void *netdev_priv(struct net_device *ndev)
{
	if (ndev)
		return ndev->priv;

	return NULL;
}

static inline int netif_rx(struct sk_buff *mb, struct net_device *dev)
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

#define netif_msg_link(x)	0
#define SET_NETDEV_DEV(ndev, pdev) ndev->vmm_dev = (void *) pdev

/** Allocate new network device */
struct net_device *netdev_alloc(const char *name);

/** Register network device to device driver framework */
int register_netdev(struct net_device *ndev);

/** Unregister network device from device driver framework */
int netdev_unregister(struct net_device * ndev);

void netdev_set_link(struct vmm_netport *port);
int netdev_can_receive(struct vmm_netport *port);
int netdev_switch2port_xfer(struct vmm_netport *port,
			struct vmm_mbuf *mbuf);
struct net_device *alloc_etherdev(int sizeof_priv);

#endif /* __LINUX_NETDEVICE_H_ */
