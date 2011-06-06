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
 * @file ne2k.h
 * @version 0.1
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief Header file for ne2k based cards.
 */

#ifndef __NE_2K_H_
#define __NE_2K_H_

#include <vmm_netdev.h>

/*
 * Based on sources from U-Boot. There is very little documentation
 * on NE2000 based NICs.
 */

/* Some generic ethernet register configurations. */
#define E8390_TX_IRQ_MASK	0xa	/* For register EN0_ISR */
#define E8390_RX_IRQ_MASK	0x5
#define E8390_RXCONFIG		0x4	/* EN0_RXCR: broadcasts, no multicast,errors */
#define E8390_RXOFF		0x20	/* EN0_RXCR: Accept no packets */
#define E8390_TXCONFIG		0x00	/* EN0_TXCR: Normal transmit mode */
#define E8390_TXOFF		0x02	/* EN0_TXCR: Transmitter off */

/*  Register accessed at EN_CMD, the 8390 base addr.  */
#define E8390_STOP	0x01	/* Stop and reset the chip */
#define E8390_START	0x02	/* Start the chip, clear reset */
#define E8390_TRANS	0x04	/* Transmit a frame */
#define E8390_RREAD	0x08	/* Remote read */
#define E8390_RWRITE	0x10	/* Remote write  */
#define E8390_NODMA	0x20	/* Remote DMA */
#define E8390_PAGE0	0x00	/* Select page chip registers */
#define E8390_PAGE1	0x40	/* using the two high-order bits */
#define E8390_PAGE2	0x80	/* Page 3 is invalid. */

#define EI_SHIFT(x)	(x)

#define E8390_CMD	EI_SHIFT(0x00)  /* The command register (for all pages) */
/* Page 0 register offsets. */
#define EN0_CLDALO	EI_SHIFT(0x01)	/* Low byte of current local dma addr  RD */
#define EN0_STARTPG	EI_SHIFT(0x01)	/* Starting page of ring bfr WR */
#define EN0_CLDAHI	EI_SHIFT(0x02)	/* High byte of current local dma addr  RD */
#define EN0_STOPPG	EI_SHIFT(0x02)	/* Ending page +1 of ring bfr WR */
#define EN0_BOUNDARY	EI_SHIFT(0x03)	/* Boundary page of ring bfr RD WR */
#define EN0_TSR		EI_SHIFT(0x04)	/* Transmit status reg RD */
#define EN0_TPSR	EI_SHIFT(0x04)	/* Transmit starting page WR */
#define EN0_NCR		EI_SHIFT(0x05)	/* Number of collision reg RD */
#define EN0_TCNTLO	EI_SHIFT(0x05)	/* Low  byte of tx byte count WR */
#define EN0_FIFO	EI_SHIFT(0x06)	/* FIFO RD */
#define EN0_TCNTHI	EI_SHIFT(0x06)	/* High byte of tx byte count WR */
#define EN0_ISR		EI_SHIFT(0x07)	/* Interrupt status reg RD WR */
#define EN0_CRDALO	EI_SHIFT(0x08)	/* low byte of current remote dma address RD */
#define EN0_RSARLO	EI_SHIFT(0x08)	/* Remote start address reg 0 */
#define EN0_CRDAHI	EI_SHIFT(0x09)	/* high byte, current remote dma address RD */
#define EN0_RSARHI	EI_SHIFT(0x09)	/* Remote start address reg 1 */
#define EN0_RCNTLO	EI_SHIFT(0x0a)	/* Remote byte count reg WR */
#define EN0_RCNTHI	EI_SHIFT(0x0b)	/* Remote byte count reg WR */
#define EN0_RSR		EI_SHIFT(0x0c)	/* rx status reg RD */
#define EN0_RXCR	EI_SHIFT(0x0c)	/* RX configuration reg WR */
#define EN0_TXCR	EI_SHIFT(0x0d)	/* TX configuration reg WR */
#define EN0_COUNTER0	EI_SHIFT(0x0d)	/* Rcv alignment error counter RD */
#define EN0_DCFG	EI_SHIFT(0x0e)	/* Data configuration reg WR */
#define EN0_COUNTER1	EI_SHIFT(0x0e)	/* Rcv CRC error counter RD */
#define EN0_IMR		EI_SHIFT(0x0f)	/* Interrupt mask reg WR */
#define EN0_COUNTER2	EI_SHIFT(0x0f)	/* Rcv missed frame error counter RD */

