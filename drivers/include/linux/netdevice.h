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
#include <linux/device.h>
#include <uapi/linux/if.h>
#include <uapi/linux/if_ether.h>

#define MAX_NETDEV_NAME_LEN			32
#define MAX_NDEV_HW_ADDRESS			32

#define	net_ratelimit()	1
#define NETIF_MSG_LINK	0

enum netdev_status {
	NETDEV_UNINITIALIZED = 0x1,
	NETDEV_REGISTERED = 0x2,
	NETDEV_OPEN	  = 0x4,
	NETDEV_TX_ALLOWED = 0x8,
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

struct phy_device;
struct net_device;

struct netdev_queue {
	struct net_device	*ndev;
};

struct net_device_ops {
	int (*ndo_init) (struct net_device *ndev);
	int (*ndo_open) (struct net_device *ndev);
	int (*ndo_stop) (struct net_device *ndev);
	int (*ndo_start_xmit) (struct sk_buff *buf, struct net_device *ndev);
	int (*ndo_validate_addr)(struct net_device *dev);
	void (*ndo_tx_timeout) (struct net_device *dev);
	int (*ndo_do_ioctl)(struct net_device *dev,
			    struct ifreq *ifr, int cmd);
	int (*ndo_change_mtu)(struct net_device *dev, int new_mtu);
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
	const struct ethtool_ops *ethtool_ops;
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
	unsigned int flags;
	unsigned long last_rx;
	u32 irq;
	physical_addr_t base_addr;
	unsigned char	dma;	/* DMA channel		*/
	struct net_device_stats stats;
	struct phy_device *phydev;
	unsigned long		trans_start;
	int			watchdog_timeo;

	struct vmm_device *vmm_dev;
	struct netdev_queue _tx;
};

/*
 * Structure for NAPI scheduling similar to tasklet but with weighting
 */
struct napi_struct {
#if 0
	/* The poll_list must only be managed by the entity which
	 * changes the state of the NAPI_STATE_SCHED bit.  This means
	 * whoever atomically sets that bit can add this napi_struct
	 * to the per-cpu poll_list, and whoever clears that bit
	 * can remove from the list right before clearing the bit.
	 */
	struct list_head	poll_list;

