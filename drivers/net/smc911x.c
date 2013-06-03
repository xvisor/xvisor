/**
 * Copyright (c) 2012 Pranav Sawargaonkar.
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
 * @file smc911x.c
 * @author Pranav Sawargaonkar (pranav.sawargaonkar@gmail.com)
 * @brief Driver for SMSC's LAN911{5,6,7,8} single-chip Ethernet devices.
 *
 * The source has been largely adapted from Linux 3.x or higher:
 * drivers/net/smc911x.c
 *
 * Copyright (C) 2005 Sensoria Corp
 *
 * The original code is licensed under the GPL.
 */

#include <vmm_modules.h>
#include <smc911x.h>

#include <linux/delay.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/interrupt.h>


#define MODULE_DESC			"SMC911x Ethernet Driver"
#define MODULE_AUTHOR			"Pranav Sawargaonkar"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(VMM_NET_CLASS_IPRIORITY + 1)
#define	MODULE_INIT			smc911x_driver_init
#define	MODULE_EXIT			smc911x_driver_exit

static const char version[] =
		"smc911x.c: v1.0 04-16-2005 by Dustin McIntire <dustin@sensoria.com>\n";

/* Debugging options */
#define	ENABLE_SMC_DEBUG_RX		0
#define	ENABLE_SMC_DEBUG_TX		0
#define	ENABLE_SMC_DEBUG_DMA		0
#define	ENABLE_SMC_DEBUG_PKTS		0
#define	ENABLE_SMC_DEBUG_MISC		0
#define	ENABLE_SMC_DEBUG_FUNC		0

#define	SMC_DEBUG_RX		((ENABLE_SMC_DEBUG_RX	? 1 : 0) << 0)
#define	SMC_DEBUG_TX		((ENABLE_SMC_DEBUG_TX	? 1 : 0) << 1)
#define	SMC_DEBUG_DMA		((ENABLE_SMC_DEBUG_DMA	? 1 : 0) << 2)
#define	SMC_DEBUG_PKTS		((ENABLE_SMC_DEBUG_PKTS	? 1 : 0) << 3)
#define	SMC_DEBUG_MISC		((ENABLE_SMC_DEBUG_MISC	? 1 : 0) << 4)
#define	SMC_DEBUG_FUNC		((ENABLE_SMC_DEBUG_FUNC	? 1 : 0) << 5)

#ifndef	SMC_DEBUG
#define	SMC_DEBUG	(SMC_DEBUG_RX   | \
			SMC_DEBUG_TX   | \
			SMC_DEBUG_DMA  | \
			SMC_DEBUG_PKTS | \
			SMC_DEBUG_MISC | \
			SMC_DEBUG_FUNC   \
			)
#endif


/*
 * Transmit timeout, default 5 seconds.
 */
static int tx_fifo_kb=8;

/*
 * The internal workings of the driver.  If you are changing anything
 * here with the SMC stuff, you should have the datasheet and know
 * what you are doing.
 */
#define CARDNAME "smc911x"

/*
 * Use power-down feature of the chip
 */
#define POWER_DOWN		 1

#if SMC_DEBUG > 0
#define DBG(n, args...)				 \
	do {					 \
		if (SMC_DEBUG & (n))		 \
			printk(args);		 \
	} while (0)

#define PRINTK(args...)   printk(args)
#else
#define DBG(n, args...)   do { } while (0)
#define PRINTK(args...)   printk(KERN_DEBUG args)
#endif

#if SMC_DEBUG_PKTS > 0
static void PRINT_PKT(u_char *buf, int length)
{
	int i;
	int remainder;
	int lines;

	lines = length / 16;
	remainder = length % 16;

	for (i = 0; i < lines ; i ++) {
		int cur;
		for (cur = 0; cur < 8; cur++) {
			u_char a, b;
			a = *buf++;
			b = *buf++;
			printk("%02x%02x ", a, b);
		}
		printk("\n");
	}
	for (i = 0; i < remainder/2 ; i++) {
		u_char a, b;
		a = *buf++;
		b = *buf++;
		printk("%02x%02x ", a, b);
	}
	printk("\n");
}
#else
#define PRINT_PKT(x...)  do { } while (0)
#endif


/* this enables an interrupt in the interrupt mask register */
#define SMC_ENABLE_INT(lp, x) do {			\
	unsigned int  __mask;				\
	__mask = SMC_GET_INT_EN((lp));			\
	__mask |= (x);					\
	SMC_SET_INT_EN((lp), __mask);			\
} while (0)

/* this disables an interrupt from the interrupt mask register */
#define SMC_DISABLE_INT(lp, x) do {			\
	unsigned int  __mask;				\
	__mask = SMC_GET_INT_EN((lp));			\
	__mask &= ~(x);					\
	SMC_SET_INT_EN((lp), __mask);			\
} while (0)

/*
 * this does a soft reset on the device
 */
static void smc911x_reset(struct net_device *dev)
{
	struct smc911x_local *lp = netdev_priv(dev);
	unsigned int reg, timeout=0, resets=1, irq_cfg;
	unsigned long flags;

	DBG(SMC_DEBUG_FUNC, "%s: --> %s\n", dev->name, __func__);

	/*	 Take out of PM setting first */
	if ((SMC_GET_PMT_CTRL(lp) & PMT_CTRL_READY_) == 0) {
		/* Write to the bytetest will take out of powerdown */
		SMC_SET_BYTE_TEST(lp, 0);
		timeout=10;
		do {
			udelay(10);
			reg = SMC_GET_PMT_CTRL(lp) & PMT_CTRL_READY_;
		} while (--timeout && !reg);
		if (timeout == 0) {
			PRINTK("%s: smc911x_reset timeout waiting for PM restore\n", dev->name);
			return;
		}
	}

	/* Disable all interrupts */
	spin_lock_irqsave(&lp->lock, flags);
	SMC_SET_INT_EN(lp, 0);
	spin_unlock_irqrestore(&lp->lock, flags);

	while (resets--) {
		SMC_SET_HW_CFG(lp, HW_CFG_SRST_);
		timeout=10;
		do {
			udelay(10);
			reg = SMC_GET_HW_CFG(lp);
			/* If chip indicates reset timeout then try again */
			if (reg & HW_CFG_SRST_TO_) {
				PRINTK("%s: chip reset timeout, retrying...\n", dev->name);
				resets++;
				break;
			}
		} while (--timeout && (reg & HW_CFG_SRST_));
	}
	if (timeout == 0) {
		PRINTK("%s: smc911x_reset timeout waiting for reset\n", dev->name);
		return;
	}

	/* make sure EEPROM has finished loading before setting GPIO_CFG */
	timeout=1000;
	while (--timeout && (SMC_GET_E2P_CMD(lp) & E2P_CMD_EPC_BUSY_))
		udelay(10);

	if (timeout == 0){
		PRINTK("%s: smc911x_reset timeout waiting for EEPROM busy\n", dev->name);
		return;
	}

	/* Initialize interrupts */
	SMC_SET_INT_EN(lp, 0);
	SMC_ACK_INT(lp, -1);

	/* Reset the FIFO level and flow control settings */
	SMC_SET_HW_CFG(lp, (lp->tx_fifo_kb & 0xF) << 16);
//TODO: Figure out what appropriate pause time is
	SMC_SET_FLOW(lp, FLOW_FCPT_ | FLOW_FCEN_);
	SMC_SET_AFC_CFG(lp, lp->afc_cfg);


	/* Set to LED outputs */
	SMC_SET_GPIO_CFG(lp, 0x70070000);

	/*
	 * Deassert IRQ for 1*10us for edge type interrupts
	 * and drive IRQ pin push-pull
	 */
	irq_cfg = (1 << 24) | INT_CFG_IRQ_EN_ | INT_CFG_IRQ_TYPE_;
#ifdef SMC_DYNAMIC_BUS_CONFIG
	if (lp->cfg.irq_polarity)
		irq_cfg |= INT_CFG_IRQ_POL_;
#endif
	SMC_SET_IRQ_CFG(lp, irq_cfg);

	/* clear anything saved */
	if (lp->pending_tx_skb != NULL) {
		dev_kfree_skb (lp->pending_tx_skb);
		lp->pending_tx_skb = NULL;
		dev->stats.tx_errors++;
		dev->stats.tx_aborted_errors++;
	}
}

/*
 * Enable Interrupts, Receive, and Transmit
 */
static void smc911x_enable(struct net_device *dev)
{
	struct smc911x_local *lp = netdev_priv(dev);
	unsigned mask, cfg, cr;
	unsigned long flags;

	DBG(SMC_DEBUG_FUNC, "%s: --> %s\n", dev->name, __func__);

	spin_lock_irqsave(&lp->lock, flags);

	SMC_SET_MAC_ADDR(lp, dev->dev_addr);

	/* Enable TX */
	cfg = SMC_GET_HW_CFG(lp);
	cfg &= HW_CFG_TX_FIF_SZ_ | 0xFFF;
	cfg |= HW_CFG_SF_;
	SMC_SET_HW_CFG(lp, cfg);
	SMC_SET_FIFO_TDA(lp, 0xFF);
	/* Update TX stats on every 64 packets received or every 1 sec */
	SMC_SET_FIFO_TSL(lp, 64);
	SMC_SET_GPT_CFG(lp, GPT_CFG_TIMER_EN_ | 10000);

	SMC_GET_MAC_CR(lp, cr);
	cr |= MAC_CR_TXEN_ | MAC_CR_HBDIS_;
	SMC_SET_MAC_CR(lp, cr);
	SMC_SET_TX_CFG(lp, TX_CFG_TX_ON_);

	/* Add 2 byte padding to start of packets */
	SMC_SET_RX_CFG(lp, (2<<8) & RX_CFG_RXDOFF_);

	/* Turn on receiver and enable RX */
	if (cr & MAC_CR_RXEN_)
		DBG(SMC_DEBUG_RX, "%s: Receiver already enabled\n", dev->name);

	SMC_SET_MAC_CR(lp, cr | MAC_CR_RXEN_);

	/* Interrupt on every received packet */
	SMC_SET_FIFO_RSA(lp, 0x01);
	SMC_SET_FIFO_RSL(lp, 0x00);

	/* now, enable interrupts */
	mask = INT_EN_TDFA_EN_ | INT_EN_TSFL_EN_ | INT_EN_RSFL_EN_ |
		INT_EN_GPT_INT_EN_ | INT_EN_RXDFH_INT_EN_ | INT_EN_RXE_EN_ |
		INT_EN_PHY_INT_EN_;
	if (IS_REV_A(lp->revision))
		mask|=INT_EN_RDFL_EN_;
	else {
		mask|=INT_EN_RDFO_EN_;
	}
	SMC_ENABLE_INT(lp, mask);

	spin_unlock_irqrestore(&lp->lock, flags);
}

