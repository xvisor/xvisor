/**
 * Copyright (C) 2017 Ashutosh Sharma.
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
 * @file smsc95xx.c
 * @author Ashutosh Sharma
 * @brief USB network device driver
 *
 * This source is largely adapted from u-boot sources:
 * <u-boot>/drivers/usb/eth/smsc95xx.c
 *
 * Copyright (c) 2015 Google, Inc
 * Copyright (c) 2011 The Chromium OS Authors.
 * Copyright (C) 2009 NVIDIA, Corporation
 * Copyright (C) 2007-2008 SMSC (Steve Glendinning)
 *
 * SPDX-License-Identifier:     GPL-2.0+
 */

#include <asm/errno.h>
#include <linux/mii.h>
#include <linux/spinlock.h>
#include <linux/skbuff.h>
#include <linux/if_ether.h>
#include <linux/netdevice.h>
#include <linux/ethtool.h>
#include <linux/io.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/delay.h>

#include <linux/clk.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/mii.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_mdio.h>
#include <linux/of_net.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/phy.h>

#include <libs/unaligned.h>

#include <drv/usb.h>


#undef DEBUG

#if defined(DEBUG)
#define DPRINTF(msg...)	vmm_printf(msg)
#else
#define DPRINTF(msg...)
#endif

#define MODULE_DESC		"USB Network Driver"
#define MODULE_AUTHOR		"Ashutosh Sharma"
#define MODULE_LICENSE		"GPL"
#define MODULE_IPRIORITY	(VMM_NET_CLASS_IPRIORITY + \
		                         USB_CORE_IPRIORITY + 1)
#define	MODULE_INIT		smsc95xx_driver_init
#define	MODULE_EXIT		smsc95xx_driver_exit

#define DRV_NAME                "smsc95xx"
#define DRV_VERSION             "0.01"

/* SMSC LAN95xx based USB 2.0 Ethernet Devices */
/* LED defines */
#define LED_GPIO_CFG            (0x24)
#define LED_GPIO_CFG_SPD_LED    (0x01000000)
#define LED_GPIO_CFG_LNK_LED    (0x00100000)
#define LED_GPIO_CFG_FDX_LED    (0x00010000)

/* Tx command words */
#define TX_CMD_A_FIRST_SEG_     0x00002000
#define TX_CMD_A_LAST_SEG_      0x00001000

/* Rx status word */
#define RX_STS_FL_              0x3FFF0000      /* Frame Length */
#define RX_STS_ES_              0x00008000      /* Error Summary */

/* SCSRs */
#define ID_REV                  0x00

#define INT_STS                 0x08

#define TX_CFG                  0x10
#define TX_CFG_ON_              0x00000004

#define HW_CFG                  0x14
#define HW_CFG_BIR_             0x00001000
#define HW_CFG_RXDOFF_          0x00000600
#define HW_CFG_MEF_             0x00000020
#define HW_CFG_BCE_             0x00000002
#define HW_CFG_LRST_            0x00000008

#define PM_CTRL                 0x20
#define PM_CTL_PHY_RST_         0x00000010

#define AFC_CFG                 0x2C

/*
 * Hi watermark = 15.5Kb (~10 mtu pkts)
 * low watermark = 3k (~2 mtu pkts)
 * backpressure duration = ~ 350us
 * Apply FC on any frame.
 */
#define AFC_CFG_DEFAULT         0x00F830A1

#define E2P_CMD                 0x30
#define E2P_CMD_BUSY_           0x80000000
#define E2P_CMD_READ_           0x00000000
#define E2P_CMD_TIMEOUT_        0x00000400
#define E2P_CMD_LOADED_         0x00000200
#define E2P_CMD_ADDR_           0x000001FF

#define E2P_DATA                0x34

#define BURST_CAP               0x38

#define INT_EP_CTL              0x68
#define INT_EP_CTL_PHY_INT_     0x00008000

#define BULK_IN_DLY             0x6C

/* MAC CSRs */
#define MAC_CR                  0x100
#define MAC_CR_MCPAS_           0x00080000
#define MAC_CR_PRMS_            0x00040000
#define MAC_CR_HPFILT_          0x00002000
#define MAC_CR_TXEN_            0x00000008
#define MAC_CR_RXEN_            0x00000004

#define ADDRH                   0x104

#define ADDRL                   0x108

#define MII_ADDR                0x114
#define MII_WRITE_              0x02
#define MII_BUSY_               0x01
#define MII_READ_               0x00 /* ~of MII Write bit */

#define MII_DATA                0x118

#define FLOW                    0x11C

#define VLAN1                   0x120

#define COE_CR                  0x130
#define Tx_COE_EN_              0x00010000
#define Rx_COE_EN_              0x00000001