/* Bits in EN0_ISR - Interrupt status register */
#define ENISR_RX	0x01	/* Receiver, no error */
#define ENISR_TX	0x02	/* Transmitter, no error */
#define ENISR_RX_ERR	0x04	/* Receiver, with error */
#define ENISR_TX_ERR	0x08	/* Transmitter, with error */
#define ENISR_OVER	0x10	/* Receiver overwrote the ring */
#define ENISR_COUNTERS	0x20	/* Counters need emptying */
#define ENISR_RDC	0x40	/* remote dma complete */
#define ENISR_RESET	0x80	/* Reset completed */
#define ENISR_ALL	0x3f	/* Interrupts we will enable */

/* Bits in EN0_DCFG - Data config register */
#define ENDCFG_WTS	0x01	/* word transfer mode selection */
#define ENDCFG_BOS	0x02	/* byte order selection */
#define ENDCFG_AUTO_INIT   0x10        /* Auto-init to remove packets from ring */
#define ENDCFG_FIFO		0x40	/* 8 bytes */

/* Page 1 register offsets. */
#define EN1_PHYS   EI_SHIFT(0x01)	/* This board's physical enet addr RD WR */
#define EN1_PHYS_SHIFT(i)  EI_SHIFT(i+1) /* Get and set mac address */
#define EN1_CURPAG EI_SHIFT(0x07)	/* Current memory page RD WR */
#define EN1_MULT   EI_SHIFT(0x08)	/* Multicast filter mask array (8 bytes) RD WR */
#define EN1_MULT_SHIFT(i)  EI_SHIFT(8+i) /* Get and set multicast filter */

/* Bits in received packet status byte and EN0_RSR*/
#define ENRSR_RXOK	0x01	/* Received a good packet */
#define ENRSR_CRC	0x02	/* CRC error */
#define ENRSR_FAE	0x04	/* frame alignment error */
#define ENRSR_FO	0x08	/* FIFO overrun */
#define ENRSR_MPA	0x10	/* missed pkt */
#define ENRSR_PHY	0x20	/* physical/multicast address */
#define ENRSR_DIS	0x40	/* receiver disable. set in monitor mode */
#define ENRSR_DEF	0x80	/* deferring */

/* Transmitted packet status, EN0_TSR. */
#define ENTSR_PTX 0x01	/* Packet transmitted without error */
#define ENTSR_ND  0x02	/* The transmit wasn't deferred. */
#define ENTSR_COL 0x04	/* The transmit collided at least once. */
#define ENTSR_ABT 0x08  /* The transmit collided 16 times, and was deferred. */
#define ENTSR_CRS 0x10	/* The carrier sense was lost. */
#define ENTSR_FU  0x20  /* A "FIFO underrun" occurred during transmit. */
#define ENTSR_CDH 0x40	/* The collision detect "heartbeat" signal was lost. */
#define ENTSR_OWC 0x80  /* There was an out-of-window collision. */

/* from ne2000.h */
#define DP_DATA		0x10
#define START_PG	0x50	/* First page of TX buffer */
#define START_PG2	0x48
#define STOP_PG		0x80	/* Last page +1 of RX ring */

#define RX_START	0x50
#define RX_END		0x80

#define DP_IN(_b_, _o_, _d_)	(_d_) = *( (volatile unsigned char *) ((_b_)+(_o_)))
#define DP_OUT(_b_, _o_, _d_)	*( (volatile unsigned char *) ((_b_)+(_o_))) = (_d_)
#define DP_IN_DATA(_b_, _d_)	(_d_) = *( (volatile unsigned char *) ((_b_)))
#define DP_OUT_DATA(_b_, _d_)	*( (volatile unsigned char *) ((_b_))) = (_d_)

#define bool int
#define false 0
#define true 1

/* timeout for tx/rx in s */
#define TOUT 5
/* Ether MAC address size */
#define ETHER_ADDR_LEN 6


#define CYGHWR_NS_DP83902A_PLF_BROKEN_TX_DMA 1
#define CYGACC_CALL_IF_DELAY_US(X) udelay(X)

/* H/W infomation struct */
typedef struct hw_info_t {
	char *dev_name;
	u32 offset;
	u8 a0, a1, a2;
	u32 flags;
} hw_info_t;