/*
 * this puts the device in an inactive state
 */
static void smc911x_shutdown(struct net_device *dev)
{
	struct smc911x_local *lp = netdev_priv(dev);
	unsigned cr;
	unsigned long flags;

	DBG(SMC_DEBUG_FUNC, "%s: --> %s\n", CARDNAME, __func__);

	/* Disable IRQ's */
	SMC_SET_INT_EN(lp, 0);

	/* Turn of Rx and TX */
	spin_lock_irqsave(&lp->lock, flags);
	SMC_GET_MAC_CR(lp, cr);
	cr &= ~(MAC_CR_TXEN_ | MAC_CR_RXEN_ | MAC_CR_HBDIS_);
	SMC_SET_MAC_CR(lp, cr);
	SMC_SET_TX_CFG(lp, TX_CFG_STOP_TX_);
	spin_unlock_irqrestore(&lp->lock, flags);
}

static inline void smc911x_drop_pkt(struct net_device *dev)
{
	struct smc911x_local *lp = netdev_priv(dev);
	unsigned int fifo_count, timeout, reg;

	DBG(SMC_DEBUG_FUNC | SMC_DEBUG_RX, "%s: --> %s\n", CARDNAME, __func__);
	fifo_count = SMC_GET_RX_FIFO_INF(lp) & 0xFFFF;
	if (fifo_count <= 4) {
		/* Manually dump the packet data */
		while (fifo_count--)
			SMC_GET_RX_FIFO(lp);
	} else	 {
		/* Fast forward through the bad packet */
		SMC_SET_RX_DP_CTRL(lp, RX_DP_CTRL_FFWD_BUSY_);
		timeout=50;
		do {
			udelay(10);
			reg = SMC_GET_RX_DP_CTRL(lp) & RX_DP_CTRL_FFWD_BUSY_;
		} while (--timeout && reg);
		if (timeout == 0) {
			PRINTK("%s: timeout waiting for RX fast forward\n", dev->name);
		}
	}
}

/*
 * This is the procedure to handle the receipt of a packet.
 * It should be called after checking for packet presence in
 * the RX status FIFO.	 It must be called with the spin lock
 * already held.
 */
static inline void	 smc911x_rcv(struct net_device *dev)
{
	struct smc911x_local *lp = netdev_priv(dev);
	unsigned int pkt_len, status;
	struct sk_buff *skb;
	unsigned char *data;

	DBG(SMC_DEBUG_FUNC | SMC_DEBUG_RX, "%s: --> %s\n",
		dev->name, __func__);
	status = SMC_GET_RX_STS_FIFO(lp);
	DBG(SMC_DEBUG_RX, "%s: Rx pkt len %d status 0x%08x \n",
		dev->name, (status & 0x3fff0000) >> 16, status & 0xc000ffff);
	pkt_len = (status & RX_STS_PKT_LEN_) >> 16;
	if (status & RX_STS_ES_) {
		/* Deal with a bad packet */
		dev->stats.rx_errors++;
		if (status & RX_STS_CRC_ERR_)
			dev->stats.rx_crc_errors++;
		else {
			if (status & RX_STS_LEN_ERR_)
				dev->stats.rx_length_errors++;
			if (status & RX_STS_MCAST_)
				dev->stats.multicast++;
		}
		/* Remove the bad packet data from the RX FIFO */
		smc911x_drop_pkt(dev);
	} else {
		/* Receive a valid packet */
		/* Alloc a buffer with extra room for DMA alignment */
		skb=dev_alloc_skb(pkt_len+32);
		if (unlikely(skb == NULL)) {
			PRINTK( "%s: Low memory, rcvd packet dropped.\n",
				dev->name);
			dev->stats.rx_dropped++;
			smc911x_drop_pkt(dev);
			return;
		}
		/* Align IP header to 32 bits
		 * Note that the device is configured to add a 2
		 * byte padding to the packet start, so we really
		 * want to write to the orignal data pointer */
		data = (unsigned char *) skb_data(skb);
		skb_reserve(skb, 2);
		skb_put(skb,pkt_len-4);
#ifdef SMC_USE_DMA
		{
		unsigned int fifo;
		/* Lower the FIFO threshold if possible */
		fifo = SMC_GET_FIFO_INT(lp);
		if (fifo & 0xFF) fifo--;
		DBG(SMC_DEBUG_RX, "%s: Setting RX stat FIFO threshold to %d\n",
			dev->name, fifo & 0xff);
		SMC_SET_FIFO_INT(lp, fifo);
		/* Setup RX DMA */
		SMC_SET_RX_CFG(lp, RX_CFG_RX_END_ALGN16_ | ((2<<8) & RX_CFG_RXDOFF_));
		lp->rxdma_active = 1;
		lp->current_rx_skb = skb;
		SMC_PULL_DATA(lp, data, (pkt_len+2+15) & ~15);
		/* Packet processing deferred to DMA RX interrupt */
		}
#else
		SMC_SET_RX_CFG(lp, RX_CFG_RX_END_ALGN4_ | ((2<<8) & RX_CFG_RXDOFF_));
		SMC_PULL_DATA(lp, data, pkt_len+2+3);

		DBG(SMC_DEBUG_PKTS, "%s: Received packet\n", dev->name);
		PRINT_PKT(data, ((pkt_len - 4) <= 64) ? pkt_len - 4 : 64);
		//Fixme: skb->protocol = eth_type_trans(skb, dev);
		netif_rx(skb, dev);
		dev->stats.rx_packets++;
		dev->stats.rx_bytes += pkt_len-4;
#endif
	}
}

/*
 * This is called to actually send a packet to the chip.
 */
static void smc911x_hardware_send_pkt(struct net_device *dev)
{
	struct smc911x_local *lp = netdev_priv(dev);
	struct sk_buff *skb;
	unsigned int cmdA, cmdB, len;
	unsigned char *buf;

	DBG(SMC_DEBUG_FUNC | SMC_DEBUG_TX, "%s: --> %s\n", dev->name, __func__);
	BUG_ON(lp->pending_tx_skb == NULL);

	skb = lp->pending_tx_skb;
	lp->pending_tx_skb = NULL;

	/* cmdA {25:24] data alignment [20:16] start offset [10:0] buffer length */
	/* cmdB {31:16] pkt tag [10:0] length */
#ifdef SMC_USE_DMA
	/* 16 byte buffer alignment mode */
	buf = (char*)((u32)(skb_data(skb)) & ~0xF);
	len = (skb_len(skb) + 0xF + ((u32)skb_data(skb) & 0xF)) & ~0xF;
	cmdA = (1<<24) | (((u32)skb_data(skb) & 0xF)<<16) |
		TX_CMD_A_INT_FIRST_SEG_ | TX_CMD_A_INT_LAST_SEG_ |
		skb_len(skb);


#else
	buf = (unsigned char*)((u32) skb_data(skb) & ~0x3);
	len = (skb_len(skb) + 3 + ((u32)skb_data(skb) & 3)) & ~0x3;
	cmdA = (((u32)skb_data(skb) & 0x3) << 16) |
		TX_CMD_A_INT_FIRST_SEG_| TX_CMD_A_INT_LAST_SEG_ |
		skb_len(skb);

#endif
	/* tag is packet length so we can use this in stats update later */
	cmdB = (skb_len(skb)  << 16) | (skb_len(skb) & 0x7FF);


	DBG(SMC_DEBUG_TX, "%s: TX PKT LENGTH 0x%04x (%d) BUF 0x%p CMDA 0x%08x CMDB 0x%08x\n",
		 dev->name, len, len, buf, cmdA, cmdB);
	SMC_SET_TX_FIFO(lp, cmdA);
	SMC_SET_TX_FIFO(lp, cmdB);

	DBG(SMC_DEBUG_PKTS, "%s: Transmitted packet\n", dev->name);
	PRINT_PKT(buf, len <= 64 ? len : 64);

	/* Send pkt via PIO or DMA */
#ifdef SMC_USE_DMA
	lp->current_tx_skb = skb;
	SMC_PUSH_DATA(lp, buf, len);
	/* DMA complete IRQ will free buffer and set jiffies */
#else
	SMC_PUSH_DATA(lp, buf, len);

	dev_kfree_skb(skb);
#endif
	if (!lp->tx_throttle) {
		netif_wake_queue(dev);
	}
	SMC_ENABLE_INT(lp, INT_EN_TDFA_EN_ | INT_EN_TSFL_EN_);
}

/*
 * Since I am not sure if I will have enough room in the chip's ram
 * to store the packet, I call this routine which either sends it
 * now, or set the card to generates an interrupt when ready
 * for the packet.
 */
static int smc911x_hard_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct smc911x_local *lp = netdev_priv(dev);
	unsigned int free;
	unsigned long flags;

	DBG(SMC_DEBUG_FUNC | SMC_DEBUG_TX, "%s: --> %s\n",
		dev->name, __func__);

	spin_lock_irqsave(&lp->lock, flags);

	BUG_ON(lp->pending_tx_skb != NULL);

	free = SMC_GET_TX_FIFO_INF(lp) & TX_FIFO_INF_TDFREE_;
	DBG(SMC_DEBUG_TX, "%s: TX free space %d\n", dev->name, free);

	/* Turn off the flow when running out of space in FIFO */
	if (free <= SMC911X_TX_FIFO_LOW_THRESHOLD) {
		DBG(SMC_DEBUG_TX, "%s: Disabling data flow due to low FIFO space (%d)\n",
			dev->name, free);
		/* Reenable when at least 1 packet of size MTU present */
		SMC_SET_FIFO_TDA(lp, (SMC911X_TX_FIFO_LOW_THRESHOLD)/64);
		lp->tx_throttle = 1;
		netif_stop_queue(dev);
	}

	/* Drop packets when we run out of space in TX FIFO
	 * Account for overhead required for:
	 *
	 *	  Tx command words			 8 bytes
	 *	  Start offset				 15 bytes
	 *	  End padding				 15 bytes
	 */
	if (unlikely(free < (skb_len(skb) + 8 + 15 + 15))) {
		printk("%s: No Tx free space %d < %d\n",
			dev->name, free, skb_len(skb));
		lp->pending_tx_skb = NULL;
		dev->stats.tx_errors++;
		dev->stats.tx_dropped++;
		spin_unlock_irqrestore(&lp->lock, flags);
		dev_kfree_skb(skb);
		return VMM_OK;
	}