/* Vendor-specific PHY Definitions */
#define PHY_INT_SRC                     29

#define PHY_INT_MASK                    30
#define PHY_INT_MASK_ANEG_COMP_         ((u16)0x0040)
#define PHY_INT_MASK_LINK_DOWN_         ((u16)0x0010)
#define PHY_INT_MASK_DEFAULT_           (PHY_INT_MASK_ANEG_COMP_ | \
		PHY_INT_MASK_LINK_DOWN_)

/* USB Vendor Requests */
#define USB_VENDOR_REQUEST_WRITE_REGISTER       0xA0
#define USB_VENDOR_REQUEST_READ_REGISTER        0xA1

/* Some extra defines */
#define HS_USB_PKT_SIZE                 512
#define FS_USB_PKT_SIZE                 64
/* 5/33 is lower limit for BURST_CAP to work */
#define DEFAULT_HS_BURST_CAP_SIZE       (5 * HS_USB_PKT_SIZE)
#define DEFAULT_FS_BURST_CAP_SIZE       (33 * FS_USB_PKT_SIZE)
#define DEFAULT_BULK_IN_DELAY           0x00002000
#define MAX_SINGLE_PACKET_SIZE          2048
#define EEPROM_MAC_OFFSET               0x01
#define SMSC95XX_INTERNAL_PHY_ID        1
#define ETH_P_8021Q                     0x8100          /* 802.1Q VLAN Extended Header  */

/* local defines */
#define USB_CTRL_SET_TIMEOUT    5000
#define USB_CTRL_GET_TIMEOUT    5000
#define USB_BULK_SEND_TIMEOUT   5000
#define USB_BULK_RECV_TIMEOUT   5000

#define RX_URB_SIZE DEFAULT_HS_BURST_CAP_SIZE
#define PHY_CONNECT_TIMEOUT     5000

#define PKTSIZE                 1522
#define PKTSIZE_ALIGN           1536

enum thread_state {
	task_stop = 0,
	task_running,
	task_terminate,
};

struct usb_net_device {
	struct usb_device    *udev;
	struct usb_interface *intf;
	struct net_device    *ndev;

	int phy_id;                     /* mii phy id */
	int have_hwaddr;
	u32 mac_cr;                     /* MAC control register value */
	size_t rx_urb_size;  		/* maximum USB URB size */

	struct vmm_thread*  rx_thread;
	enum   thread_state rx_thread_state;

	unsigned char ep_in;		/* in endpoint */
	unsigned char ep_out;		/* out ....... */
	unsigned char ep_int;		/* interrupt . */
	unsigned int irqpipe;	 	/* pipe for release_irq */
	unsigned char irqmaxp;		/* max packed for irq Pipe */
	unsigned char irqinterval;	/* Intervall for IRQ Pipe */
};


static vmm_spinlock_t unet_lock;

/*
 * Smsc95xx infrastructure commands
 */
static int smsc95xx_write_reg(struct usb_device *udev, u32 index, u32 data)
{
	int rc, len;
	u32 __cacheline_aligned tmpbuf;

	tmpbuf = vmm_cpu_to_le32(data);

	rc = usb_control_msg(udev, usb_sndctrlpipe(udev, 0),
			USB_VENDOR_REQUEST_WRITE_REGISTER,
			USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
			0, index, &tmpbuf, sizeof(data), &len,
			USB_CTRL_SET_TIMEOUT);

	if(rc != VMM_OK)
		return rc;

	if (len != sizeof(data)) {
		vmm_printf("smsc95xx_write_reg failed: index=%d, data=%d, len=%d",
				index, data, len);
		return VMM_EIO;
	}
	return VMM_OK;
}

static int smsc95xx_read_reg(struct usb_device *udev, u32 index, u32 *data)
{
	int len;
	u32 __cacheline_aligned tmpbuf;

	usb_control_msg(udev, usb_rcvctrlpipe(udev, 0),
			USB_VENDOR_REQUEST_READ_REGISTER,
			USB_DIR_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
			0, index, &tmpbuf, sizeof(*data), &len,
			USB_CTRL_GET_TIMEOUT);
	*data = tmpbuf;
	if (len != sizeof(*data)) {
		vmm_printf("smsc95xx_read_reg failed: index=%d, len=%d",
				index, len);
		return VMM_EIO;
	}

	*data = vmm_le32_to_cpu(tmpbuf);
	return VMM_OK;
}

/* Loop until the read is completed with timeout */
static int smsc95xx_phy_wait_not_busy(struct usb_device *udev)
{
	u64 timeout = 100000 + vmm_timer_timestamp();
	u32 val;

	do {
		smsc95xx_read_reg(udev, MII_ADDR, &val);
		if (!(val & MII_BUSY_))
			return VMM_OK;
	} while (vmm_timer_timestamp() < timeout);

	return VMM_ETIMEDOUT;
}