typedef struct nic_priv_data {
	u8* base;
	u8* data;
	u8* reset;
	void *rx_rb; /* receive data ring buffer */
	vmm_netdev_t *parent;	/* Parent driver framework */
	int tx_next;		/* First free Tx page */
	int tx_int;		/* Expecting interrupt from this buffer */
	int rx_next;		/* First free Rx page */
	int tx1, tx2;		/* Page numbers for Tx buffers */
	u32 tx1_key, tx2_key;	/* Used to ack when packet sent */
	int tx1_len, tx2_len;
	bool tx_started, running, hardwired_esa;
	u8 esa[6];
	void* plf_priv;

	/* Buffer allocation */
	int tx_buf1, tx_buf2;
	int rx_buf_start, rx_buf_end;
	int initialized;
	vmm_hyperthread_t *txrx_thread;
} nic_priv_data_t;

/* ------------------------------------------------------------------------ */
/* Register offsets */

#define DP_CR		0x00
#define DP_CLDA0	0x01
#define DP_PSTART	0x01	/* write */
#define DP_CLDA1	0x02
#define DP_PSTOP	0x02	/* write */
#define DP_BNDRY	0x03
#define DP_TSR		0x04
#define DP_TPSR		0x04	/* write */
#define DP_NCR		0x05
#define DP_TBCL		0x05	/* write */
#define DP_FIFO		0x06
#define DP_TBCH		0x06	/* write */
#define DP_ISR		0x07
#define DP_CRDA0	0x08
#define DP_RSAL		0x08	/* write */
#define DP_CRDA1	0x09
#define DP_RSAH		0x09	/* write */
#define DP_RBCL		0x0a	/* write */
#define DP_RBCH		0x0b	/* write */
#define DP_RSR		0x0c
#define DP_RCR		0x0c	/* write */
#define DP_FER		0x0d
#define DP_TCR		0x0d	/* write */
#define DP_CER		0x0e
#define DP_DCR		0x0e	/* write */
#define DP_MISSED	0x0f
#define DP_IMR		0x0f	/* write */
#define DP_DATAPORT	0x10	/* "eprom" data port */

#define DP_P1_CR	0x00
#define DP_P1_PAR0	0x01
#define DP_P1_PAR1	0x02
#define DP_P1_PAR2	0x03
#define DP_P1_PAR3	0x04
#define DP_P1_PAR4	0x05
#define DP_P1_PAR5	0x06
#define DP_P1_CURP	0x07
#define DP_P1_MAR0	0x08
#define DP_P1_MAR1	0x09
#define DP_P1_MAR2	0x0a
#define DP_P1_MAR3	0x0b
#define DP_P1_MAR4	0x0c
#define DP_P1_MAR5	0x0d
#define DP_P1_MAR6	0x0e
#define DP_P1_MAR7	0x0f

#define DP_P2_CR	0x00
#define DP_P2_PSTART	0x01
#define DP_P2_CLDA0	0x01	/* write */
#define DP_P2_PSTOP	0x02
#define DP_P2_CLDA1	0x02	/* write */
#define DP_P2_RNPP	0x03
#define DP_P2_TPSR	0x04
#define DP_P2_LNPP	0x05
#define DP_P2_ACH	0x06
#define DP_P2_ACL	0x07
#define DP_P2_RCR	0x0c
#define DP_P2_TCR	0x0d
#define DP_P2_DCR	0x0e
#define DP_P2_IMR	0x0f

/* Command register - common to all pages */

#define DP_CR_STOP	0x01	/* Stop: software reset */
#define DP_CR_START	0x02	/* Start: initialize device */
#define DP_CR_TXPKT	0x04	/* Transmit packet */
#define DP_CR_RDMA	0x08	/* Read DMA (recv data from device) */
#define DP_CR_WDMA	0x10	/* Write DMA (send data to device) */
#define DP_CR_SEND	0x18	/* Send packet */
#define DP_CR_NODMA	0x20	/* Remote (or no) DMA */
#define DP_CR_PAGE0	0x00	/* Page select */
#define DP_CR_PAGE1	0x40
#define DP_CR_PAGE2	0x80
#define DP_CR_PAGEMSK	0x3F	/* Used to mask out page bits */

/* Data configuration register */