#ifdef SMC_USE_DMA
	{
		/* If the DMA is already running then defer this packet Tx until
		 * the DMA IRQ starts it
		 */
		if (lp->txdma_active) {
			DBG(SMC_DEBUG_TX | SMC_DEBUG_DMA, "%s: Tx DMA running, deferring packet\n", dev->name);
			lp->pending_tx_skb = skb;
			netif_stop_queue(dev);
			spin_unlock_irqrestore(&lp->lock, flags);
			return VMM_OK; // return NETDEV_TX_OK;
		} else {
			DBG(SMC_DEBUG_TX | SMC_DEBUG_DMA, "%s: Activating Tx DMA\n", dev->name);
			lp->txdma_active = 1;
		}
	}
#endif
	lp->pending_tx_skb = skb;
	smc911x_hardware_send_pkt(dev);
	spin_unlock_irqrestore(&lp->lock, flags);

	return VMM_OK;	// return NETDEV_TX_OK;
}

/*
 * This handles a TX status interrupt, which is only called when:
 * - a TX error occurred, or
 * - TX of a packet completed.
 */
static void smc911x_tx(struct net_device *dev)
{
	struct smc911x_local *lp = netdev_priv(dev);
	unsigned int tx_status;

	DBG(SMC_DEBUG_FUNC | SMC_DEBUG_TX, "%s: --> %s\n",
		dev->name, __func__);

	/* Collect the TX status */
	while (((SMC_GET_TX_FIFO_INF(lp) & TX_FIFO_INF_TSUSED_) >> 16) != 0) {
		DBG(SMC_DEBUG_TX, "%s: Tx stat FIFO used 0x%04x\n",
			dev->name,
			(SMC_GET_TX_FIFO_INF(lp) & TX_FIFO_INF_TSUSED_) >> 16);
		tx_status = SMC_GET_TX_STS_FIFO(lp);
		dev->stats.tx_packets++;
		dev->stats.tx_bytes+=tx_status>>16;
		DBG(SMC_DEBUG_TX, "%s: Tx FIFO tag 0x%04x status 0x%04x\n",
			dev->name, (tx_status & 0xffff0000) >> 16,
			tx_status & 0x0000ffff);
		/* count Tx errors, but ignore lost carrier errors when in
		 * full-duplex mode */
		if ((tx_status & TX_STS_ES_) && !(lp->ctl_rfduplx &&
		    !(tx_status & 0x00000306))) {
			dev->stats.tx_errors++;
		}
		if (tx_status & TX_STS_MANY_COLL_) {
			dev->stats.collisions+=16;
			dev->stats.tx_aborted_errors++;
		} else {
			dev->stats.collisions+=(tx_status & TX_STS_COLL_CNT_) >> 3;
		}
		/* carrier error only has meaning for half-duplex communication */
		if ((tx_status & (TX_STS_LOC_ | TX_STS_NO_CARR_)) &&
		    !lp->ctl_rfduplx) {
			dev->stats.tx_carrier_errors++;
		}
		if (tx_status & TX_STS_LATE_COLL_) {
			dev->stats.collisions++;
			dev->stats.tx_aborted_errors++;
		}
	}
}


/*---PHY CONTROL AND CONFIGURATION-----------------------------------------*/
/*
 * Reads a register from the MII Management serial interface
 */

static int smc911x_phy_read(struct net_device *dev, int phyaddr, int phyreg)
{
	struct smc911x_local *lp = netdev_priv(dev);
	unsigned int phydata;

	SMC_GET_MII(lp, phyreg, phyaddr, phydata);

	DBG(SMC_DEBUG_MISC, "%s: phyaddr=0x%x, phyreg=0x%02x, phydata=0x%04x\n",
		__func__, phyaddr, phyreg, phydata);
	return phydata;
}


/*
 * Writes a register to the MII Management serial interface
 */
static void smc911x_phy_write(struct net_device *dev, int phyaddr, int phyreg,
			int phydata)
{
	struct smc911x_local *lp = netdev_priv(dev);

	DBG(SMC_DEBUG_MISC, "%s: phyaddr=0x%x, phyreg=0x%x, phydata=0x%x\n",
		__func__, phyaddr, phyreg, phydata);

	SMC_SET_MII(lp, phyreg, phyaddr, phydata);
}

/*
 * Finds and reports the PHY address (115 and 117 have external
 * PHY interface 118 has internal only
 */
static void smc911x_phy_detect(struct net_device *dev)
{
	struct smc911x_local *lp = netdev_priv(dev);
	int phyaddr;
	unsigned int cfg, id1, id2;

	DBG(SMC_DEBUG_FUNC, "%s: --> %s\n", dev->name, __func__);

	lp->phy_type = 0;

	/*
	 * Scan all 32 PHY addresses if necessary, starting at
	 * PHY#1 to PHY#31, and then PHY#0 last.
	 */
	switch(lp->version) {
		case CHIP_9115:
		case CHIP_9117:
		case CHIP_9215:
		case CHIP_9217:
			cfg = SMC_GET_HW_CFG(lp);
			if (cfg & HW_CFG_EXT_PHY_DET_) {
				cfg &= ~HW_CFG_PHY_CLK_SEL_;
				cfg |= HW_CFG_PHY_CLK_SEL_CLK_DIS_;
				SMC_SET_HW_CFG(lp, cfg);
				udelay(10); /* Wait for clocks to stop */

				cfg |= HW_CFG_EXT_PHY_EN_;
				SMC_SET_HW_CFG(lp, cfg);
				udelay(10); /* Wait for clocks to stop */

				cfg &= ~HW_CFG_PHY_CLK_SEL_;
				cfg |= HW_CFG_PHY_CLK_SEL_EXT_PHY_;
				SMC_SET_HW_CFG(lp, cfg);
				udelay(10); /* Wait for clocks to stop */

				cfg |= HW_CFG_SMI_SEL_;
				SMC_SET_HW_CFG(lp, cfg);

				for (phyaddr = 1; phyaddr < 32; ++phyaddr) {

					/* Read the PHY identifiers */
					SMC_GET_PHY_ID1(lp, phyaddr & 31, id1);
					SMC_GET_PHY_ID2(lp, phyaddr & 31, id2);

					/* Make sure it is a valid identifier */
					if (id1 != 0x0000 && id1 != 0xffff &&
					    id1 != 0x8000 && id2 != 0x0000 &&
					    id2 != 0xffff && id2 != 0x8000) {
						/* Save the PHY's address */
						lp->mii.phy_id = phyaddr & 31;
						lp->phy_type = id1 << 16 | id2;
						break;
					}
				}
				if (phyaddr < 32)
					/* Found an external PHY */
					break;
			}
		default:
			/* Internal media only */
			SMC_GET_PHY_ID1(lp, 1, id1);
			SMC_GET_PHY_ID2(lp, 1, id2);
			/* Save the PHY's address */
			lp->mii.phy_id = 1;
			lp->phy_type = id1 << 16 | id2;
	}

	DBG(SMC_DEBUG_MISC, "%s: phy_id1=0x%x, phy_id2=0x%x phyaddr=0x%d\n",
		dev->name, id1, id2, lp->mii.phy_id);
}

/*
 * Sets the PHY to a configuration as determined by the user.
 * Called with spin_lock held.
 */
static int smc911x_phy_fixed(struct net_device *dev)
{
	struct smc911x_local *lp = netdev_priv(dev);
	int phyaddr = lp->mii.phy_id;
	int bmcr;

	DBG(SMC_DEBUG_FUNC, "%s: --> %s\n", dev->name, __func__);

	/* Enter Link Disable state */
	SMC_GET_PHY_BMCR(lp, phyaddr, bmcr);
	bmcr |= BMCR_PDOWN;
	SMC_SET_PHY_BMCR(lp, phyaddr, bmcr);

	/*
	 * Set our fixed capabilities
	 * Disable auto-negotiation
	 */
	bmcr &= ~BMCR_ANENABLE;
	if (lp->ctl_rfduplx)
		bmcr |= BMCR_FULLDPLX;

	if (lp->ctl_rspeed == 100)
		bmcr |= BMCR_SPEED100;

	/* Write our capabilities to the phy control register */
	SMC_SET_PHY_BMCR(lp, phyaddr, bmcr);

	/* Re-Configure the Receive/Phy Control register */
	bmcr &= ~BMCR_PDOWN;
	SMC_SET_PHY_BMCR(lp, phyaddr, bmcr);

	return 1;
}

/*
 * smc911x_phy_reset - reset the phy
 * @dev: net device
 * @phy: phy address
 *
 * Issue a software reset for the specified PHY and
 * wait up to 100ms for the reset to complete.	 We should
 * not access the PHY for 50ms after issuing the reset.
 *
 * The time to wait appears to be dependent on the PHY.
 *
 */
static int smc911x_phy_reset(struct net_device *dev, int phy)
{
	struct smc911x_local *lp = netdev_priv(dev);
	int timeout;
	unsigned long flags;
	unsigned int reg;

	DBG(SMC_DEBUG_FUNC, "%s: --> %s()\n", dev->name, __func__);

	spin_lock_irqsave(&lp->lock, flags);
	reg = SMC_GET_PMT_CTRL(lp);
	reg &= ~0xfffff030;
	reg |= PMT_CTRL_PHY_RST_;
	SMC_SET_PMT_CTRL(lp, reg);
	spin_unlock_irqrestore(&lp->lock, flags);
	for (timeout = 2; timeout; timeout--) {
		msleep(50);
		spin_lock_irqsave(&lp->lock, flags);
		reg = SMC_GET_PMT_CTRL(lp);
		spin_unlock_irqrestore(&lp->lock, flags);
		if (!(reg & PMT_CTRL_PHY_RST_)) {
			/* extra delay required because the phy may
			 * not be completed with its reset
			 * when PHY_BCR_RESET_ is cleared. 256us
			 * should suffice, but use 500us to be safe
			 */
			udelay(500);
		break;
		}
	}

	return reg & PMT_CTRL_PHY_RST_;
}