static int smsc95xx_mdio_read(struct usb_device *udev, int phy_id, int idx)
{
	u32 val, addr;

	/* confirm MII not busy */
	if (smsc95xx_phy_wait_not_busy(udev)) {
		vmm_printf("MII is busy in smsc95xx_mdio_read\n");
		return VMM_ETIMEDOUT;
	}

	/* set the address, index & direction (read from PHY) */
	addr = (phy_id << 11) | (idx << 6) | MII_READ_;
	smsc95xx_write_reg(udev, MII_ADDR, addr);

	if (smsc95xx_phy_wait_not_busy(udev)) {
		vmm_printf("Timed out reading MII reg %02X\n", idx);
		return VMM_ETIMEDOUT;
	}

	smsc95xx_read_reg(udev, MII_DATA, &val);

	return (u16)(val & 0xFFFF);
}

static void smsc95xx_mdio_write(struct usb_device *udev, int phy_id, int idx,
		int regval)
{
	u32 val, addr;

	/* confirm MII not busy */
	if (smsc95xx_phy_wait_not_busy(udev)) {
		vmm_printf("MII is busy in smsc95xx_mdio_write\n");
		return;
	}

	val = regval;
	smsc95xx_write_reg(udev, MII_DATA, val);

	/* set the address, index & direction (write to PHY) */
	addr = (phy_id << 11) | (idx << 6) | MII_WRITE_;
	smsc95xx_write_reg(udev, MII_ADDR, addr);

	if (smsc95xx_phy_wait_not_busy(udev))
		vmm_printf("Timed out writing MII reg %02X\n", idx);
}

/*
 * mii_nway_restart - restart NWay (autonegotiation) for this interface
 * Returns 0 on success, negative on error.
 */
static int _mii_nway_restart(struct usb_device *udev, struct usb_net_device *ndev)
{
	int bmcr;
	int r = -1;

	/* if autoneg is off, it's an error */
	bmcr = smsc95xx_mdio_read(udev, ndev->phy_id, MII_BMCR);

	if (bmcr & BMCR_ANENABLE) {
		bmcr |= BMCR_ANRESTART;
		smsc95xx_mdio_write(udev, ndev->phy_id, MII_BMCR, bmcr);
		r = 0;
	}
	return r;
}

static int smsc95xx_phy_initialize(struct usb_device *udev,
		struct usb_net_device *ndev)
{
	smsc95xx_mdio_write(udev, ndev->phy_id, MII_BMCR, BMCR_RESET);
	smsc95xx_mdio_write(udev, ndev->phy_id, MII_ADVERTISE,
			ADVERTISE_ALL | ADVERTISE_CSMA |
			ADVERTISE_PAUSE_CAP | ADVERTISE_PAUSE_ASYM);

	/* read to clear */
	smsc95xx_mdio_read(udev, ndev->phy_id, PHY_INT_SRC);

	smsc95xx_mdio_write(udev, ndev->phy_id, PHY_INT_MASK,
			PHY_INT_MASK_DEFAULT_);
	_mii_nway_restart(udev, ndev);

	DPRINTF("phy initialised succesfully\n");
	return VMM_OK;
}

static int smsc95xx_write_hwaddr_common(struct usb_device *udev,
		struct usb_net_device *dev,
		unsigned char *enetaddr)
{
	u32 addr_lo = get_unaligned((u32*)&enetaddr[0]);
	u32 addr_hi = get_unaligned((u16*)&enetaddr[4]);

	int ret;

	/* set hardware address */
	DPRINTF("%s MAC Low = 0x%x MAC High = 0x%x\n", __func__, addr_lo, addr_hi);
	ret = smsc95xx_write_reg(udev, ADDRL, addr_lo);
	if (ret < 0)
		return ret;

	ret = smsc95xx_write_reg(udev, ADDRH, addr_hi);
	if (ret < 0)
		return ret;

	dev->have_hwaddr = 1;

	return VMM_OK;
}

/* Enable or disable Tx & Rx checksum offload engines */
static int smsc95xx_set_csums(struct usb_device *udev, int use_tx_csum,
		int use_rx_csum)
{
	u32 read_buf;
	int ret = smsc95xx_read_reg(udev, COE_CR, &read_buf);
	if (ret < 0)
		return ret;

	if (use_tx_csum)
		read_buf |= Tx_COE_EN_;
	else
		read_buf &= ~Tx_COE_EN_;

	if (use_rx_csum)
		read_buf |= Rx_COE_EN_;
	else
		read_buf &= ~Rx_COE_EN_;

	ret = smsc95xx_write_reg(udev, COE_CR, read_buf);
	if (ret < 0)
		return ret;

	DPRINTF("COE_CR = 0x%08x\n", read_buf);
	return VMM_OK;
}