#define DP_DCR_WTS	0x01	/* 1=16 bit word transfers */
#define DP_DCR_BOS	0x02	/* 1=Little Endian */
#define DP_DCR_LAS	0x04	/* 1=Single 32 bit DMA mode */
#define DP_DCR_LS	0x08	/* 1=normal mode, 0=loopback */
#define DP_DCR_ARM	0x10	/* 0=no send command (program I/O) */
#define DP_DCR_FIFO_1	0x00	/* FIFO threshold */
#define DP_DCR_FIFO_2	0x20
#define DP_DCR_FIFO_4	0x40
#define DP_DCR_FIFO_6	0x60

#define DP_DCR_INIT	(DP_DCR_LS|DP_DCR_FIFO_4)

/* Interrupt status register */

#define DP_ISR_RxP	0x01	/* Packet received */
#define DP_ISR_TxP	0x02	/* Packet transmitted */
#define DP_ISR_RxE	0x04	/* Receive error */
#define DP_ISR_TxE	0x08	/* Transmit error */
#define DP_ISR_OFLW	0x10	/* Receive overflow */
#define DP_ISR_CNT	0x20	/* Tally counters need emptying */
#define DP_ISR_RDC	0x40	/* Remote DMA complete */
#define DP_ISR_RESET	0x80	/* Device has reset (shutdown, error) */

/* Interrupt mask register */

#define DP_IMR_RxP	0x01	/* Packet received */
#define DP_IMR_TxP	0x02	/* Packet transmitted */
#define DP_IMR_RxE	0x04	/* Receive error */
#define DP_IMR_TxE	0x08	/* Transmit error */
#define DP_IMR_OFLW	0x10	/* Receive overflow */
#define DP_IMR_CNT	0x20	/* Tall counters need emptying */
#define DP_IMR_RDC	0x40	/* Remote DMA complete */

#define DP_IMR_All	0x3F	/* Everything but remote DMA */

/* Receiver control register */

#define DP_RCR_SEP	0x01	/* Save bad(error) packets */
#define DP_RCR_AR	0x02	/* Accept runt packets */
#define DP_RCR_AB	0x04	/* Accept broadcast packets */
#define DP_RCR_AM	0x08	/* Accept multicast packets */
#define DP_RCR_PROM	0x10	/* Promiscuous mode */
#define DP_RCR_MON	0x20	/* Monitor mode - 1=accept no packets */

/* Receiver status register */

#define DP_RSR_RxP	0x01	/* Packet received */
#define DP_RSR_CRC	0x02	/* CRC error */
#define DP_RSR_FRAME	0x04	/* Framing error */
#define DP_RSR_FO	0x08	/* FIFO overrun */
#define DP_RSR_MISS	0x10	/* Missed packet */
#define DP_RSR_PHY	0x20	/* 0=pad match, 1=mad match */
#define DP_RSR_DIS	0x40	/* Receiver disabled */
#define DP_RSR_DFR	0x80	/* Receiver processing deferred */

/* Transmitter control register */

#define DP_TCR_NOCRC	0x01	/* 1=inhibit CRC */
#define DP_TCR_NORMAL	0x00	/* Normal transmitter operation */
#define DP_TCR_LOCAL	0x02	/* Internal NIC loopback */
#define DP_TCR_INLOOP	0x04	/* Full internal loopback */
#define DP_TCR_OUTLOOP	0x08	/* External loopback */
#define DP_TCR_ATD	0x10	/* Auto transmit disable */
#define DP_TCR_OFFSET	0x20	/* Collision offset adjust */

/* Transmit status register */

#define DP_TSR_TxP	0x01	/* Packet transmitted */
#define DP_TSR_COL	0x04	/* Collision (at least one) */
#define DP_TSR_ABT	0x08	/* Aborted because of too many collisions */
#define DP_TSR_CRS	0x10	/* Lost carrier */
#define DP_TSR_FU	0x20	/* FIFO underrun */
#define DP_TSR_CDH	0x40	/* Collision Detect Heartbeat */
#define DP_TSR_OWC	0x80	/* Collision outside normal window */

#define IEEE_8023_MAX_FRAME	1518	/* Largest possible ethernet frame */
#define IEEE_8023_MIN_FRAME	64	/* Smallest possible ethernet frame */

/* Functions */
int get_prom(nic_priv_data_t *dp, u8* mac_addr);

#endif /* __NE_2K_H_ */