/*
 * smc911x_phy_powerdown - powerdown phy
 * @dev: net device
 * @phy: phy address
 *
 * Power down the specified PHY
 */
static void smc911x_phy_powerdown(struct net_device *dev, int phy)
{
	struct smc911x_local *lp = netdev_priv(dev);
	unsigned int bmcr;

	/* Enter Link Disable state */
	SMC_GET_PHY_BMCR(lp, phy, bmcr);
	bmcr |= BMCR_PDOWN;
	SMC_SET_PHY_BMCR(lp, phy, bmcr);
}

/*
 * smc911x_phy_check_media - check the media status and adjust BMCR
 * @dev: net device
 * @init: set true for initialisation
 *
 * Select duplex mode depending on negotiation state.	This
 * also updates our carrier state.
 */
static void smc911x_phy_check_media(struct net_device *dev, int init)
{
	struct smc911x_local *lp = netdev_priv(dev);
	int phyaddr = lp->mii.phy_id;
	unsigned int bmcr, cr;

	DBG(SMC_DEBUG_FUNC, "%s: --> %s\n", dev->name, __func__);

	if (mii_check_media(&lp->mii, netif_msg_link(lp), init)) {
		/* duplex state has changed */
		SMC_GET_PHY_BMCR(lp, phyaddr, bmcr);
		SMC_GET_MAC_CR(lp, cr);
		if (lp->mii.full_duplex) {
			DBG(SMC_DEBUG_MISC, "%s: Configuring for full-duplex mode\n", dev->name);
			bmcr |= BMCR_FULLDPLX;
			cr |= MAC_CR_RCVOWN_;
		} else {
			DBG(SMC_DEBUG_MISC, "%s: Configuring for half-duplex mode\n", dev->name);
			bmcr &= ~BMCR_FULLDPLX;
			cr &= ~MAC_CR_RCVOWN_;
		}
		SMC_SET_PHY_BMCR(lp, phyaddr, bmcr);
		SMC_SET_MAC_CR(lp, cr);
	}
}

/*
 * Configures the specified PHY through the MII management interface
 * using Autonegotiation.
 * Calls smc911x_phy_fixed() if the user has requested a certain config.
 * If RPC ANEG bit is set, the media selection is dependent purely on
 * the selection by the MII (either in the MII BMCR reg or the result
 * of autonegotiation.)  If the RPC ANEG bit is cleared, the selection
 * is controlled by the RPC SPEED and RPC DPLX bits.
 */
static void smc911x_phy_configure(struct work_struct *work)
{
	struct smc911x_local *lp = container_of(work, struct smc911x_local,
						phy_configure);
	struct net_device *dev = lp->netdev;
	int phyaddr = lp->mii.phy_id;
	int my_phy_caps; /* My PHY capabilities */
	int my_ad_caps; /* My Advertised capabilities */
	unsigned long flags;

	DBG(SMC_DEBUG_FUNC, "%s: --> %s()\n", dev->name, __func__);

	/*
	 * We should not be called if phy_type is zero.
	 */
	if (lp->phy_type == 0)
		return;

	if (smc911x_phy_reset(dev, phyaddr)) {
		printk("%s: PHY reset timed out\n", dev->name);
		return;
	}
	spin_lock_irqsave(&lp->lock, flags);

	/*
	 * Enable PHY Interrupts (for register 18)
	 * Interrupts listed here are enabled
	 */
	SMC_SET_PHY_INT_MASK(lp, phyaddr, PHY_INT_MASK_ENERGY_ON_ |
		 PHY_INT_MASK_ANEG_COMP_ | PHY_INT_MASK_REMOTE_FAULT_ |
		 PHY_INT_MASK_LINK_DOWN_);

	/* If the user requested no auto neg, then go set his request */
	if (lp->mii.force_media) {
		smc911x_phy_fixed(dev);
		goto smc911x_phy_configure_exit;
	}

	/* Copy our capabilities from MII_BMSR to MII_ADVERTISE */
	SMC_GET_PHY_BMSR(lp, phyaddr, my_phy_caps);
	if (!(my_phy_caps & BMSR_ANEGCAPABLE)) {
		printk(KERN_INFO "Auto negotiation NOT supported\n");
		smc911x_phy_fixed(dev);
		goto smc911x_phy_configure_exit;
	}

	/* CSMA capable w/ both pauses */
	my_ad_caps = ADVERTISE_CSMA | ADVERTISE_PAUSE_CAP | ADVERTISE_PAUSE_ASYM;

	if (my_phy_caps & BMSR_100BASE4)
		my_ad_caps |= ADVERTISE_100BASE4;
	if (my_phy_caps & BMSR_100FULL)
		my_ad_caps |= ADVERTISE_100FULL;
	if (my_phy_caps & BMSR_100HALF)
		my_ad_caps |= ADVERTISE_100HALF;
	if (my_phy_caps & BMSR_10FULL)
		my_ad_caps |= ADVERTISE_10FULL;
	if (my_phy_caps & BMSR_10HALF)
		my_ad_caps |= ADVERTISE_10HALF;

	/* Disable capabilities not selected by our user */
	if (lp->ctl_rspeed != 100)
		my_ad_caps &= ~(ADVERTISE_100BASE4|ADVERTISE_100FULL|ADVERTISE_100HALF);

	 if (!lp->ctl_rfduplx)
		my_ad_caps &= ~(ADVERTISE_100FULL|ADVERTISE_10FULL);

	/* Update our Auto-Neg Advertisement Register */
	SMC_SET_PHY_MII_ADV(lp, phyaddr, my_ad_caps);
	lp->mii.advertising = my_ad_caps;

	/*
	 * Read the register back.	 Without this, it appears that when
	 * auto-negotiation is restarted, sometimes it isn't ready and
	 * the link does not come up.
	 */
	udelay(10);
	SMC_GET_PHY_MII_ADV(lp, phyaddr, my_ad_caps);

	DBG(SMC_DEBUG_MISC, "%s: phy caps=0x%04x\n", dev->name, my_phy_caps);
	DBG(SMC_DEBUG_MISC, "%s: phy advertised caps=0x%04x\n", dev->name, my_ad_caps);

	/* Restart auto-negotiation process in order to advertise my caps */
	SMC_SET_PHY_BMCR(lp, phyaddr, BMCR_ANENABLE | BMCR_ANRESTART);

	smc911x_phy_check_media(dev, 1);

smc911x_phy_configure_exit:
	spin_unlock_irqrestore(&lp->lock, flags);
}

/*
 * smc911x_phy_interrupt
 *
 * Purpose:  Handle interrupts relating to PHY register 18. This is
 *	 called from the "hard" interrupt handler under our private spinlock.
 */
static void smc911x_phy_interrupt(struct net_device *dev)
{
	struct smc911x_local *lp = netdev_priv(dev);
	int phyaddr = lp->mii.phy_id;
	int status;

	DBG(SMC_DEBUG_FUNC, "%s: --> %s\n", dev->name, __func__);

	if (lp->phy_type == 0)
		return;

	smc911x_phy_check_media(dev, 0);
	/* read to clear status bits */
	SMC_GET_PHY_INT_SRC(lp, phyaddr, status);
	DBG(SMC_DEBUG_MISC, "%s: PHY interrupt status 0x%04x\n",
		dev->name, status & 0xffff);
	DBG(SMC_DEBUG_MISC, "%s: AFC_CFG 0x%08x\n",
		dev->name, SMC_GET_AFC_CFG(lp));

	(void)status;
}

/*--- END PHY CONTROL AND CONFIGURATION-------------------------------------*/

/*
 * This is the main routine of the driver, to handle the device when
 * it needs some attention.
 */
static vmm_irq_return_t smc911x_interrupt(int irq_no, void *dev_id)