	unsigned long		state;
	int			weight;
	unsigned int		gro_count;
#endif /* 0 */
	int			(*poll)(struct napi_struct *, int);
#if 0
#ifdef CONFIG_NETPOLL
	spinlock_t		poll_lock;
	int			poll_owner;
#endif
#endif /* 0 */
	struct net_device	*dev;
#if 0
	struct sk_buff		*gro_list;
	struct sk_buff		*skb;
	struct list_head	dev_list;
	struct hlist_node	napi_hash_node;
	unsigned int		napi_id;
#else /* 0 */
	struct vmm_netport_xfer	xfer;
#endif /* 0 */
};

enum gro_result {
	GRO_MERGED,
	GRO_MERGED_FREE,
	GRO_HELD,
	GRO_NORMAL,
	GRO_DROP,
};
typedef enum gro_result gro_result_t;

#define NETDEV_ALIGN            32

#include <linux/ethtool.h>

/* No harm in enabling these debug messages. */
#define	netif_msg_ifup(db)		1
#define	netif_msg_ifdown(db)		1
#define	netif_msg_timer(db)		1
#define	netif_msg_rx_err(db)		1
#define	netif_msg_tx_err(db)		1

/* These debug messages will throw too may prints, disabling them by default */
#define	netif_msg_intr(db)		0
#define	netif_msg_tx_done(db)		0
#define	netif_msg_rx_status(db)		0
#define	netif_msg_tx_queued(db)		0

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

static inline bool netif_running(struct net_device *dev)
{
	if (dev->state & NETDEV_OPEN)
		return true;

	return false;
}

/* Multi-queue is not managed yet */
#define netif_tx_wake_all_queues(dev)	netif_wake_queue(dev)
#define netif_tx_start_all_queues(dev)	netif_start_queue(dev)

/**
 *	netif_device_present - is device available or removed
 *	@dev: network device
 *
 * Check if device has not been removed from system.
 */
static inline bool netif_device_present(struct net_device *dev)
{
	return (NETDEV_REGISTERED & dev->state);
}

static inline void ether_setup(struct net_device *dev)
{
	dev->hw_addr_len = ETH_ALEN;
	dev->mtu = ETH_DATA_LEN;
}

static inline int eth_change_mtu(struct net_device *dev, int new_mtu)
{
	if (new_mtu < 68 || new_mtu > ETH_DATA_LEN)
		return -EINVAL;
	dev->mtu = new_mtu;
	return 0;
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

	vmm_port2switch_xfer_mbuf(port, mb);

	return VMM_OK;
}

static inline void free_netdev(struct net_device *dev)
{
	vmm_free(dev);
}

static inline
struct netdev_queue *netdev_get_tx_queue(struct net_device *dev,
					 unsigned int index)
{
	return &dev->_tx;
}

static inline void netif_tx_stop_queue(struct netdev_queue *dev_queue)
{
	netif_stop_queue(dev_queue->ndev);
}

static inline void netif_tx_wake_queue(struct netdev_queue *dev_queue)
{
	netif_wake_queue(dev_queue->ndev);
}

#define	netif_msg_link(x)		0
#define	SET_NETDEV_DEV(ndev, pdev)	ndev->vmm_dev = (void *) pdev
#define	unregister_netdev(ndev)		netdev_unregister(ndev)

/** Allocate new network device */
struct net_device *netdev_alloc(const char *name);

struct net_device *alloc_netdev_mqs(int sizeof_priv, const char *name,
		unsigned char name_assign_type,
		void (*setup)(struct net_device *),
		unsigned int txqs, unsigned int rxqs);

/** Register network device to device driver framework */
int register_netdev(struct net_device *ndev);

/** Unregister network device from device driver framework */
int netdev_unregister(struct net_device * ndev);

void netdev_set_link(struct vmm_netport *port);
int netdev_can_receive(struct vmm_netport *port);
int netdev_switch2port_xfer(struct vmm_netport *port,
			struct vmm_mbuf *mbuf);
struct net_device *alloc_etherdev(int sizeof_priv);

/* Default NAPI poll() weight
 * Device drivers are strongly advised to not use bigger value
 */
#define NAPI_POLL_WEIGHT 64
extern int              netdev_budget;

/**
 *	netif_napi_add - initialize a napi context
 *	@dev:  network device
 *	@napi: napi context
 *	@poll: polling function
 *	@weight: default weight
 *
 * netif_napi_add() must be used to initialize a napi context prior to calling
 * *any* of the other napi related functions.
 */
void netif_napi_add(struct net_device *dev, struct napi_struct *napi,
		    int (*poll)(struct napi_struct *, int), int weight);

/**
 *  netif_napi_del - remove a napi context
 *  @napi: napi context
 *
 *  netif_napi_del() removes a napi context from the network device napi list
 */
void netif_napi_del(struct napi_struct *napi);

/**
 *	napi_complete - NAPI processing complete
 *	@n: napi context
 *
 * Mark NAPI processing as complete.
 */
void __napi_complete(struct napi_struct *n);
void napi_complete(struct napi_struct *n);

/**
 *	napi_disable - prevent NAPI from scheduling
 *	@n: napi context
 *
 * Stop NAPI from being scheduled on this context.
 * Waits till any outstanding processing completes.
 */
void napi_disable(struct napi_struct *n);

/**
 *	napi_enable - enable NAPI scheduling
 *	@n: napi context
 *
 * Resume NAPI from being scheduled on this context.
 * Must be paired with napi_disable.
 */
void napi_enable(struct napi_struct *n);

/**
 *	napi_schedule - schedule NAPI poll
 *	@n: napi context
 *
 * Schedule NAPI poll routine to be called if it is not already
 * running.
 */
void napi_schedule(struct napi_struct *n);

static inline gro_result_t napi_gro_receive(struct napi_struct *napi,
					    struct sk_buff *skb)
{
	return netif_rx(skb, napi->dev);
}

#endif /* __LINUX_NETDEVICE_H_ */