static void smsc95xx_set_multicast(struct usb_net_device *priv)
{
	/* disable */
	/* priv->mac_cr &= ~(MAC_CR_PRMS_ | MAC_CR_MCPAS_ | MAC_CR_HPFILT_); */
	/* Enable */
	priv->mac_cr |= (MAC_CR_PRMS_ | MAC_CR_MCPAS_ | MAC_CR_HPFILT_);
}

/* starts the TX path */
static void smsc95xx_start_tx_path(struct usb_device *udev,
		struct usb_net_device *priv)
{
	u32 reg_val;

	/* Enable Tx at MAC */
	priv->mac_cr |= MAC_CR_TXEN_;

	smsc95xx_write_reg(udev, MAC_CR, priv->mac_cr);

	/* Enable Tx at SCSRs */
	reg_val = TX_CFG_ON_;
	smsc95xx_write_reg(udev, TX_CFG, reg_val);
}

/* Starts the Receive path */
static void smsc95xx_start_rx_path(struct usb_device *udev,
		struct usb_net_device *priv)
{
	priv->mac_cr |= MAC_CR_RXEN_;
	smsc95xx_write_reg(udev, MAC_CR, priv->mac_cr);
}

static int smsc95xx_init_common(struct usb_device *udev, struct usb_net_device *dev,
		unsigned char *enetaddr)
{
	int ret;
	u32 write_buf;
	u32 read_buf;
	u32 burst_cap;
	int timeout;
#define TIMEOUT_RESOLUTION 50   /* ms */
	int link_detected;

	dev->phy_id = SMSC95XX_INTERNAL_PHY_ID; /* fixed phy id */

	write_buf = HW_CFG_LRST_;
	ret = smsc95xx_write_reg(udev, HW_CFG, write_buf);
	if (ret < 0)
		return ret;

	timeout = 0;
	do {
		ret = smsc95xx_read_reg(udev, HW_CFG, &read_buf);
		if (ret < 0)
			return ret;
		vmm_usleep(10 * 1000);
		timeout++;
	} while ((read_buf & HW_CFG_LRST_) && (timeout < 100));

	if (timeout >= 100) {
		vmm_printf("timeout waiting for completion of Lite Reset\n");
		return VMM_ETIMEDOUT;
	}

	write_buf = PM_CTL_PHY_RST_;
	ret = smsc95xx_write_reg(udev, PM_CTRL, write_buf);
	if (ret < 0)
		return ret;

	timeout = 0;
	do {
		ret = smsc95xx_read_reg(udev, PM_CTRL, &read_buf);
		if (ret < 0)
			return ret;
		vmm_usleep(10 * 1000);
		timeout++;
	} while ((read_buf & PM_CTL_PHY_RST_) && (timeout < 100));
	if (timeout >= 100) {
		vmm_printf("timeout waiting for PHY Reset\n");
		return VMM_ETIMEDOUT;
	}
	if(is_valid_ether_addr(enetaddr)) {
		DPRINTF("Have valid MAC address\n");
		dev->have_hwaddr = 1;
	}

	if (!dev->have_hwaddr) {
		vmm_printf("Error: SMSC95xx: No MAC address set - set usbethaddr\n");
		//return -EADDRNOTAVAIL;
		return VMM_EINVALID;
	}
	ret = smsc95xx_write_hwaddr_common(udev, dev, enetaddr);
	if (ret < 0)
		return ret;

#ifdef TURBO_MODE
	if (dev->pusb_dev->speed == USB_SPEED_HIGH) {
		burst_cap = DEFAULT_HS_BURST_CAP_SIZE / HS_USB_PKT_SIZE;
		dev->rx_urb_size = DEFAULT_HS_BURST_CAP_SIZE;
	} else {
		burst_cap = DEFAULT_FS_BURST_CAP_SIZE / FS_USB_PKT_SIZE;
		dev->rx_urb_size = DEFAULT_FS_BURST_CAP_SIZE;
	}
#else
	burst_cap = 0;
	dev->rx_urb_size = MAX_SINGLE_PACKET_SIZE;
#endif
	DPRINTF("rx_urb_size=%ld\n", (ulong)dev->rx_urb_size);

	ret = smsc95xx_write_reg(udev, BURST_CAP, burst_cap);
	if (ret < 0)
		return ret;

	ret = smsc95xx_read_reg(udev, BURST_CAP, &read_buf);
	if (ret < 0)
		return ret;
	DPRINTF("Read Value from BURST_CAP after writing: 0x%08x\n", read_buf);

	read_buf = DEFAULT_BULK_IN_DELAY;
	ret = smsc95xx_write_reg(udev, BULK_IN_DLY, read_buf);
	if (ret < 0)
		return ret;

	ret = smsc95xx_read_reg(udev, BULK_IN_DLY, &read_buf);
	if (ret < 0)
		return ret;
	DPRINTF("Read Value from BULK_IN_DLY after writing: "
			"0x%08x\n", read_buf);

	ret = smsc95xx_read_reg(udev, HW_CFG, &read_buf);
	if (ret < 0)
		return ret;
	DPRINTF("Read Value from HW_CFG: 0x%08x\n", read_buf);
#ifdef TURBO_MODE
	read_buf |= (HW_CFG_MEF_ | HW_CFG_BCE_);
#endif
	read_buf &= ~HW_CFG_RXDOFF_;

#define _NET_IP_ALIGN 0
	read_buf |= _NET_IP_ALIGN << 9;

	ret = smsc95xx_write_reg(udev, HW_CFG, read_buf);
	if (ret < 0)
		return ret;

	ret = smsc95xx_read_reg(udev, HW_CFG, &read_buf);
	if (ret < 0)
		return ret;
	DPRINTF("Read Value from HW_CFG after writing: 0x%08x\n", read_buf);

	write_buf = 0xFFFFFFFF;
	ret = smsc95xx_write_reg(udev, INT_STS, write_buf);
	if (ret < 0)
		return ret;

	ret = smsc95xx_read_reg(udev, ID_REV, &read_buf);
	if (ret < 0)
		return ret;
	DPRINTF("ID_REV = 0x%08x\n", read_buf);

	/* Configure GPIO pins as LED outputs */
	write_buf = LED_GPIO_CFG_SPD_LED | LED_GPIO_CFG_LNK_LED |
		LED_GPIO_CFG_FDX_LED;
	ret = smsc95xx_write_reg(udev, LED_GPIO_CFG, write_buf);
	if (ret < 0)
		return ret;

	/* Init Tx */
	write_buf = 0;
	ret = smsc95xx_write_reg(udev, FLOW, write_buf);
	if (ret < 0)
		return ret;

	read_buf = AFC_CFG_DEFAULT;
	ret = smsc95xx_write_reg(udev, AFC_CFG, read_buf);
	if (ret < 0)
		return ret;

	ret = smsc95xx_read_reg(udev, MAC_CR, &dev->mac_cr);
	if (ret < 0)
		return ret;

	/* Init Rx. Set Vlan */
	write_buf = (u32)ETH_P_8021Q;
	ret = smsc95xx_write_reg(udev, VLAN1, write_buf);
	if (ret < 0)
		return ret;
	/* Disable checksum offload engines */
	ret = smsc95xx_set_csums(udev, 0, 0);
	if (ret < 0) {
		vmm_printf("Failed to set csum offload: %d\n", ret);
		return ret;
	}
	smsc95xx_set_multicast(dev);

	ret = smsc95xx_phy_initialize(udev, dev);
	if (ret < 0)
		return ret;
	ret = smsc95xx_read_reg(udev, INT_EP_CTL, &read_buf);
	if (ret < 0)
		return ret;
	/* enable PHY interrupts */
	read_buf |= INT_EP_CTL_PHY_INT_;

	ret = smsc95xx_write_reg(udev, INT_EP_CTL, read_buf);
	if (ret < 0)
		return ret;

	smsc95xx_start_tx_path(udev, dev);
	smsc95xx_start_rx_path(udev, dev);

	timeout = 0;
	do {
		link_detected = smsc95xx_mdio_read(udev, dev->phy_id, MII_BMSR)
			& BMSR_LSTATUS;
		if (!link_detected) {
			if (timeout == 0)
				vmm_printf("Waiting for Ethernet connection... ");
			vmm_udelay(TIMEOUT_RESOLUTION * 1000);
			timeout += TIMEOUT_RESOLUTION;
		}
	} while (!link_detected && timeout < PHY_CONNECT_TIMEOUT);
	if (link_detected) {
		if (timeout != 0)
			vmm_printf("done.\n");
	} else {
		vmm_printf("unable to connect.\n");
		return VMM_EIO;
	}
	return VMM_OK;
}