{
	struct net_device *dev = dev_id;
	struct smc911x_local *lp = netdev_priv(dev);
	unsigned int status, mask, timeout;
	unsigned int rx_overrun=0, cr, pkts;
	unsigned long flags;

	DBG(SMC_DEBUG_FUNC, "%s: --> %s\n", dev->name, __func__);

	spin_lock_irqsave(&lp->lock, flags);

	/* Spurious interrupt check */
	if ((SMC_GET_IRQ_CFG(lp) & (INT_CFG_IRQ_INT_ | INT_CFG_IRQ_EN_)) !=
		(INT_CFG_IRQ_INT_ | INT_CFG_IRQ_EN_)) {
		spin_unlock_irqrestore(&lp->lock, flags);
		return IRQ_NONE;
	}

	mask = SMC_GET_INT_EN(lp);
	SMC_SET_INT_EN(lp, 0);

	/* set a timeout value, so I don't stay here forever */
	timeout = 8;


	do {
		status = SMC_GET_INT(lp);

		DBG(SMC_DEBUG_MISC, "%s: INT 0x%08x MASK 0x%08x OUTSIDE MASK 0x%08x\n",
			dev->name, status, mask, status & ~mask);

		status &= mask;
		if (!status)
			break;

		/* Handle SW interrupt condition */
		if (status & INT_STS_SW_INT_) {
			SMC_ACK_INT(lp, INT_STS_SW_INT_);
			mask &= ~INT_EN_SW_INT_EN_;
		}
		/* Handle various error conditions */
		if (status & INT_STS_RXE_) {
			SMC_ACK_INT(lp, INT_STS_RXE_);
			dev->stats.rx_errors++;
		}
		if (status & INT_STS_RXDFH_INT_) {
			SMC_ACK_INT(lp, INT_STS_RXDFH_INT_);
			dev->stats.rx_dropped+=SMC_GET_RX_DROP(lp);
		 }
		/* Undocumented interrupt-what is the right thing to do here? */
		if (status & INT_STS_RXDF_INT_) {
			SMC_ACK_INT(lp, INT_STS_RXDF_INT_);
		}

		/* Rx Data FIFO exceeds set level */
		if (status & INT_STS_RDFL_) {
			if (IS_REV_A(lp->revision)) {
				rx_overrun=1;
				SMC_GET_MAC_CR(lp, cr);
				cr &= ~MAC_CR_RXEN_;
				SMC_SET_MAC_CR(lp, cr);
				DBG(SMC_DEBUG_RX, "%s: RX overrun\n", dev->name);
				dev->stats.rx_errors++;
				dev->stats.rx_fifo_errors++;
			}
			SMC_ACK_INT(lp, INT_STS_RDFL_);
		}
		if (status & INT_STS_RDFO_) {
			if (!IS_REV_A(lp->revision)) {
				SMC_GET_MAC_CR(lp, cr);
				cr &= ~MAC_CR_RXEN_;
				SMC_SET_MAC_CR(lp, cr);
				rx_overrun=1;
				DBG(SMC_DEBUG_RX, "%s: RX overrun\n", dev->name);
				dev->stats.rx_errors++;
				dev->stats.rx_fifo_errors++;
			}
			SMC_ACK_INT(lp, INT_STS_RDFO_);
		}
		/* Handle receive condition */
		if ((status & INT_STS_RSFL_) || rx_overrun) {
			unsigned int fifo;
			DBG(SMC_DEBUG_RX, "%s: RX irq\n", dev->name);
			fifo = SMC_GET_RX_FIFO_INF(lp);
			pkts = (fifo & RX_FIFO_INF_RXSUSED_) >> 16;
			DBG(SMC_DEBUG_RX, "%s: Rx FIFO pkts %d, bytes %d\n",
				dev->name, pkts, fifo & 0xFFFF );
			if (pkts != 0) {
#ifdef SMC_USE_DMA
				unsigned int fifo;
				if (lp->rxdma_active){
					DBG(SMC_DEBUG_RX | SMC_DEBUG_DMA,
						"%s: RX DMA active\n", dev->name);
					/* The DMA is already running so up the IRQ threshold */
					fifo = SMC_GET_FIFO_INT(lp) & ~0xFF;
					fifo |= pkts & 0xFF;
					DBG(SMC_DEBUG_RX,
						"%s: Setting RX stat FIFO threshold to %d\n",
						dev->name, fifo & 0xff);
					SMC_SET_FIFO_INT(lp, fifo);
				} else
#endif
				smc911x_rcv(dev);
			}
			SMC_ACK_INT(lp, INT_STS_RSFL_);
		}
		/* Handle transmit FIFO available */
		if (status & INT_STS_TDFA_) {
			DBG(SMC_DEBUG_TX, "%s: TX data FIFO space available irq\n", dev->name);
			SMC_SET_FIFO_TDA(lp, 0xFF);
			lp->tx_throttle = 0;
#ifdef SMC_USE_DMA
			if (!lp->txdma_active)
#endif
				netif_wake_queue(dev);
			SMC_ACK_INT(lp, INT_STS_TDFA_);
		}
		/* Handle transmit done condition */
#if 1
		if (status & (INT_STS_TSFL_ | INT_STS_GPT_INT_)) {
			DBG(SMC_DEBUG_TX | SMC_DEBUG_MISC,
				"%s: Tx stat FIFO limit (%d) /GPT irq\n",
				dev->name, (SMC_GET_FIFO_INT(lp) & 0x00ff0000) >> 16);
			smc911x_tx(dev);
			SMC_SET_GPT_CFG(lp, GPT_CFG_TIMER_EN_ | 10000);
			SMC_ACK_INT(lp, INT_STS_TSFL_);
			SMC_ACK_INT(lp, INT_STS_TSFL_ | INT_STS_GPT_INT_);
		}
#else
		if (status & INT_STS_TSFL_) {
			DBG(SMC_DEBUG_TX, "%s: TX status FIFO limit (%d) irq \n", dev->name, );
			smc911x_tx(dev);
			SMC_ACK_INT(lp, INT_STS_TSFL_);
		}

		if (status & INT_STS_GPT_INT_) {
			DBG(SMC_DEBUG_RX, "%s: IRQ_CFG 0x%08x FIFO_INT 0x%08x RX_CFG 0x%08x\n",
				dev->name,
				SMC_GET_IRQ_CFG(lp),
				SMC_GET_FIFO_INT(lp),
				SMC_GET_RX_CFG(lp));
			DBG(SMC_DEBUG_RX, "%s: Rx Stat FIFO Used 0x%02x "
				"Data FIFO Used 0x%04x Stat FIFO 0x%08x\n",
				dev->name,
				(SMC_GET_RX_FIFO_INF(lp) & 0x00ff0000) >> 16,
				SMC_GET_RX_FIFO_INF(lp) & 0xffff,
				SMC_GET_RX_STS_FIFO_PEEK(lp));
			SMC_SET_GPT_CFG(lp, GPT_CFG_TIMER_EN_ | 10000);
			SMC_ACK_INT(lp, INT_STS_GPT_INT_);
		}
#endif

		/* Handle PHY interrupt condition */
		if (status & INT_STS_PHY_INT_) {
			DBG(SMC_DEBUG_MISC, "%s: PHY irq\n", dev->name);
			smc911x_phy_interrupt(dev);
			SMC_ACK_INT(lp, INT_STS_PHY_INT_);
		}
	} while (--timeout);

	/* restore mask state */
	SMC_SET_INT_EN(lp, mask);

	DBG(SMC_DEBUG_MISC, "%s: Interrupt done (%d loops)\n",
		dev->name, 8-timeout);

	spin_unlock_irqrestore(&lp->lock, flags);

	return IRQ_HANDLED;
}

#ifdef SMC_USE_DMA
static void
smc911x_tx_dma_irq(int dma, void *data)
{
	struct net_device *dev = (struct net_device *)data;
	struct smc911x_local *lp = netdev_priv(dev);
	struct sk_buff *skb = lp->current_tx_skb;
	unsigned long flags;

	DBG(SMC_DEBUG_FUNC, "%s: --> %s\n", dev->name, __func__);

	DBG(SMC_DEBUG_TX | SMC_DEBUG_DMA, "%s: TX DMA irq handler\n", dev->name);
	/* Clear the DMA interrupt sources */
	SMC_DMA_ACK_IRQ(dev, dma);
	BUG_ON(skb == NULL);
	dma_unmap_single(NULL, tx_dmabuf, tx_dmalen, DMA_TO_DEVICE);
	dev->trans_start = jiffies;
	dev_kfree_skb_irq(skb);
	lp->current_tx_skb = NULL;
	if (lp->pending_tx_skb != NULL)
		smc911x_hardware_send_pkt(dev);
	else {
		DBG(SMC_DEBUG_TX | SMC_DEBUG_DMA,
			"%s: No pending Tx packets. DMA disabled\n", dev->name);
		spin_lock_irqsave(&lp->lock, flags);
		lp->txdma_active = 0;
		if (!lp->tx_throttle) {
			netif_wake_queue(dev);
		}
		spin_unlock_irqrestore(&lp->lock, flags);
	}

	DBG(SMC_DEBUG_TX | SMC_DEBUG_DMA,
		"%s: TX DMA irq completed\n", dev->name);
}
static void
smc911x_rx_dma_irq(int dma, void *data)
{
	struct net_device *dev = (struct net_device *)data;
	unsigned long ioaddr = dev->base_addr;
	struct smc911x_local *lp = netdev_priv(dev);
	struct sk_buff *skb = lp->current_rx_skb;
	unsigned long flags;
	unsigned int pkts;

	DBG(SMC_DEBUG_FUNC, "%s: --> %s\n", dev->name, __func__);
	DBG(SMC_DEBUG_RX | SMC_DEBUG_DMA, "%s: RX DMA irq handler\n", dev->name);
	/* Clear the DMA interrupt sources */
	SMC_DMA_ACK_IRQ(dev, dma);
	dma_unmap_single(NULL, rx_dmabuf, rx_dmalen, DMA_FROM_DEVICE);
	BUG_ON(skb == NULL);
	lp->current_rx_skb = NULL;
	PRINT_PKT(skb->data, skb->len);
	skb->protocol = eth_type_trans(skb, dev);
	dev->stats.rx_packets++;
	dev->stats.rx_bytes += skb->len;
	netif_rx(skb);

	spin_lock_irqsave(&lp->lock, flags);
	pkts = (SMC_GET_RX_FIFO_INF(lp) & RX_FIFO_INF_RXSUSED_) >> 16;
	if (pkts != 0) {
		smc911x_rcv(dev);
	}else {
		lp->rxdma_active = 0;
	}
	spin_unlock_irqrestore(&lp->lock, flags);
	DBG(SMC_DEBUG_RX | SMC_DEBUG_DMA,
		"%s: RX DMA irq completed. DMA RX FIFO PKTS %d\n",
		dev->name, pkts);
}
#endif	 /* SMC_USE_DMA */

#ifdef CONFIG_NET_POLL_CONTROLLER
/*
 * Polling receive - used by netconsole and other diagnostic tools
 * to allow network i/o with interrupts disabled.
 */
static void smc911x_poll_controller(struct net_device *dev)
{
	disable_irq(dev->irq);
	smc911x_interrupt(dev->irq, dev);
	enable_irq(dev->irq);
}
#endif

#if 0
/* Our watchdog timed out. Called by the networking layer */
static void smc911x_timeout(struct net_device *dev)
{
	struct smc911x_local *lp = netdev_priv(dev);
	int status, mask;
	unsigned long flags;

	DBG(SMC_DEBUG_FUNC, "%s: --> %s\n", dev->name, __func__);

	spin_lock_irqsave(&lp->lock, flags);
	status = SMC_GET_INT(lp);
	mask = SMC_GET_INT_EN(lp);
	spin_unlock_irqrestore(&lp->lock, flags);
	DBG(SMC_DEBUG_MISC, "%s: INT 0x%02x MASK 0x%02x \n",
		dev->name, status, mask);

	/* Dump the current TX FIFO contents and restart */
	mask = SMC_GET_TX_CFG(lp);
	SMC_SET_TX_CFG(lp, mask | TX_CFG_TXS_DUMP_ | TX_CFG_TXD_DUMP_);
	/*
	 * Reconfiguring the PHY doesn't seem like a bad idea here, but
	 * smc911x_phy_configure() calls msleep() which calls schedule_timeout()
	 * which calls schedule().	 Hence we use a work queue.
	 */
	if (lp->phy_type != 0)
		schedule_work(&lp->phy_configure);

	/* We can accept TX packets again */
	dev->trans_start = jiffies;
	netif_wake_queue(dev);
}