static int smsc95xx_send_common(struct usb_net_device *ndev, void *packet, int length)
{
	int err;
	int actual_len;
	u32 tx_cmd_a;
	u32 tx_cmd_b;
	u8 __cacheline_aligned msg[(PKTSIZE + sizeof(tx_cmd_a) + sizeof(tx_cmd_b))];

	if (length > PKTSIZE)
		return VMM_ENOSPC;

	tx_cmd_a = (u32)length | TX_CMD_A_FIRST_SEG_ | TX_CMD_A_LAST_SEG_;
	tx_cmd_b = (u32)length;

	tx_cmd_a = vmm_cpu_to_le32(tx_cmd_a);
	tx_cmd_b = vmm_cpu_to_le32(tx_cmd_b);

	/* prepend cmd_a and cmd_b */
	memcpy(msg, &tx_cmd_a, sizeof(tx_cmd_a));
	memcpy(msg + sizeof(tx_cmd_a), &tx_cmd_b, sizeof(tx_cmd_b));
	memcpy(msg + sizeof(tx_cmd_a) + sizeof(tx_cmd_b), (void *)packet,
			length);
	err = usb_bulk_msg(ndev->udev,
			usb_sndbulkpipe(ndev->udev, ndev->ep_out),
			(void *)msg,
			length + sizeof(tx_cmd_a) + sizeof(tx_cmd_b),
			&actual_len,
			USB_BULK_SEND_TIMEOUT);
	DPRINTF("Tx: len = %u, actual = %u, err = %d\n",
			(unsigned int)(length + sizeof(tx_cmd_a) + sizeof(tx_cmd_b)),
			(unsigned int)actual_len, err);

	return err;
}