/*
 * This routine will, depending on the values passed to it,
 * either make it accept multicast packets, go into
 * promiscuous mode (for TCPDUMP and cousins) or accept
 * a select set of multicast packets
 */
static void smc911x_set_multicast_list(struct net_device *dev)
{
	struct smc911x_local *lp = netdev_priv(dev);
	unsigned int multicast_table[2];
	unsigned int mcr, update_multicast = 0;
	unsigned long flags;

	DBG(SMC_DEBUG_FUNC, "%s: --> %s\n", dev->name, __func__);

	spin_lock_irqsave(&lp->lock, flags);
	SMC_GET_MAC_CR(lp, mcr);
	spin_unlock_irqrestore(&lp->lock, flags);

	if (dev->flags & IFF_PROMISC) {

		DBG(SMC_DEBUG_MISC, "%s: RCR_PRMS\n", dev->name);
		mcr |= MAC_CR_PRMS_;
	}
	/*
	 * Here, I am setting this to accept all multicast packets.
	 * I don't need to zero the multicast table, because the flag is
	 * checked before the table is
	 */
	else if (dev->flags & IFF_ALLMULTI || dev->mc_count > 16) {
		DBG(SMC_DEBUG_MISC, "%s: RCR_ALMUL\n", dev->name);
		mcr |= MAC_CR_MCPAS_;
	}

	/*
	 * This sets the internal hardware table to filter out unwanted
	 * multicast packets before they take up memory.
	 *
	 * The SMC chip uses a hash table where the high 6 bits of the CRC of
	 * address are the offset into the table.	If that bit is 1, then the
	 * multicast packet is accepted.  Otherwise, it's dropped silently.
	 *
	 * To use the 6 bits as an offset into the table, the high 1 bit is
	 * the number of the 32 bit register, while the low 5 bits are the bit
	 * within that register.
	 */
	else if (dev->mc_count)  {
		int i;
		struct dev_mc_list *cur_addr;

		/* Set the Hash perfec mode */
		mcr |= MAC_CR_HPFILT_;

		/* start with a table of all zeros: reject all */
		memset(multicast_table, 0, sizeof(multicast_table));

		cur_addr = dev->mc_list;
		for (i = 0; i < dev->mc_count; i++, cur_addr = cur_addr->next) {
			u32 position;

			/* do we have a pointer here? */
			if (!cur_addr)
				break;
			/* make sure this is a multicast address -
				shouldn't this be a given if we have it here ? */
			if (!(*cur_addr->dmi_addr & 1))
				 continue;

			/* upper 6 bits are used as hash index */
			position = ether_crc(ETH_ALEN, cur_addr->dmi_addr)>>26;

			multicast_table[position>>5] |= 1 << (position&0x1f);
		}

		/* be sure I get rid of flags I might have set */
		mcr &= ~(MAC_CR_PRMS_ | MAC_CR_MCPAS_);

		/* now, the table can be loaded into the chipset */
		update_multicast = 1;
	} else	 {
		DBG(SMC_DEBUG_MISC, "%s: ~(MAC_CR_PRMS_|MAC_CR_MCPAS_)\n",
			dev->name);
		mcr &= ~(MAC_CR_PRMS_ | MAC_CR_MCPAS_);

		/*
		 * since I'm disabling all multicast entirely, I need to
		 * clear the multicast list
		 */
		memset(multicast_table, 0, sizeof(multicast_table));
		update_multicast = 1;
	}

	spin_lock_irqsave(&lp->lock, flags);
	SMC_SET_MAC_CR(lp, mcr);
	if (update_multicast) {
		DBG(SMC_DEBUG_MISC,
			"%s: update mcast hash table 0x%08x 0x%08x\n",
			dev->name, multicast_table[0], multicast_table[1]);
		SMC_SET_HASHL(lp, multicast_table[0]);
		SMC_SET_HASHH(lp, multicast_table[1]);
	}
	spin_unlock_irqrestore(&lp->lock, flags);
}


#endif

/*
 * Open and Initialize the board
 *
 * Set up everything, reset the card, etc..
 */
static int
smc911x_open(struct net_device *dev)
{
	struct smc911x_local *lp = netdev_priv(dev);

	DBG(SMC_DEBUG_FUNC, "%s: --> %s\n", dev->name, __func__);

	/*
	 * Check that the address is valid.  If its not, refuse
	 * to bring the device up.	 The user must specify an
	 * address using ifconfig eth0 hw ether xx:xx:xx:xx:xx:xx
	 */
	if (!is_valid_ether_addr(dev->dev_addr)) {
		PRINTK("%s: no valid ethernet hw addr\n", __func__);
		return VMM_EINVALID;
	}

	/* reset the hardware */
	smc911x_reset(dev);

	/* Configure the PHY, initialize the link state */
	smc911x_phy_configure(&lp->phy_configure);

	/* Turn on Tx + Rx */
	smc911x_enable(dev);

	netif_start_queue(dev);

	return 0;
}

/*
 * smc911x_close
 *
 * this makes the board clean up everything that it can
 * and not talk to the outside world.	 Caused by
 * an 'ifconfig ethX down'
 */
static int smc911x_close(struct net_device *dev)
{
	struct smc911x_local *lp = netdev_priv(dev);

	DBG(SMC_DEBUG_FUNC, "%s: --> %s\n", dev->name, __func__);

	netif_stop_queue(dev);
	netif_carrier_off(dev);

	/* clear everything */
	smc911x_shutdown(dev);

	if (lp->phy_type != 0) {
		/* We need to ensure that no calls to
		 * smc911x_phy_configure are pending.
		 */
		 // Fixme: cancel_work_sync(&lp->phy_configure);
		smc911x_phy_powerdown(dev, lp->mii.phy_id);
	}

	if (lp->pending_tx_skb) {
		dev_kfree_skb(lp->pending_tx_skb);
		lp->pending_tx_skb = NULL;
	}

	return 0;
}

/*
 * Ethtool support
 */
static int
smc911x_ethtool_getsettings(struct net_device *dev, struct ethtool_cmd *cmd)
{
	struct smc911x_local *lp = netdev_priv(dev);
	int ret, status;
	unsigned long flags;

	DBG(SMC_DEBUG_FUNC, "%s: --> %s\n", dev->name, __func__);
	cmd->maxtxpkt = 1;
	cmd->maxrxpkt = 1;

	if (lp->phy_type != 0) {
		spin_lock_irqsave(&lp->lock, flags);
		ret = mii_ethtool_gset(&lp->mii, cmd);
		spin_unlock_irqrestore(&lp->lock, flags);
	} else {
		cmd->supported = SUPPORTED_10baseT_Half |
				SUPPORTED_10baseT_Full |
				SUPPORTED_TP | SUPPORTED_AUI;

		if (lp->ctl_rspeed == 10)
			cmd->speed = SPEED_10;
		else if (lp->ctl_rspeed == 100)
			cmd->speed = SPEED_100;

		cmd->autoneg = AUTONEG_DISABLE;
		if (lp->mii.phy_id==1)
			cmd->transceiver = XCVR_INTERNAL;
		else
			cmd->transceiver = XCVR_EXTERNAL;
		cmd->port = 0;
		SMC_GET_PHY_SPECIAL(lp, lp->mii.phy_id, status);
		cmd->duplex =
			(status & (PHY_SPECIAL_SPD_10FULL_ | PHY_SPECIAL_SPD_100FULL_)) ?
				DUPLEX_FULL : DUPLEX_HALF;
		ret = 0;
	}

	return ret;
}

static int
smc911x_ethtool_setsettings(struct net_device *dev, struct ethtool_cmd *cmd)
{
	struct smc911x_local *lp = netdev_priv(dev);
	int ret;
	unsigned long flags;

	if (lp->phy_type != 0) {
		spin_lock_irqsave(&lp->lock, flags);
		ret = mii_ethtool_sset(&lp->mii, cmd);
		spin_unlock_irqrestore(&lp->lock, flags);
	} else {
		if (cmd->autoneg != AUTONEG_DISABLE ||
			cmd->speed != SPEED_10 ||
			(cmd->duplex != DUPLEX_HALF && cmd->duplex != DUPLEX_FULL) ||
			(cmd->port != PORT_TP && cmd->port != PORT_AUI))
			return -EINVAL;

		lp->ctl_rfduplx = cmd->duplex == DUPLEX_FULL;

		ret = 0;
	}

	return ret;
}

static void
smc911x_ethtool_getdrvinfo(struct net_device *dev, struct ethtool_drvinfo *info)
{
	strlcpy(info->driver, CARDNAME, sizeof(info->driver));
	strlcpy(info->version, version, sizeof(info->version));
#if 0
	strlcpy(info->bus_info, dev_name(dev->dev.parent), sizeof(info->bus_info));
#endif
}

static int smc911x_ethtool_nwayreset(struct net_device *dev)
{
	struct smc911x_local *lp = netdev_priv(dev);
	int ret = -EINVAL;
	unsigned long flags;

	if (lp->phy_type != 0) {
		spin_lock_irqsave(&lp->lock, flags);
		ret = mii_nway_restart(&lp->mii);
		spin_unlock_irqrestore(&lp->lock, flags);
	}

	return ret;
}

static u32 smc911x_ethtool_getmsglevel(struct net_device *dev)
{
	struct smc911x_local *lp = netdev_priv(dev);
	return lp->msg_enable;
}

static void smc911x_ethtool_setmsglevel(struct net_device *dev, u32 level)
{
	struct smc911x_local *lp = netdev_priv(dev);
	lp->msg_enable = level;
}