static int smsc95xx_recv(struct usb_net_device *ndev, int* rx_len)
{
	struct sk_buff *skb;
	u8 *rdptr;
	u8 __cacheline_aligned recv_buf[RX_URB_SIZE];
	unsigned char *buf_ptr;
	int err;
	int actual_len;
	u32 packet_len;
	int cur_buf_align;

	memset(recv_buf, 0, RX_URB_SIZE);

	err = usb_bulk_msg(ndev->udev,
			usb_rcvbulkpipe(ndev->udev, ndev->ep_in),
			(void *)recv_buf, RX_URB_SIZE, &actual_len,
			USB_BULK_RECV_TIMEOUT);

	if (err != 0) {
		DPRINTF("Rx: failed to receive\n");
		return -err;
	}
	if (actual_len > RX_URB_SIZE) {
		DPRINTF("Rx: received too many bytes %d\n", actual_len);
		return -ENOSPC;
	}

	buf_ptr = recv_buf;
	while (actual_len > 0) {
		/*
		 * 1st 4 bytes contain the length of the actual data plus error
		 * info. Extract data length.
		 */
		if (actual_len < sizeof(packet_len)) {
			DPRINTF("Rx: incomplete packet length\n");
			return -EIO;
		}
		/* First word contains length of packet */
		packet_len = ((u32*)buf_ptr)[0];
		*rx_len = packet_len;
		packet_len = vmm_le32_to_cpu(packet_len);
		if (packet_len & RX_STS_ES_) {
			DPRINTF("Rx: Error header=%#x", packet_len);
			return -EIO;
		}
		packet_len = ((packet_len & RX_STS_FL_) >> 16);


		if (packet_len > actual_len - sizeof(packet_len)) {
			DPRINTF("Rx: too large packet: %d\n", packet_len);
			return -EIO;
		}

		/* Notify net stack */
		if(packet_len) {
			DPRINTF("Rx: packet lenght %d\n", packet_len);
			skb = dev_alloc_skb(packet_len + 4);
			skb_reserve(skb, 2);
			rdptr = (u8 *)skb_put(skb, packet_len-4);
			memcpy(rdptr, buf_ptr+sizeof(packet_len), packet_len-4);
			netif_rx(skb, ndev->ndev);
			/* Update stats */
			ndev->ndev->stats.rx_bytes += (packet_len-4);
		}

		/* Adjust for next iteration */
		actual_len -= sizeof(packet_len) + packet_len;
		buf_ptr += sizeof(packet_len) + packet_len;
		cur_buf_align = (ulong)buf_ptr - (ulong)recv_buf;

		if (cur_buf_align & 0x03) {
			int align = 4 - (cur_buf_align & 0x03);

			actual_len -= align;
			buf_ptr += align;
		}
	}
	return err;
}

/* Polling thread for Rx data */
static int smsc95xx_worker(void* data) {
	struct net_device *ndev = (struct net_device*)data;
	struct usb_net_device *nd = ndev->net_priv;
	int rx_len;

	while(nd->rx_thread_state) {
		smsc95xx_recv(nd, &rx_len);
		/* sleep for some time if nothing received */
		if(0 == rx_len)
			vmm_msleep(1);
	}
	vmm_printf("rx worker thread terminated\n");
	/* Signal thread status */
	nd->rx_thread_state = task_terminate;

	return VMM_OK;
}


/* ethtool ops */
static void smsc95xx_get_drvinfo(struct net_device *dev,
		struct ethtool_drvinfo *info)
{
	strlcpy(info->driver, DRV_NAME, sizeof(DRV_NAME));
	strlcpy(info->version, DRV_VERSION, sizeof(DRV_VERSION));
	strlcpy(info->bus_info, dev_name(dev->dev), sizeof(info->bus_info));
}

static int smsc95xx_get_settings(struct net_device *dev, struct ethtool_cmd *cmd)
{
	return 0;
}

static int smsc95xx_set_settings(struct net_device *dev, struct ethtool_cmd *cmd)
{
	return 0;
}

/*
 *  Open the interface.
 *  The interface is opened whenever "ifconfig" actives it.
 */
static int smsc95xx_open(struct net_device *dev)
{
	struct usb_net_device *nd = (struct usb_net_device*)dev->net_priv;
	int ret = smsc95xx_init_common( nd->udev,
			dev->net_priv,
			dev->dev_addr);

	if(VMM_OK == ret) {
		netif_carrier_on(dev);
		netif_start_queue(dev);
	}

	return ret;
}


/* 
 * Stop the interface.
 * The interface is stopped when it is brought.
 */
static int smsc95xx_stop(struct net_device *ndev)
{
	if (netif_msg_ifdown(ndev))
		vmm_printf("%s shutting down %s\n", __func__, ndev->name);

	netif_stop_queue(ndev);
	netif_carrier_off(ndev);

	return 0;
}


/* 
 * Hardware start transmission.
 * Send a packet to media from the upper layer.
 */
static int smsc95xx_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	int ret = 0;
	struct usb_net_device *nd = (struct usb_net_device*)dev->net_priv;

	/* TODO: check for partial tansfer */
	ret = smsc95xx_send_common(nd, skb_data(skb), skb_len(skb));
	if(ret != VMM_OK) {
		return NETDEV_TX_BUSY; /* FIXME: add correct error code */
	}
	dev->stats.tx_bytes += skb_len(skb);
	dev->trans_start = jiffies;

	/* free this SKB */
	dev_kfree_skb(skb);
	return NETDEV_TX_OK;
}

static int smsc95xx_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	return 0;
}


static const struct ethtool_ops smsc95xx_ethtool_ops = {
	.get_drvinfo    = smsc95xx_get_drvinfo,
	.get_settings   = smsc95xx_get_settings,
	.set_settings   = smsc95xx_set_settings,
	.get_link       = ethtool_op_get_link,
};


static const struct net_device_ops smsc95xx_netdev_ops = {
	.ndo_open               = smsc95xx_open,
	.ndo_stop               = smsc95xx_stop,
	.ndo_start_xmit         = smsc95xx_start_xmit,
	.ndo_do_ioctl           = smsc95xx_ioctl,
	.ndo_change_mtu         = eth_change_mtu,
	.ndo_validate_addr      = eth_validate_addr,
};