static int smc911x_ethtool_getregslen(struct net_device *dev)
{
	/* System regs + MAC regs + PHY regs */
	return (((E2P_CMD - ID_REV)/4 + 1) +
			(WUCSR - MAC_CR)+1 + 32) * sizeof(u32);
}

static void smc911x_ethtool_getregs(struct net_device *dev,
										 struct ethtool_regs* regs, void *buf)
{
	struct smc911x_local *lp = netdev_priv(dev);
	unsigned long flags;
	u32 reg,i,j=0;
	u32 *data = (u32*)buf;

	regs->version = lp->version;
	for(i=ID_REV;i<=E2P_CMD;i+=4) {
		data[j++] = SMC_inl(lp, i);
	}
	for(i=MAC_CR;i<=WUCSR;i++) {
		spin_lock_irqsave(&lp->lock, flags);
		SMC_GET_MAC_CSR(lp, i, reg);
		spin_unlock_irqrestore(&lp->lock, flags);
		data[j++] = reg;
	}
	for(i=0;i<=31;i++) {
		spin_lock_irqsave(&lp->lock, flags);
		SMC_GET_MII(lp, i, lp->mii.phy_id, reg);
		spin_unlock_irqrestore(&lp->lock, flags);
		data[j++] = reg & 0xFFFF;
	}
}

static int smc911x_ethtool_wait_eeprom_ready(struct net_device *dev)
{
	struct smc911x_local *lp = netdev_priv(dev);
	unsigned int timeout;
	int e2p_cmd;

	e2p_cmd = SMC_GET_E2P_CMD(lp);
	for(timeout=10;(e2p_cmd & E2P_CMD_EPC_BUSY_) && timeout; timeout--) {
		if (e2p_cmd & E2P_CMD_EPC_TIMEOUT_) {
			PRINTK("%s: %s timeout waiting for EEPROM to respond\n",
				dev->name, __func__);
			return -EFAULT;
		}
		mdelay(1);
		e2p_cmd = SMC_GET_E2P_CMD(lp);
	}
	if (timeout == 0) {
		PRINTK("%s: %s timeout waiting for EEPROM CMD not busy\n",
			dev->name, __func__);
		return -ETIMEDOUT;
	}
	return 0;
}

static inline int smc911x_ethtool_write_eeprom_cmd(struct net_device *dev,
													int cmd, int addr)
{
	struct smc911x_local *lp = netdev_priv(dev);
	int ret;

	if ((ret = smc911x_ethtool_wait_eeprom_ready(dev))!=0)
		return ret;
	SMC_SET_E2P_CMD(lp, E2P_CMD_EPC_BUSY_ |
		((cmd) & (0x7<<28)) |
		((addr) & 0xFF));
	return 0;
}

static inline int smc911x_ethtool_read_eeprom_byte(struct net_device *dev,
													u8 *data)
{
	struct smc911x_local *lp = netdev_priv(dev);
	int ret;

	if ((ret = smc911x_ethtool_wait_eeprom_ready(dev))!=0)
		return ret;
	*data = SMC_GET_E2P_DATA(lp);
	return 0;
}

static inline int smc911x_ethtool_write_eeprom_byte(struct net_device *dev,
													 u8 data)
{
	struct smc911x_local *lp = netdev_priv(dev);
	int ret;

	if ((ret = smc911x_ethtool_wait_eeprom_ready(dev))!=0)
		return ret;
	SMC_SET_E2P_DATA(lp, data);
	return 0;
}

static int smc911x_ethtool_geteeprom(struct net_device *dev,
									  struct ethtool_eeprom *eeprom, u8 *data)
{
	u8 eebuf[SMC911X_EEPROM_LEN];
	int i, ret;

	for(i=0;i<SMC911X_EEPROM_LEN;i++) {
		if ((ret=smc911x_ethtool_write_eeprom_cmd(dev, E2P_CMD_EPC_CMD_READ_, i ))!=0)
			return ret;
		if ((ret=smc911x_ethtool_read_eeprom_byte(dev, &eebuf[i]))!=0)
			return ret;
		}
	memcpy(data, eebuf+eeprom->offset, eeprom->len);
	return 0;
}

static int smc911x_ethtool_seteeprom(struct net_device *dev,
									   struct ethtool_eeprom *eeprom, u8 *data)
{
	int i, ret;

	/* Enable erase */
	if ((ret=smc911x_ethtool_write_eeprom_cmd(dev, E2P_CMD_EPC_CMD_EWEN_, 0 ))!=0)
		return ret;
	for(i=eeprom->offset;i<(eeprom->offset+eeprom->len);i++) {
		/* erase byte */
		if ((ret=smc911x_ethtool_write_eeprom_cmd(dev, E2P_CMD_EPC_CMD_ERASE_, i ))!=0)
			return ret;
		/* write byte */
		if ((ret=smc911x_ethtool_write_eeprom_byte(dev, *data))!=0)
			 return ret;
		if ((ret=smc911x_ethtool_write_eeprom_cmd(dev, E2P_CMD_EPC_CMD_WRITE_, i ))!=0)
			return ret;
		}
	 return 0;
}

static int smc911x_ethtool_geteeprom_len(struct net_device *dev)
{
	 return SMC911X_EEPROM_LEN;
}

static const struct ethtool_ops smc911x_ethtool_ops = {
	.get_settings	 = smc911x_ethtool_getsettings,
	.set_settings	 = smc911x_ethtool_setsettings,
	.get_drvinfo	 = smc911x_ethtool_getdrvinfo,
	.get_msglevel	 = smc911x_ethtool_getmsglevel,
	.set_msglevel	 = smc911x_ethtool_setmsglevel,
	.nway_reset = smc911x_ethtool_nwayreset,
	.get_link	 = ethtool_op_get_link,
	.get_regs_len	 = smc911x_ethtool_getregslen,
	.get_regs	 = smc911x_ethtool_getregs,
	.get_eeprom_len = smc911x_ethtool_geteeprom_len,
	.get_eeprom = smc911x_ethtool_geteeprom,
	.set_eeprom = smc911x_ethtool_seteeprom,
};

#if 0

/*
 * smc911x_findirq
 *
 * This routine has a simple purpose -- make the SMC chip generate an
 * interrupt, so an auto-detect routine can detect it, and find the IRQ,
 */
static int __devinit smc911x_findirq(struct net_device *dev)
{
	struct smc911x_local *lp = netdev_priv(dev);
	int timeout = 20;
	unsigned long cookie;

	DBG(SMC_DEBUG_FUNC, "--> %s\n", __func__);

	cookie = probe_irq_on();

	/*
	 * Force a SW interrupt
	 */

	SMC_SET_INT_EN(lp, INT_EN_SW_INT_EN_);

	/*
	 * Wait until positive that the interrupt has been generated
	 */
	do {
		int int_status;
		udelay(10);
		int_status = SMC_GET_INT_EN(lp);
		if (int_status & INT_EN_SW_INT_EN_)
			 break;		/* got the interrupt */
	} while (--timeout);

	/*
	 * there is really nothing that I can do here if timeout fails,
	 * as autoirq_report will return a 0 anyway, which is what I
	 * want in this case.	 Plus, the clean up is needed in both
	 * cases.
	 */

	/* and disable all interrupts again */
	SMC_SET_INT_EN(lp, 0);

	/* and return what I found */
	return probe_irq_off(cookie);
}
#endif

static const struct net_device_ops smc911x_netdev_ops = {
	.ndo_open		= smc911x_open,
	.ndo_stop		= smc911x_close,
	.ndo_start_xmit		= smc911x_hard_start_xmit,
#if 0
	.ndo_tx_timeout		= smc911x_timeout,
	.ndo_set_multicast_list	= smc911x_set_multicast_list,
	.ndo_change_mtu		= eth_change_mtu,
	.ndo_validate_addr	= eth_validate_addr,
	.ndo_set_mac_address	= eth_mac_addr,
#ifdef CONFIG_NET_POLL_CONTROLLER
	.ndo_poll_controller	= smc911x_poll_controller,
#endif
#endif
};