static int smsc95xx_probe(struct usb_interface *intf,
		const struct usb_device_id *id)
{
	int rc = VMM_OK;;
	struct usb_net_device *unet;
	struct usb_interface_descriptor *iface_desc;
	struct usb_device *udev = interface_to_usbdev(intf);

	struct net_device *nd;

	iface_desc = &intf->desc;

	vmm_printf("USB network device detected\n");

	/*
	 * We are expecting a minimum of 3 endpoints - in, out (bulk), and int.
	 * We will ignore any others.
	 */
	if(intf->no_of_ep != 3) {
		vmm_printf("Invalid device detected\n");
		return VMM_ENODEV;
	}

	/* Update curent settings of usb interface */
	rc = usb_set_interface(udev, intf->desc.bInterfaceNumber, 0);
	if (rc) {
		return VMM_EIO;
	}

	/* Alloc usb network instance */
	unet = vmm_zalloc(sizeof(*unet));
	if (!unet) {
		return VMM_ENOMEM;
	}

	for (int i = 0; i < intf->no_of_ep; i++) {
		/* is it an BULK endpoint? */
		if ((intf->ep_desc[i].bmAttributes &
					USB_ENDPOINT_XFERTYPE_MASK) == USB_ENDPOINT_XFER_BULK) {
			if (intf->ep_desc[i].bEndpointAddress & USB_DIR_IN) {
				unet->ep_in = intf->ep_desc[i].bEndpointAddress &
					USB_ENDPOINT_NUMBER_MASK;
			} else {
				unet->ep_out = intf->ep_desc[i].bEndpointAddress &
					USB_ENDPOINT_NUMBER_MASK;
			}
		}

		/* is it an interrupt endpoint? */
		if ((intf->ep_desc[i].bmAttributes &
					USB_ENDPOINT_XFERTYPE_MASK) == USB_ENDPOINT_XFER_INT) {
			unet->ep_int = intf->ep_desc[i].bEndpointAddress &
				USB_ENDPOINT_NUMBER_MASK;
			unet->irqinterval = intf->ep_desc[i].bInterval;
		}
	}

	DPRINTF("%s Endpoints In %d Out %d Int %d\n", __func__,
			unet->ep_in, unet->ep_out, unet->ep_int);

	/* Do some basic sanity checks, and bail if we find a problem */
	if (usb_set_interface(udev, iface_desc->bInterfaceNumber, 0) ||
			!unet->ep_in || !unet->ep_out || !unet->ep_int) {
		vmm_printf("Problems with device\n");
		return VMM_ENODEV;
	}

	nd = alloc_etherdev(sizeof(struct usb_net_device));
	if (!nd) {
		vmm_free(unet);
		vmm_printf("%s: could not allocate net device.\n", __func__);
		return VMM_ENOMEM;
	}

	usb_ref_device(udev);
	unet->udev = udev;
	unet->intf = intf;
	unet->ndev = nd;

	nd->net_priv=(void*)unet;
	eth_hw_addr_random(nd);

	strlcpy(nd->name, DRV_NAME, sizeof(DRV_NAME));

	SET_NETDEV_DEV(nd, &intf->dev);

	ether_setup(nd);

	nd->netdev_ops = &smsc95xx_netdev_ops;
	nd->ethtool_ops = &smsc95xx_ethtool_ops;

	rc = register_netdev(nd);
	if (rc) {
		vmm_free(unet);
		free_netdev(nd);
		vmm_printf("%s: Registering netdev failed!\n", __func__);
		return VMM_ENODEV;
	}
#if DEBUG
	print_mac_address_fmt(ndev->dev_addr);
#endif
	unet->rx_thread = vmm_threads_create("smsc95xx_rx",
			smsc95xx_worker, nd,
			VMM_THREAD_DEF_PRIORITY,
			VMM_THREAD_DEF_TIME_SLICE);

	unet->rx_thread_state = task_stop;


	if (!unet->rx_thread) {
		vmm_free(unet);
		free_netdev(nd);
		vmm_printf("%s Error: Not able to create Rx thread\n", __func__);
		return VMM_ENOSPC;
	} else {
		/* Mark thread as running */
		unet->rx_thread_state = task_running;
		vmm_threads_start(unet->rx_thread);
	}


	/* Set usb interface data */
	interface_set_data(intf, unet);

	return VMM_OK;
}

static void smsc95xx_disconnect(struct usb_interface *intf)
{
	struct usb_net_device *nd = interface_get_data(intf);

	/* Stop Rx thread */
	nd->rx_thread_state = task_stop;

	/* wait for Rx task to complete */
	while(nd->rx_thread_state != task_terminate) {
		vmm_mdelay(1);
	}

	vmm_threads_destroy(nd->rx_thread);

	/* Clear usb interface data */
	interface_set_data(intf, NULL);

	/* Destroy network instance */
	unregister_netdev(nd->ndev);
	free_netdev(nd->ndev);

	/* Free the USB device */
	usb_dref_device(nd->udev);

	/* Free usb storage instance */
	vmm_free(nd);
}

static const struct usb_device_id smsc95xx_eth_id_table[] = {
	{ USB_DEVICE(0x05ac, 0x1402) },
	{ USB_DEVICE(0x0424, 0xec00) }, /* LAN9512/LAN9514 Ethernet */
	{ USB_DEVICE(0x0424, 0x9500) }, /* LAN9500 Ethernet */
	{ USB_DEVICE(0x0424, 0x9730) }, /* LAN9730 Ethernet (HSIC) */
	{ USB_DEVICE(0x0424, 0x9900) }, /* SMSC9500 USB Ethernet (SAL10) */
	{ USB_DEVICE(0x0424, 0x9e00) }, /* LAN9500A Ethernet */
	{ }                             /* Terminating entry */
};

static struct usb_driver usb_smsc95xx_driver = {
	.name		= "smsc95xx",
	.id_table	= smsc95xx_eth_id_table,
	.probe		= smsc95xx_probe,
	.disconnect	= smsc95xx_disconnect,
};

static int __init smsc95xx_driver_init(void)
{
	INIT_SPIN_LOCK(&unet_lock);
	return usb_register(&usb_smsc95xx_driver);
}

static void __exit smsc95xx_driver_exit(void)
{
	usb_deregister(&usb_smsc95xx_driver);
}

VMM_DECLARE_MODULE(MODULE_DESC,
		MODULE_AUTHOR,
		MODULE_LICENSE,
		MODULE_IPRIORITY,
		MODULE_INIT,
		MODULE_EXIT);