static int smc911x_probe(struct net_device *dev)
{
        struct smc911x_local *lp = netdev_priv(dev);
        int i, retval = 0;
        unsigned int val, chip_id, revision;
        const char *version_string;
        unsigned long irq_flags;

	DBG(SMC_DEBUG_FUNC, "%s: --> %s\n", dev->name, __func__);

	/* First, see if the endian word is recognized */
	val = SMC_GET_BYTE_TEST(lp);
	DBG(SMC_DEBUG_MISC, "%s: endian probe returned 0x%04x\n", CARDNAME, val);
	if (val != 0x87654321) {
		printk(KERN_ERR "Invalid chip endian 0x%08x\n",val);
		retval = VMM_ENODEV;
		goto err_out;
	}

	/*
	 * check if the revision register is something that I
	 * recognize.	These might need to be added to later,
	 * as future revisions could be added.
	 */
	chip_id = SMC_GET_PN(lp);
	DBG(SMC_DEBUG_MISC, "%s: id probe returned 0x%04x\n", CARDNAME, chip_id);
	for(i=0;chip_ids[i].id != 0; i++) {
		if (chip_ids[i].id == chip_id) break;
	}
	if (!chip_ids[i].id) {
		printk(KERN_ERR "Unknown chip ID %04x\n", chip_id);
		retval = VMM_ENODEV;
		goto err_out;
	}
	version_string = chip_ids[i].name;

	revision = SMC_GET_REV(lp);
	DBG(SMC_DEBUG_MISC, "%s: revision = 0x%04x\n", CARDNAME, revision);

	/* At this point I'll assume that the chip is an SMC911x. */
	DBG(SMC_DEBUG_MISC, "%s: Found a %s\n", CARDNAME, chip_ids[i].name);

	/* Validate the TX FIFO size requested */
	if ((tx_fifo_kb < 2) || (tx_fifo_kb > 14)) {
		printk(KERN_ERR "Invalid TX FIFO size requested %d\n", tx_fifo_kb);
		retval = VMM_EINVALID;
		goto err_out;
	}

	/* fill in some of the fields */
	lp->version = chip_ids[i].id;
	lp->revision = revision;
	lp->tx_fifo_kb = tx_fifo_kb;
	/* Reverse calculate the RX FIFO size from the TX */
	lp->tx_fifo_size=(lp->tx_fifo_kb<<10) - 512;
	lp->rx_fifo_size= ((0x4000 - 512 - lp->tx_fifo_size) / 16) * 15;

	/* Set the automatic flow control values */
	switch(lp->tx_fifo_kb) {
		/*
		 *	 AFC_HI is about ((Rx Data Fifo Size)*2/3)/64
		 *	 AFC_LO is AFC_HI/2
		 *	 BACK_DUR is about 5uS*(AFC_LO) rounded down
		 */
		case 2:/* 13440 Rx Data Fifo Size */
			lp->afc_cfg=0x008C46AF;break;
		case 3:/* 12480 Rx Data Fifo Size */
			lp->afc_cfg=0x0082419F;break;
		case 4:/* 11520 Rx Data Fifo Size */
			lp->afc_cfg=0x00783C9F;break;
		case 5:/* 10560 Rx Data Fifo Size */
			lp->afc_cfg=0x006E374F;break;
		case 6:/* 9600 Rx Data Fifo Size */
			lp->afc_cfg=0x0064328F;break;
		case 7:/* 8640 Rx Data Fifo Size */
			lp->afc_cfg=0x005A2D7F;break;
		case 8:/* 7680 Rx Data Fifo Size */
			lp->afc_cfg=0x0050287F;break;
		case 9:/* 6720 Rx Data Fifo Size */
			lp->afc_cfg=0x0046236F;break;
		case 10:/* 5760 Rx Data Fifo Size */
			lp->afc_cfg=0x003C1E6F;break;
		case 11:/* 4800 Rx Data Fifo Size */
			lp->afc_cfg=0x0032195F;break;
		/*
		 *	 AFC_HI is ~1520 bytes less than RX Data Fifo Size
		 *	 AFC_LO is AFC_HI/2
		 *	 BACK_DUR is about 5uS*(AFC_LO) rounded down
		 */
		case 12:/* 3840 Rx Data Fifo Size */
			lp->afc_cfg=0x0024124F;break;
		case 13:/* 2880 Rx Data Fifo Size */
			lp->afc_cfg=0x0015073F;break;
		case 14:/* 1920 Rx Data Fifo Size */
			lp->afc_cfg=0x0006032F;break;
		 default:
			 PRINTK("%s: ERROR -- no AFC_CFG setting found",
				dev->name);
			 break;
	}

	DBG(SMC_DEBUG_MISC | SMC_DEBUG_TX | SMC_DEBUG_RX,
		"%s: tx_fifo %d rx_fifo %d afc_cfg 0x%08x\n", CARDNAME,
		lp->tx_fifo_size, lp->rx_fifo_size, lp->afc_cfg);

	spin_lock_init(&lp->lock);

	/* Get the MAC address */
	SMC_GET_MAC_ADDR(lp, dev->dev_addr);

	/* now, reset the chip, and put it into a known state */
	smc911x_reset(dev);

	/*
	 * If dev->irq is 0, then the device has to be banged on to see
	 * what the IRQ is.
	 *
	 * Specifying an IRQ is done with the assumption that the user knows
	 * what (s)he is doing.  No checking is done!!!!
	 */
#if 0
	if (dev->irq < 1) {
		int trials;

		trials = 3;
		while (trials--) {
			dev->irq = smc911x_findirq(dev);
			if (dev->irq)
				break;
			/* kick the card and try again */
			smc911x_reset(dev);
		}
	}
	if (dev->irq == 0) {
		printk("%s: Couldn't autodetect your IRQ. Use irq=xx.\n",
			dev->name);
		retval = -ENODEV;
		goto err_out;
	}
	dev->irq = irq_canonicalize(dev->irq);
#endif

	/* Fill in the fields of the device structure with ethernet values. */
	ether_setup(dev);

	dev->netdev_ops = &smc911x_netdev_ops;
#if 0
	dev->watchdog_timeo = msecs_to_jiffies(watchdog);
#endif
	dev->ethtool_ops = &smc911x_ethtool_ops;
#if 0
	INIT_WORK(&lp->phy_configure, smc911x_phy_configure);
#endif
	lp->mii.phy_id_mask = 0x1f;
	lp->mii.reg_num_mask = 0x1f;
	lp->mii.force_media = 0;
	lp->mii.full_duplex = 0;
	lp->mii.dev = dev;
	lp->mii.mdio_read = smc911x_phy_read;
	lp->mii.mdio_write = smc911x_phy_write;

	/*
	 * Locate the phy, if any.
	 */
	smc911x_phy_detect(dev);

	/* Set default parameters */
	// Fixme: lp->msg_enable = NETIF_MSG_LINK;
	lp->ctl_rfduplx = 1;
	lp->ctl_rspeed = 100;

#ifdef SMC_DYNAMIC_BUS_CONFIG
	irq_flags = lp->cfg.irq_flags;
#else
	irq_flags = IRQF_SHARED | SMC_IRQ_SENSE;
#endif

	/* Grab the IRQ */
	retval = request_irq(dev->irq, &smc911x_interrupt,
			     irq_flags, dev->name, dev);
	if (retval)
		goto err_out;

#ifdef SMC_USE_DMA
	lp->rxdma = SMC_DMA_REQUEST(dev, smc911x_rx_dma_irq);
	lp->txdma = SMC_DMA_REQUEST(dev, smc911x_tx_dma_irq);
	lp->rxdma_active = 0;
	lp->txdma_active = 0;
	dev->dma = lp->rxdma;
#endif

	retval = register_netdev(dev);
	if (retval == VMM_OK) {
		/* now, print out the card info, in a short format.. */
		printk("%s: %s (rev %d) IRQ %02X",
			dev->name, version_string, lp->revision,
			dev->irq);

#ifdef SMC_USE_DMA
		if (lp->rxdma != -1)
			printk(" RXDMA %d ", lp->rxdma);

		if (lp->txdma != -1)
			printk("TXDMA %d", lp->txdma);
#endif
		printk("\n");
		if (!is_valid_ether_addr(dev->dev_addr)) {
			printk("%s: Invalid ethernet MAC address. Please "
					"set using ifconfig\n", dev->name);
		} else {
			/* Print the Ethernet address */
			printk("%s: Ethernet addr: ", dev->name);
			for (i = 0; i < 5; i++)
				printk("%02X:", dev->dev_addr[i]);
			printk("%02X\n", dev->dev_addr[5]);
		}

		if (lp->phy_type == 0) {
			PRINTK("%s: No PHY found\n", dev->name);
		} else if ((lp->phy_type & ~0xff) == LAN911X_INTERNAL_PHY_ID) {
			PRINTK("%s: LAN911x Internal PHY\n", dev->name);
		} else {
			PRINTK("%s: External PHY 0x%08x\n", dev->name, lp->phy_type);
		}
	}

err_out:
#ifdef SMC_USE_DMA
	if (retval) {
		if (lp->rxdma != -1) {
			SMC_DMA_FREE(dev, lp->rxdma);
		}
		if (lp->txdma != -1) {
			SMC_DMA_FREE(dev, lp->txdma);
		}
	}
#endif
	return retval;
}

static int smc911x_driver_probe(struct vmm_device *dev,
				const struct vmm_devtree_nodeid *devid)
{
	int rc = VMM_OK;
	struct net_device *ndev;
	virtual_addr_t addr;
	struct smc911x_local *lp;

	ndev = alloc_etherdev(sizeof(struct smc911x_local));
	if (!ndev) {
		vmm_printf("%s Failed to allocate netdev for %s\n", __func__,
				dev->node->name);
		rc = VMM_EFAIL;
		goto exit_probe;
	}


	dev->priv = (void *) ndev;
	ndev->vmm_dev = dev;
	strcpy(ndev->name, dev->node->name);

	lp = netdev_priv(ndev);
	lp->netdev = ndev;

	rc = vmm_devtree_regmap(dev->node, &addr, 0);
	if (rc) {
		vmm_printf("Failed to ioreamp\n");
		goto free_ndev;
	}

	DBG(SMC_DEBUG_MISC, "vmm_devtree_regmap success at address 0x%02X\n",
									addr);
	lp->base = (void *) addr;

	rc = vmm_devtree_irq_get(dev->node, &ndev->irq, 0);
	if (rc) {
		rc = VMM_EFAIL;
		goto free_ioreamp_mem;
	}

	DBG(SMC_DEBUG_MISC, "%s IRQ  0x%02X\n", ndev->name, ndev->irq);

	if(vmm_devtree_getattr(dev->node, "smsc,irq-active-high")) {
		lp->cfg.irq_polarity = SMSC911X_IRQ_POLARITY_ACTIVE_HIGH;
		DBG(SMC_DEBUG_MISC, "%s IRQ polarity is high\n", ndev->name);
	} else {
		lp->cfg.irq_polarity = SMSC911X_IRQ_POLARITY_ACTIVE_LOW;
		DBG(SMC_DEBUG_MISC, "%s IRQ polarity is low\n", ndev->name);
	}

#ifdef SMC_DYNAMIC_BUS_CONFIG
        lp->cfg.flags = SMC911X_USE_32BIT;
#endif

	rc = smc911x_probe(ndev);

	return rc;

free_ioreamp_mem:
free_ndev:
	vmm_free(lp);
	vmm_free(ndev);
exit_probe:
	return rc;
}

static int smc911x_driver_remove(struct vmm_device *dev)
{
	int rc = VMM_OK;
	struct net_device *ndev = (struct net_device *) dev->priv;

	if (ndev) {
		rc = netdev_unregister(ndev);
		vmm_free(ndev->priv);
		vmm_free(ndev);
		dev->priv = NULL;
	}

	return rc;
}

static struct vmm_devtree_nodeid smc911x_devid_table[] = {
	{ .type = "nic", .compatible = "smc911x"},
	{ /* end of list */ },
};

static struct vmm_driver smc911x_driver = {
	.name = "smc911x_driver",
	.match_table = smc911x_devid_table,
	.probe = smc911x_driver_probe,
	.remove = smc911x_driver_remove,
};

static int __init smc911x_driver_init(void)
{
	return vmm_devdrv_register_driver(&smc911x_driver);
}

static void __exit smc911x_driver_exit(void)
{
	vmm_devdrv_unregister_driver(&smc911x_driver);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
