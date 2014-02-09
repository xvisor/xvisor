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
 * @file lan9118.c
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief SMSC LAN 9118 Ethernet Interface Emulator.
 *
 * The source has been largely adapted from QEMU hw/lan9118.c 
 *
 * SMSC LAN9118 Ethernet interface emulation
 *
 * Copyright (c) 2009 CodeSourcery, LLC.
 * Written by Paul Brook
 *
 * This code is licensed under the GNU GPL v2
 *
 * Contributions after 2012-01-13 are licensed under the terms of the
 * GNU GPL, version 2 or (at your option) any later version.
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_timer.h>
#include <vmm_modules.h>
#include <vmm_devtree.h>
#include <vmm_devemu.h>
#include <vmm_stdio.h>
#include <net/vmm_protocol.h>
#include <net/vmm_mbuf.h>
#include <net/vmm_net.h>
#include <net/vmm_netswitch.h>
#include <net/vmm_netport.h>
#include <libs/mathlib.h>

#define MODULE_DESC			"SMSC LAN9118 Emulator"
#define MODULE_AUTHOR			"Sukanto Ghosh"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(VMM_NET_CLASS_IPRIORITY+1)
#define	MODULE_INIT			lan9118_emulator_init
#define	MODULE_EXIT			lan9118_emulator_exit

static inline u32 bswap32(u32 data)
{
	return (((data & 0xFF) << 24) |
		((data & 0xFF00) << 8) |
		((data & 0xFF0000) >> 8) |
		((data & 0xFF000000) >> 24));
}

#define crc32(seed, data, length)  crc32_le(seed, (unsigned char const *)(data), length)
static u32 crc32_le(u32 crc, unsigned char const *p, u32 len)
{
	int i;
	while (len--) {
		crc ^= *p++;
		for (i = 0; i < 8; i++)
			crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320 : 0);
	}
	return crc;
}

#undef DEBUG_LAN9118

#ifdef DEBUG_LAN9118
#define DPRINTF(fmt, ...) \
	do { vmm_printf(fmt , ## __VA_ARGS__); } while (0)
#define BADF(fmt, ...) \
	do { vmm_printf("lan9118: error: " fmt , ## __VA_ARGS__);} while (0)
#else
#define DPRINTF(fmt, ...) do {} while(0)
#define BADF(fmt, ...) \
	do { vmm_printf("lan9118: error: " fmt , ## __VA_ARGS__);} while (0)
#endif

#define CSR_ID_REV      0x50
#define CSR_IRQ_CFG     0x54
#define CSR_INT_STS     0x58
#define CSR_INT_EN      0x5c
#define CSR_BYTE_TEST   0x64
#define CSR_FIFO_INT    0x68
#define CSR_RX_CFG      0x6c
#define CSR_TX_CFG      0x70
#define CSR_HW_CFG      0x74
#define CSR_RX_DP_CTRL  0x78
#define CSR_RX_FIFO_INF 0x7c
#define CSR_TX_FIFO_INF 0x80
#define CSR_PMT_CTRL    0x84
#define CSR_GPIO_CFG    0x88
#define CSR_GPT_CFG     0x8c
#define CSR_GPT_CNT     0x90
#define CSR_WORD_SWAP   0x98
#define CSR_FREE_RUN    0x9c
#define CSR_RX_DROP     0xa0
#define CSR_MAC_CSR_CMD 0xa4
#define CSR_MAC_CSR_DATA 0xa8
#define CSR_AFC_CFG     0xac
#define CSR_E2P_CMD     0xb0
#define CSR_E2P_DATA    0xb4

/* IRQ_CFG */
#define IRQ_INT         0x00001000
#define IRQ_EN          0x00000100
#define IRQ_POL         0x00000010
#define IRQ_TYPE        0x00000001

/* INT_STS/INT_EN */
#define SW_INT          0x80000000
#define TXSTOP_INT      0x02000000
#define RXSTOP_INT      0x01000000
#define RXDFH_INT       0x00800000
#define TX_IOC_INT      0x00200000
#define RXD_INT         0x00100000
#define GPT_INT         0x00080000
#define PHY_INT         0x00040000
#define PME_INT         0x00020000
#define TXSO_INT        0x00010000
#define RWT_INT         0x00008000
#define RXE_INT         0x00004000
#define TXE_INT         0x00002000
#define TDFU_INT        0x00000800
#define TDFO_INT        0x00000400
#define TDFA_INT        0x00000200
#define TSFF_INT        0x00000100
#define TSFL_INT        0x00000080
#define RXDF_INT        0x00000040
#define RDFL_INT        0x00000020
#define RSFF_INT        0x00000010
#define RSFL_INT        0x00000008
#define GPIO2_INT       0x00000004
#define GPIO1_INT       0x00000002
#define GPIO0_INT       0x00000001
#define RESERVED_INT    0x7c001000

#define MAC_CR          1
#define MAC_ADDRH       2
#define MAC_ADDRL       3
#define MAC_HASHH       4
#define MAC_HASHL       5
#define MAC_MII_ACC     6
#define MAC_MII_DATA    7
#define MAC_FLOW        8
#define MAC_VLAN1       9 /* TODO */
#define MAC_VLAN2       10 /* TODO */
#define MAC_WUFF        11 /* TODO */
#define MAC_WUCSR       12 /* TODO */

#define MAC_CR_RXALL    0x80000000
#define MAC_CR_RCVOWN   0x00800000
#define MAC_CR_LOOPBK   0x00200000
#define MAC_CR_FDPX     0x00100000
#define MAC_CR_MCPAS    0x00080000
#define MAC_CR_PRMS     0x00040000
#define MAC_CR_INVFILT  0x00020000
#define MAC_CR_PASSBAD  0x00010000
#define MAC_CR_HO       0x00008000
#define MAC_CR_HPFILT   0x00002000
#define MAC_CR_LCOLL    0x00001000
#define MAC_CR_BCAST    0x00000800
#define MAC_CR_DISRTY   0x00000400
#define MAC_CR_PADSTR   0x00000100
#define MAC_CR_BOLMT    0x000000c0
#define MAC_CR_DFCHK    0x00000020
#define MAC_CR_TXEN     0x00000008
#define MAC_CR_RXEN     0x00000004
#define MAC_CR_RESERVED 0x7f404213

#define PHY_INT_ENERGYON            0x80
#define PHY_INT_AUTONEG_COMPLETE    0x40
#define PHY_INT_FAULT               0x20
#define PHY_INT_DOWN                0x10
#define PHY_INT_AUTONEG_LP          0x08
#define PHY_INT_PARFAULT            0x04
#define PHY_INT_AUTONEG_PAGE        0x02

#define GPT_TIMER_EN    0x20000000

enum tx_state {
	TX_IDLE,
	TX_B,
	TX_DATA
};

#define LAN9118_MTU	2048

struct LAN9118Packet{
	enum tx_state state;
	u32 cmd_a;
	u32 cmd_b;
	s32 buffer_size;
	s32 offset;
	s32 pad;
	s32 fifo_used;
	struct vmm_mbuf *mbuf;
};

struct lan9118_state {
	struct vmm_netport *port;

	vmm_spinlock_t lock;
	struct vmm_guest *guest;
	u32 irq;
	struct vmm_timer_event event;
	u16 gpt_count;
	bool link_down; 

	u32 irq_cfg;
	u32 int_sts;
	u32 int_en;
	u32 fifo_int;
	u32 rx_cfg;
	u32 tx_cfg;
	u32 hw_cfg;
	u32 pmt_ctrl;
	u32 gpio_cfg;
	u32 gpt_cfg;
	u32 word_swap;
	u64 gpt_start_tstamp;
	u64 free_timer_start_tstamp;
	u32 mac_cmd;
	u32 mac_data;
	u32 afc_cfg;
	u32 e2p_cmd;
	u32 e2p_data;

	u32 mac_cr;
	u32 mac_hashh;
	u32 mac_hashl;
	u32 mac_mii_acc;
	u32 mac_mii_data;
	u32 mac_flow;

	u32 phy_status;
	u32 phy_control;
	u32 phy_advertise;
	u32 phy_int;
	u32 phy_int_mask;

	s32 eeprom_writable;
	u8 eeprom[128];

	s32 tx_fifo_size;
	struct LAN9118Packet *txp;
	struct LAN9118Packet tx_packet;

	s32 tx_status_fifo_used;
	s32 tx_status_fifo_head;
	u32 tx_status_fifo[512];

	s32 rx_status_fifo_size;
	s32 rx_status_fifo_used;
	s32 rx_status_fifo_head;
	u32 rx_status_fifo[896];
	s32 rx_fifo_size;
	s32 rx_fifo_used;
	s32 rx_fifo_head;
	u32 rx_fifo[3360];
	s32 rx_packet_size_head;
	s32 rx_packet_size_tail;
	s32 rx_packet_size[1024];

	s32 rxp_offset;
	s32 rxp_size;
	s32 rxp_pad;

	u32 write_word_prev_offset;
	u32 write_word_n;
	u16 write_word_l;
	u16 write_word_h;
	u32 read_word_prev_offset;
	u32 read_word_n;
	u32 read_long;

	u32 mode_16bit;
};

#define txp_mbuf(s)	((s)->txp->mbuf)
#define txp_mbuf_len(s)	((s)->txp->mbuf->m_len)

static void lan9118_update(struct lan9118_state *s)
{
	int level;

	/* TODO: Implement FIFO level IRQs.  */
	level = (s->int_sts & s->int_en) != 0;
	if (level) {
		s->irq_cfg |= IRQ_INT;
	} else {
		s->irq_cfg &= ~IRQ_INT;
	}
	if ((s->irq_cfg & IRQ_EN) == 0) {
		level = 0;
	}
	if ((s->irq_cfg & (IRQ_TYPE | IRQ_POL)) != (IRQ_TYPE | IRQ_POL)) {
		/* Interrupt is active low unless we're configured as
		 * active-high polarity, push-pull type.
		 */
		level = !level;
	}
	vmm_devemu_emulate_irq(s->guest, s->irq, level);
}

static void gpt_reload(struct lan9118_state *s, bool preload)
{
	u64 nsecs;

	s->gpt_start_tstamp = vmm_timer_timestamp();

	if(preload) {
		nsecs = (s->gpt_count & 0xffff); 
	} else {
		s->gpt_count = nsecs = 0xffff;
	}

	nsecs *= 100000;   /* 10KHz */

	vmm_timer_event_stop(&s->event); 
	vmm_timer_event_start(&s->event, nsecs);
}


static u16 gpt_counter_value(struct lan9118_state *s)
{
	u16 ret = 0xffff;

	if(s->gpt_cfg & GPT_TIMER_EN)  {
		/* How much nsecs since the timer was started */
		u64 cval = vmm_timer_timestamp() - s->gpt_start_tstamp;
		/* LAN9118 GPT has 100us granularity i.e. 10KHz */
		cval = udiv64(cval, 100000ULL);
		/* Always in auto-reload mode */
		cval = umod64(cval, (u64)s->gpt_count);
		ret = s->gpt_count - (u32)cval;
	}

	return ret;
}

static void gpt_event(struct vmm_timer_event *event)
{
	struct lan9118_state *s = event->priv;
	gpt_reload(s, 0);
	if (s->int_en & GPT_INT) {
		s->int_sts |= GPT_INT;
	}
	lan9118_update(s);
}

static void lan9118_mac_changed(struct lan9118_state *s)
{
#if DEBUG_LAN9118
	char tname[30];
	DPRINTF("MAC of \"%s\" changed to [%s]\n", s->port->name,
		ethaddr_to_str(tname, vmm_netport_mac(s->port)));
#endif
}

static void lan9118_reload_eeprom(struct lan9118_state *s)
{
	int i;
	if (s->eeprom[0] != 0xa5) {
		s->e2p_cmd &= ~0x10;
		DPRINTF("MACADDR load failed\n");
		return;
	}
	for (i = 0; i < 6; i++) {
		vmm_netport_mac(s->port)[i] = s->eeprom[i + 1];
	}
	s->e2p_cmd |= 0x10;
	DPRINTF("MACADDR loaded from eeprom\n");
	lan9118_mac_changed(s);
}

static void phy_update_irq(struct lan9118_state *s)
{
	if (s->phy_int & s->phy_int_mask) {
		s->int_sts |= PHY_INT;
	} else {
		s->int_sts &= ~PHY_INT;
	}
	lan9118_update(s);
}

static void phy_update_link(struct lan9118_state *s)
{
	/* Autonegotiation status mirrors link status. */
	if (s->link_down) {
		s->phy_status &= ~0x0024;
		s->phy_int |= PHY_INT_DOWN;
	} else {
		s->phy_status |= 0x0024;
		s->phy_int |= PHY_INT_ENERGYON;
		s->phy_int |= PHY_INT_AUTONEG_COMPLETE;
	}
	phy_update_irq(s);
}

static void lan9118_set_link(struct vmm_netport *port)
{
	struct lan9118_state *s = (struct lan9118_state *)port->priv;
	s->link_down = !(port->flags & VMM_NETPORT_LINK_UP);
	phy_update_link(s);
}

static void phy_reset(struct lan9118_state *s)
{
	s->phy_status = 0x7809;
	s->phy_control = 0x3000;
	s->phy_advertise = 0x01e1;
	s->phy_int_mask = 0;
	s->phy_int = 0;
	phy_update_link(s);
}

static int lan9118_state_reset(struct lan9118_state *s)
{
	vmm_spin_lock(&s->lock);
	s->irq_cfg &= (IRQ_TYPE | IRQ_POL);
	s->int_sts = 0;
	s->int_en = 0;
	s->fifo_int = 0x48000000;
	s->rx_cfg = 0;
	s->tx_cfg = 0;
	/* TODO: Support 16bit mode - probably by reading DTS */
	s->mode_16bit = 0;
	s->hw_cfg = s->mode_16bit ? 0x00050000 : 0x00050004;
	s->pmt_ctrl &= 0x45;
	s->gpio_cfg = 0;
	s->txp->fifo_used = 0;
	s->txp->state = TX_IDLE;
	s->txp->cmd_a = 0xffffffffu;
	s->txp->cmd_b = 0xffffffffu;
	if (txp_mbuf(s)) {
		m_freem(txp_mbuf(s));
		txp_mbuf(s) = NULL;
	}
	MGETHDR(txp_mbuf(s), 0, 0);
	MEXTMALLOC(txp_mbuf(s), LAN9118_MTU, M_WAIT);
	s->txp->fifo_used = 0;
	s->tx_fifo_size = 4608;
	s->tx_status_fifo_used = 0;
	s->rx_status_fifo_size = 704;
	s->rx_fifo_size = 2640;
	s->rx_fifo_used = 0;
	s->rx_status_fifo_size = 176;
	s->rx_status_fifo_used = 0;
	s->rxp_offset = 0;
	s->rxp_size = 0;
	s->rxp_pad = 0;
	s->rx_packet_size_tail = s->rx_packet_size_head;
	s->rx_packet_size[s->rx_packet_size_head] = 0;
	s->mac_cmd = 0;
	s->mac_data = 0;
	s->afc_cfg = 0;
	s->e2p_cmd = 0;
	s->e2p_data = 0;
	s->free_timer_start_tstamp = vmm_timer_timestamp();

	vmm_timer_event_stop(&s->event);
	s->gpt_count = 0xffff;
	s->gpt_cfg = 0xffff;

	s->mac_cr = MAC_CR_PRMS;
	s->mac_hashh = 0;
	s->mac_hashl = 0;
	s->mac_mii_acc = 0;
	s->mac_mii_data = 0;
	s->mac_flow = 0;

	s->read_word_n = 0;
	s->write_word_n = 0;

	phy_reset(s);

	s->eeprom_writable = 0;
	lan9118_reload_eeprom(s);
	vmm_spin_unlock(&s->lock);

	return VMM_OK;
}

static int lan9118_emulator_reset(struct vmm_emudev *edev)
{
	struct lan9118_state *s = edev->priv;
	return lan9118_state_reset(s);
}

static void rx_fifo_push(struct lan9118_state *s, u32 val)
{
	int fifo_pos;
	fifo_pos = s->rx_fifo_head + s->rx_fifo_used;
	if (fifo_pos >= s->rx_fifo_size)
		fifo_pos -= s->rx_fifo_size;
	s->rx_fifo[fifo_pos] = val;
	s->rx_fifo_used++;
}

/* Return nonzero if the packet is accepted by the filter.  */
static int lan9118_filter(struct lan9118_state *s, const u8 *addr)
{
	int multicast;
	u32 hash;

	if (s->mac_cr & MAC_CR_PRMS) {
		return 1;
	}
	if (addr[0] == 0xff && addr[1] == 0xff && addr[2] == 0xff &&
			addr[3] == 0xff && addr[4] == 0xff && addr[5] == 0xff) {
		return (s->mac_cr & MAC_CR_BCAST) == 0;
	}

	multicast = addr[0] & 1;
	if (multicast &&s->mac_cr & MAC_CR_MCPAS) {
		return 1;
	}
	if (multicast ? (s->mac_cr & MAC_CR_HPFILT) == 0
			: (s->mac_cr & MAC_CR_HO) == 0) {
		/* Exact matching.  */
		hash = memcmp(addr, vmm_netport_mac(s->port), 6);
		if (s->mac_cr & MAC_CR_INVFILT) {
			return hash != 0;
		} else {
			return hash == 0;
		}
	} else {
		/* Hash matching  */
		hash = (crc32(~0, addr, 6) >> 26);
		if (hash & 0x20) {
			return (s->mac_hashh >> (hash & 0x1f)) & 1;
		} else {
			return (s->mac_hashl >> (hash & 0x1f)) & 1;
		}
	}
}

static int lan9118_can_receive(struct vmm_netport *port)
{
	struct lan9118_state *s = port->priv;
	return ((s->mac_cr & MAC_CR_RXEN) != 0);
}

static u32 lan9118_receive(struct lan9118_state *s, struct vmm_mbuf *mbuf)
{
	int fifo_len;
	int offset;
	int src_pos;
	int n;
	int filter;
	u32 val;
	u32 crc;
	u32 status;

	/* FIXME: Handle chained/fragmented mbufs */
	u8 *buf = mtod(mbuf, u8 *);
	u32 size = mbuf->m_len;

	if ((s->mac_cr & MAC_CR_RXEN) == 0) {
		return -1;
	}

	if (size >= 2048 || size < 14) {
		return -1;
	}

	/* TODO: Implement FIFO overflow notification.  */
	if (s->rx_status_fifo_used == s->rx_status_fifo_size) {
		return -1;
	}

	filter = lan9118_filter(s, buf);
	if (!filter && (s->mac_cr & MAC_CR_RXALL) == 0) {
		return size;
	}

	offset = (s->rx_cfg >> 8) & 0x1f;
	n = offset & 3;
	fifo_len = (size + n + 3) >> 2;
	/* Add a word for the CRC.  */
	fifo_len++;
	if (s->rx_fifo_size - s->rx_fifo_used < fifo_len) {
		return -1;
	}

	DPRINTF("Got packet len:%d fifo:%d filter:%s\n",
			(int)size, fifo_len, filter ? "pass" : "fail");
	val = 0;
	/* As a Emulator, we don't need to insert CRC
	 * crc = bswap32(crc32(~0, buf, size));
	 */
	crc = 0; /* CRC computation skipped !!! */
	for (src_pos = 0; src_pos < size; src_pos++) {
		val = (val >> 8) | ((u32)buf[src_pos] << 24);
		n++;
		if (n == 4) {
			n = 0;
			rx_fifo_push(s, val);
			val = 0;
		}
	}
	if (n) {
		val >>= ((4 - n) * 8);
		val |= crc << (n * 8);
		rx_fifo_push(s, val);
		val = crc >> ((4 - n) * 8);
		rx_fifo_push(s, val);
	} else {
		rx_fifo_push(s, crc);
	}
	n = s->rx_status_fifo_head + s->rx_status_fifo_used;
	if (n >= s->rx_status_fifo_size) {
		n -= s->rx_status_fifo_size;
	}
	s->rx_packet_size[s->rx_packet_size_tail] = fifo_len;
	s->rx_packet_size_tail = (s->rx_packet_size_tail + 1023) & 1023;
	s->rx_status_fifo_used++;

	status = (size + 4) << 16;
	if (buf[0] == 0xff && buf[1] == 0xff && buf[2] == 0xff &&
			buf[3] == 0xff && buf[4] == 0xff && buf[5] == 0xff) {
		status |= 0x00002000;
	} else if (buf[0] & 1) {
		status |= 0x00000400;
	}
	if (!filter) {
		status |= 0x40000000;
	}
	s->rx_status_fifo[n] = status;

	if (s->rx_status_fifo_used > (s->fifo_int & 0xff)) {
		s->int_sts |= RSFL_INT;
	}
	lan9118_update(s);

	return size;
}

static int lan9118_switch2port_xfer(struct vmm_netport *port,
				    struct vmm_mbuf *mbuf)
{
	int rc = VMM_OK;
	struct lan9118_state *s = port->priv;
	char *buf;
	int len;

	if(mbuf->m_next) {
		/* Cannot avoid a copy in case of fragmented mbuf data */
		len = min(LAN9118_MTU, mbuf->m_pktlen);
		buf = vmm_malloc(len);
		m_copydata(mbuf, 0, len, buf);
		m_freem(mbuf);
		MGETHDR(mbuf, 0, 0);
		MEXTADD(mbuf, buf, len, 0, 0);
	}
	DPRINTF("LAN9118: RX(data: 0x%8X, len: %d)\n", \
			mbuf->m_data, mbuf->m_len);
	lan9118_receive(s, mbuf);
	m_freem(mbuf);

	return rc;
}

static u32 rx_fifo_pop(struct lan9118_state *s)
{
	int n;
	u32 val;

	if (s->rxp_size == 0 && s->rxp_pad == 0) {
		s->rxp_size = s->rx_packet_size[s->rx_packet_size_head];
		s->rx_packet_size[s->rx_packet_size_head] = 0;
		if (s->rxp_size != 0) {
			s->rx_packet_size_head = (s->rx_packet_size_head + 1023) & 1023;
			s->rxp_offset = (s->rx_cfg >> 10) & 7;
			n = s->rxp_offset + s->rxp_size;
			switch (s->rx_cfg >> 30) {
				case 1:
					n = (-n) & 3;
					break;
				case 2:
					n = (-n) & 7;
					break;
				default:
					n = 0;
					break;
			}
			s->rxp_pad = n;
			DPRINTF("Pop packet size:%d offset:%d pad: %d\n",
					s->rxp_size, s->rxp_offset, s->rxp_pad);
		}
	}
	if (s->rxp_offset > 0) {
		s->rxp_offset--;
		val = 0;
	} else if (s->rxp_size > 0) {
		s->rxp_size--;
		val = s->rx_fifo[s->rx_fifo_head++];
		if (s->rx_fifo_head >= s->rx_fifo_size) {
			s->rx_fifo_head -= s->rx_fifo_size;
		}
		s->rx_fifo_used--;
	} else if (s->rxp_pad > 0) {
		s->rxp_pad--;
		val =  0;
	} else {
		DPRINTF("RX underflow\n");
		s->int_sts |= RXE_INT;
		val =  0;
	}
	lan9118_update(s);
	return val;
}

static void do_tx_packet(struct lan9118_state *s)
{
	int n;
	u32 status;
#ifdef DEBUG_LAN9118
	int i;
	char tname[30];
	struct vmm_mbuf *mbuf = txp_mbuf(s);

	DPRINTF("LAN9118: %s pkt with srcaddr[%s]", __func__, 
			ethaddr_to_str(tname, ether_srcmac(mtod(mbuf, u8 *))));
	DPRINTF(", dstaddr[%s]", ethaddr_to_str(tname, ether_dstmac(mtod(mbuf, u8 *))));
	DPRINTF(", ethertype: 0x%04X\n", ether_type(mtod(mbuf, u8 *)));
	DPRINTF("LAN9118: %s[mbuf(data: 0x%X, len: %d)] to ", __func__,
			   txp_mbuf(s)->m_data, txp_mbuf(s)->m_len);
#endif
	/* FIXME: Honor TX disable, and allow queueing of packets.  */
	if (s->phy_control & 0x4000)  {
		DPRINTF(" - phy-loopback\n");
		lan9118_receive(s, txp_mbuf(s));
	} else {
		if (s->port->nsw) {
			DPRINTF(" - switch\n");
			/* we know the data is contiguous in this mbuf,
			 * so we update mbuf->m_pktlen */
			txp_mbuf(s)->m_pktlen = txp_mbuf(s)->m_len;
			vmm_port2switch_xfer_mbuf(s->port, txp_mbuf(s));
			MGETHDR(txp_mbuf(s), 0, 0);
			MEXTMALLOC(txp_mbuf(s), LAN9118_MTU, M_WAIT);
		}
	}
	s->txp->fifo_used = 0;

	if (s->tx_status_fifo_used == 512) {
		/* Status FIFO full */
		return;
	}
	/* Add entry to status FIFO.  */
	status = s->txp->cmd_b & 0xffff0000u;
	DPRINTF("Sent packet tag:%04x len %d\n", status >> 16, txp_mbuf_len(s));
	n = (s->tx_status_fifo_head + s->tx_status_fifo_used) & 511;
	s->tx_status_fifo[n] = status;
	s->tx_status_fifo_used++;
	if (s->tx_status_fifo_used == 512) {
		s->int_sts |= TSFF_INT;
		/* TODO: Stop transmission.  */
	}
}

static u32 rx_status_fifo_pop(struct lan9118_state *s)
{
	u32 val;

	val = s->rx_status_fifo[s->rx_status_fifo_head];
	if (s->rx_status_fifo_used != 0) {
		s->rx_status_fifo_used--;
		s->rx_status_fifo_head++;
		if (s->rx_status_fifo_head >= s->rx_status_fifo_size) {
			s->rx_status_fifo_head -= s->rx_status_fifo_size;
		}
		/* ??? What value should be returned when the FIFO is empty?  */
		DPRINTF("RX status pop 0x%08x\n", val);
	}
	return val;
}

static u32 tx_status_fifo_pop(struct lan9118_state *s)
{
	u32 val;

	val = s->tx_status_fifo[s->tx_status_fifo_head];
	if (s->tx_status_fifo_used != 0) {
		s->tx_status_fifo_used--;
		s->tx_status_fifo_head = (s->tx_status_fifo_head + 1) & 511;
		/* ??? What value should be returned when the FIFO is empty?  */
	}
	return val;
}

static void tx_fifo_push(struct lan9118_state *s, u32 val)
{
	int n;

	if (s->txp->fifo_used == s->tx_fifo_size) {
		s->int_sts |= TDFO_INT;
		return;
	}
	switch (s->txp->state) {
	case TX_IDLE:
		s->txp->cmd_a = val & 0x831f37ff;
		s->txp->fifo_used++;
		s->txp->state = TX_B;
		break;
	case TX_B:
		if (s->txp->cmd_a & 0x2000) {
			/* First segment */
			s->txp->cmd_b = val;
			s->txp->fifo_used++;
			s->txp->buffer_size = s->txp->cmd_a & 0x7ff;
			s->txp->offset = (s->txp->cmd_a >> 16) & 0x1f;
			/* End alignment does not include command words.  */
			n = (s->txp->buffer_size + s->txp->offset + 3) >> 2;
			switch ((n >> 24) & 3) {
				case 1:
					n = (-n) & 3;
					break;
				case 2:
					n = (-n) & 7;
					break;
				default:
					n = 0;
			}
			s->txp->pad = n;
			txp_mbuf_len(s) = 0;
		}
		DPRINTF("Block len:%d offset:%d pad:%d cmd %08x\n",
				s->txp->buffer_size, s->txp->offset, s->txp->pad,
				s->txp->cmd_a);
		s->txp->state = TX_DATA;
		break;
	case TX_DATA:
		if (s->txp->offset >= 4) {
			s->txp->offset -= 4;
			break;
		}
		if (s->txp->buffer_size <= 0 && s->txp->pad != 0) {
			s->txp->pad--;
		} else {
			n = 4;
			while (s->txp->offset) {
				val >>= 8;
				n--;
				s->txp->offset--;
			}
			/* Documentation is somewhat unclear on the ordering of bytes
			   in FIFO words.  Empirical results show it to be little-endian.
			   */
			/* TODO: FIFO overflow checking.  */
			while (n--) {
				*(mtod(txp_mbuf(s), u8 *) + txp_mbuf_len(s)) = val & 0xff;
				txp_mbuf_len(s)++;
				val >>= 8;
				s->txp->buffer_size--;
			}
			s->txp->fifo_used++;
		}
		if (s->txp->buffer_size <= 0 && s->txp->pad == 0) {
			if (s->txp->cmd_a & 0x1000) {
				do_tx_packet(s);
			}
			if (s->txp->cmd_a & 0x80000000) {
				s->int_sts |= TX_IOC_INT;
			}
			s->txp->state = TX_IDLE;
		}
		break;
	}
}

static u32 do_phy_read(struct lan9118_state *s, int reg)
{
	u32 val;

	switch (reg) {
		case 0: /* Basic Control */
			return s->phy_control;
		case 1: /* Basic Status */
			return s->phy_status;
		case 2: /* ID1 */
			return 0x0007;
		case 3: /* ID2 */
			return 0xc0d1;
		case 4: /* Auto-neg advertisement */
			return s->phy_advertise;
		case 5: /* Auto-neg Link Partner Ability */
			return 0x0f71;
		case 6: /* Auto-neg Expansion */
			return 1;
			/* TODO 17, 18, 27, 29, 30, 31 */
		case 29: /* Interrupt source.  */
			val = s->phy_int;
			s->phy_int = 0;
			phy_update_irq(s);
			return val;
		case 30: /* Interrupt mask */
			return s->phy_int_mask;
		default:
			BADF("PHY read reg %d\n", reg);
			return 0;
	}
}

static void do_phy_write(struct lan9118_state *s, int reg, u32 val)
{
	switch (reg) {
	case 0: /* Basic Control */
		if (val & 0x8000) {
			phy_reset(s);
			break;
		}
		s->phy_control = val & 0x7980;
		/* Complete autonegotiation immediately.  */
		if (val & 0x1000) {
			s->phy_status |= 0x0020;
		}
		break;
	case 4: /* Auto-neg advertisement */
		s->phy_advertise = (val & 0x2d7f) | 0x80;
		break;
		/* TODO 17, 18, 27, 31 */
	case 30: /* Interrupt mask */
		s->phy_int_mask = val & 0xff;
		phy_update_irq(s);
		break;
	default:
		BADF("PHY write reg %d = 0x%04x\n", reg, val);
	}
}

static void do_mac_write(struct lan9118_state *s, int reg, u32 val)
{
	switch (reg) {
	case MAC_CR:
		if ((s->mac_cr & MAC_CR_RXEN) != 0 && (val & MAC_CR_RXEN) == 0) {
			s->int_sts |= RXSTOP_INT;
		}
		s->mac_cr = val & ~MAC_CR_RESERVED;
		DPRINTF("MAC_CR: %08x\n", val);
		break;
	case MAC_ADDRH:
		vmm_netport_mac(s->port)[4] = val & 0xff;
		vmm_netport_mac(s->port)[5] = (val >> 8) & 0xff;
		lan9118_mac_changed(s);
		break;
	case MAC_ADDRL:
		vmm_netport_mac(s->port)[0] = val & 0xff;
		vmm_netport_mac(s->port)[1] = (val >> 8) & 0xff;
		vmm_netport_mac(s->port)[2] = (val >> 16) & 0xff;
		vmm_netport_mac(s->port)[3] = (val >> 24) & 0xff;
		lan9118_mac_changed(s);
		break;
	case MAC_HASHH:
		s->mac_hashh = val;
		break;
	case MAC_HASHL:
		s->mac_hashl = val;
		break;
	case MAC_MII_ACC:
		s->mac_mii_acc = val & 0xffc2;
		if (val & 2) {
			DPRINTF("PHY write %d = 0x%04x\n",
					(val >> 6) & 0x1f, s->mac_mii_data);
			do_phy_write(s, (val >> 6) & 0x1f, s->mac_mii_data);
		} else {
			s->mac_mii_data = do_phy_read(s, (val >> 6) & 0x1f);
			DPRINTF("PHY read %d = 0x%04x\n",
					(val >> 6) & 0x1f, s->mac_mii_data);
		}
		break;
	case MAC_MII_DATA:
		s->mac_mii_data = val & 0xffff;
		break;
	case MAC_FLOW:
		s->mac_flow = val & 0xffff0000;
		break;
	case MAC_VLAN1:
		/* Writing to this register changes a condition for
		 * FrameTooLong bit in rx_status.  Since we do not set
		 * FrameTooLong anyway, just ignore write to this.
		 */
		break;
	default:
		vmm_printf("lan9118: Unimplemented MAC register write: %d = 0x%x\n",
				s->mac_cmd & 0xf, val);
	}
}

static u32 do_mac_read(struct lan9118_state *s, int reg)
{
	switch (reg) {
	case MAC_CR:
		return s->mac_cr;
	case MAC_ADDRH:
		return vmm_netport_mac(s->port)[4] | (vmm_netport_mac(s->port)[5] << 8);
	case MAC_ADDRL:
		return vmm_netport_mac(s->port)[0] | (vmm_netport_mac(s->port)[1] << 8)
			| (vmm_netport_mac(s->port)[2] << 16) | (vmm_netport_mac(s->port)[3] << 24);
	case MAC_HASHH:
		return s->mac_hashh;
		break;
	case MAC_HASHL:
		return s->mac_hashl;
		break;
	case MAC_MII_ACC:
		return s->mac_mii_acc;
	case MAC_MII_DATA:
		return s->mac_mii_data;
	case MAC_FLOW:
		return s->mac_flow;
	default:
		vmm_printf("lan9118: Unimplemented MAC register read: %d\n",
				s->mac_cmd & 0xf);
	}
	return 0;
}

static void lan9118_eeprom_cmd(struct lan9118_state *s, int cmd, int addr)
{
	s->e2p_cmd = (s->e2p_cmd & 0x10) | (cmd << 28) | addr;
	switch (cmd) {
	case 0:
		s->e2p_data = s->eeprom[addr];
		DPRINTF("EEPROM Read %d = 0x%02x\n", addr, s->e2p_data);
		break;
	case 1:
		s->eeprom_writable = 0;
		DPRINTF("EEPROM Write Disable\n");
		break;
	case 2: /* EWEN */
		s->eeprom_writable = 1;
		DPRINTF("EEPROM Write Enable\n");
		break;
	case 3: /* WRITE */
		if (s->eeprom_writable) {
			s->eeprom[addr] &= s->e2p_data;
			DPRINTF("EEPROM Write %d = 0x%02x\n", addr, s->e2p_data);
		} else {
			DPRINTF("EEPROM Write %d (ignored)\n", addr);
		}
		break;
	case 4: /* WRAL */
		if (s->eeprom_writable) {
			for (addr = 0; addr < 128; addr++) {
				s->eeprom[addr] &= s->e2p_data;
			}
			DPRINTF("EEPROM Write All 0x%02x\n", s->e2p_data);
		} else {
			DPRINTF("EEPROM Write All (ignored)\n");
		}
		break;
	case 5: /* ERASE */
		if (s->eeprom_writable) {
			s->eeprom[addr] = 0xff;
			DPRINTF("EEPROM Erase %d\n", addr);
		} else {
			DPRINTF("EEPROM Erase %d (ignored)\n", addr);
		}
		break;
	case 6: /* ERAL */
		if (s->eeprom_writable) {
			memset(s->eeprom, 0xff, 128);
			DPRINTF("EEPROM Erase All\n");
		} else {
			DPRINTF("EEPROM Erase All (ignored)\n");
		}
		break;
	case 7: /* RELOAD */
		lan9118_reload_eeprom(s);
		break;
	}
}

static int lan9118_reg_write(struct lan9118_state *s, physical_addr_t offset,
			     u32 src_mask, u32 src)
{
	bool do_reset = FALSE;
	offset &= 0xff;
	src = src & ~src_mask;

	DPRINTF("Write reg 0x%02x = 0x%08x\n", (int)offset, src);
	if (offset >= 0x20 && offset < 0x40) {
		vmm_spin_lock(&s->lock);
		/* TX FIFO */
		tx_fifo_push(s, src);
		vmm_spin_unlock(&s->lock);
		return VMM_OK;
	}
	vmm_spin_lock(&s->lock);
	switch (offset) {
	case CSR_IRQ_CFG:
		/* TODO: Implement interrupt deassertion intervals.  */
		src &= (IRQ_EN | IRQ_POL | IRQ_TYPE);
		s->irq_cfg = (s->irq_cfg & IRQ_INT) | src;
		break;
	case CSR_INT_STS:
		s->int_sts &= ~src;
		break;
	case CSR_INT_EN:
		s->int_en = src & ~RESERVED_INT;
		s->int_sts |= src & SW_INT;
		break;
	case CSR_FIFO_INT:
		DPRINTF("FIFO INT levels %08x\n", src);
		s->fifo_int = src;
		break;
	case CSR_RX_CFG:
		if (src & 0x8000) {
			/* RX_DUMP */
			s->rx_fifo_used = 0;
			s->rx_status_fifo_used = 0;
			s->rx_packet_size_tail = s->rx_packet_size_head;
			s->rx_packet_size[s->rx_packet_size_head] = 0;
		}
		s->rx_cfg = src & 0xcfff1ff0;
		break;
	case CSR_TX_CFG:
		if (src & 0x8000) {
			s->tx_status_fifo_used = 0;
		}
		if (src & 0x4000) {
			s->txp->state = TX_IDLE;
			s->txp->fifo_used = 0;
			s->txp->cmd_a = 0xffffffff;
		}
		s->tx_cfg = src & 6;
		break;
	case CSR_HW_CFG:
		if (src & 1) {
			/* SRST */
			do_reset = TRUE;
		} else {
			s->hw_cfg = (src & 0x003f300) | (s->hw_cfg & 0x4);
		}
		break;
	case CSR_RX_DP_CTRL:
		if (src & 0x80000000) {
			/* Skip forward to next packet.  */
			s->rxp_pad = 0;
			s->rxp_offset = 0;
			if (s->rxp_size == 0) {
				/* Pop a word to start the next packet.  */
				rx_fifo_pop(s);
				s->rxp_pad = 0;
				s->rxp_offset = 0;
			}
			s->rx_fifo_head += s->rxp_size;
			if (s->rx_fifo_head >= s->rx_fifo_size) {
				s->rx_fifo_head -= s->rx_fifo_size;
			}
		}
		break;
	case CSR_PMT_CTRL:
		if (src & 0x400) {
			phy_reset(s);
		}
		s->pmt_ctrl &= ~0x34e;
		s->pmt_ctrl |= (src & 0x34e);
		break;
	case CSR_GPIO_CFG:
		/* Probably just enabling LEDs.  */
		s->gpio_cfg = src & 0x7777071f;
		break;
	case CSR_GPT_CFG:
		if ((s->gpt_cfg ^ src) & GPT_TIMER_EN) {
			if (src & GPT_TIMER_EN) {
				s->gpt_count = (src & 0xffff);
				gpt_reload(s, 1);				
			} else {
				vmm_timer_event_stop(&s->event);
				s->gpt_count = (src & 0xffff);
			}
		}
		s->gpt_cfg = src & (GPT_TIMER_EN | 0xffff);
		break;
	case CSR_WORD_SWAP:
		/* Ignored because we're in 32-bit mode.  */
		s->word_swap = src;
		break;
	case CSR_MAC_CSR_CMD:
		s->mac_cmd = src & 0x4000000f;
		if (src & 0x80000000) {
			if (src & 0x40000000) {
				s->mac_data = do_mac_read(s, src & 0xf);
				DPRINTF("MAC read %d = 0x%08x\n", src & 0xf, s->mac_data);
			} else {
				DPRINTF("MAC write %d = 0x%08x\n", src & 0xf, s->mac_data);
				do_mac_write(s, src & 0xf, s->mac_data);
			}
		}
		break;
	case CSR_MAC_CSR_DATA:
		s->mac_data = src;
		break;
	case CSR_AFC_CFG:
		s->afc_cfg = src & 0x00ffffff;
		break;
	case CSR_E2P_CMD:
		lan9118_eeprom_cmd(s, (src >> 28) & 7, src & 0x7f);
		break;
	case CSR_E2P_DATA:
		s->e2p_data = src & 0xff;
		break;

	default:
		vmm_printf("lan9118_write: Bad reg 0x%x = %x\n", (int)offset, (int)src);
		break;
	}
	lan9118_update(s);
	vmm_spin_unlock(&s->lock);
	if (do_reset) {
		lan9118_state_reset(s);
	}
	return VMM_OK;
}

static int lan9118_reg_read(struct lan9118_state *s, 
			    physical_addr_t offset, 
			    u32 *dst)
{
	u64 cval;
	if (offset < 0x20) {
		/* RX FIFO */
		*dst = rx_fifo_pop(s);
		return VMM_OK;
	}
	vmm_spin_lock(&s->lock);
	switch (offset) {
	case 0x40:
		*dst = rx_status_fifo_pop(s);
		break;
	case 0x44:
		*dst = s->rx_status_fifo[s->tx_status_fifo_head];
		break;
	case 0x48:
		*dst = tx_status_fifo_pop(s);
		break;
	case 0x4c:
		*dst = s->tx_status_fifo[s->tx_status_fifo_head];
		break;
	case CSR_ID_REV:
		*dst = 0x01180001;
		break;
	case CSR_IRQ_CFG:
		*dst = s->irq_cfg;
		break;
	case CSR_INT_STS:
		*dst = s->int_sts;
		break;
	case CSR_INT_EN:
		*dst = s->int_en;
		break;
	case CSR_BYTE_TEST:
		*dst = 0x87654321;
		break;
	case CSR_FIFO_INT:
		*dst = s->fifo_int;
		break;
	case CSR_RX_CFG:
		*dst = s->rx_cfg;
		break;
	case CSR_TX_CFG:
		*dst = s->tx_cfg;
		break;
	case CSR_HW_CFG:
		*dst = s->hw_cfg;
		break;
	case CSR_RX_DP_CTRL:
		*dst = 0;
		break;
	case CSR_RX_FIFO_INF:
		*dst = (s->rx_status_fifo_used << 16) | (s->rx_fifo_used << 2);
		break;
	case CSR_TX_FIFO_INF:
		*dst = (s->tx_status_fifo_used << 16)
			| (s->tx_fifo_size - s->txp->fifo_used);
		break;
	case CSR_PMT_CTRL:
		*dst = s->pmt_ctrl;
		break;
	case CSR_GPIO_CFG:
		*dst = s->gpio_cfg;
		break;
	case CSR_GPT_CFG:
		*dst = s->gpt_cfg;
		break;
	case CSR_GPT_CNT:
		*dst = gpt_counter_value(s);
		break;
	case CSR_WORD_SWAP:
		*dst = s->word_swap;
		break;
	case CSR_FREE_RUN:
		cval = udiv64((vmm_timer_timestamp() - s->free_timer_start_tstamp), 
			      (u64) 40);
		*dst = umod64(cval, 0x100000000ULL);
		break;
	case CSR_RX_DROP:
		/* TODO: Implement dropped frames counter.  */
		*dst = 0;
		break;
	case CSR_MAC_CSR_CMD:
		*dst = s->mac_cmd;
		break;
	case CSR_MAC_CSR_DATA:
		*dst = s->mac_data;
		break;
	case CSR_AFC_CFG:
		*dst = s->afc_cfg;
		break;
	case CSR_E2P_CMD:
		*dst = s->e2p_cmd;
		break;
	case CSR_E2P_DATA:
		*dst = s->e2p_data;
		break;
	default:
		vmm_printf("lan9118_read: Bad reg 0x%x\n", (int)offset);
		return VMM_EFAIL;
	}
	vmm_spin_unlock(&s->lock);
	return VMM_OK;
}

static int lan9118_emulator_read8(struct vmm_emudev *edev,
				  physical_addr_t offset, 
				  u8 *dst)
{
	int rc;
	u32 regval = 0x0;

	rc = lan9118_reg_read(edev->priv, offset, &regval);
	if (!rc) {
		*dst = regval & 0xFF;
	}

	return rc;
}

static int lan9118_emulator_read16(struct vmm_emudev *edev,
				   physical_addr_t offset, 
				   u16 *dst)
{
	int rc;
	u32 regval = 0x0;

	rc = lan9118_reg_read(edev->priv, offset, &regval);
	if (!rc) {
		*dst = regval & 0xFFFF;
	}

	return rc;
}

static int lan9118_emulator_read32(struct vmm_emudev *edev,
				   physical_addr_t offset, 
				   u32 *dst)
{
	return lan9118_reg_read(edev->priv, offset, dst);
}

static int lan9118_emulator_write8(struct vmm_emudev *edev,
				   physical_addr_t offset, 
				   u8 src)
{
	return lan9118_reg_write(edev->priv, offset, 0xFFFFFF00, src);
}

static int lan9118_emulator_write16(struct vmm_emudev *edev,
				    physical_addr_t offset, 
				    u16 src)
{
	return lan9118_reg_write(edev->priv, offset, 0xFFFF0000, src);
}

static int lan9118_emulator_write32(struct vmm_emudev *edev,
				    physical_addr_t offset, 
				    u32 src)
{
	return lan9118_reg_write(edev->priv, offset, 0x00000000, src);
}

static int lan9118_emulator_probe(struct vmm_guest *guest,
				  struct vmm_emudev *edev,
				  const struct vmm_devtree_nodeid *eid)
{
	int i, rc = VMM_OK;
	char tname[64];
	const char *attr;
	struct vmm_netswitch *nsw;
	struct lan9118_state *s = NULL;

	s = vmm_zalloc(sizeof(struct lan9118_state));
	if (!s) {
		vmm_printf("%s: state alloc failed\n", __func__);
		rc = VMM_ENOMEM;
		goto lan9118_emulator_probe_done;
	}

	rc = vmm_devtree_irq_get(edev->node, &s->irq, 0);
	if (rc) {
		vmm_printf("%s: no interrupts found\n", __func__);
		goto lan9118_emulator_probe_freestate_failed;
	}

	s->guest = guest;
	INIT_SPIN_LOCK(&s->lock);
	edev->priv = s;

	vmm_sprintf(tname, "%s/%s", guest->name, edev->node->name);
	s->port = vmm_netport_alloc(tname, VMM_NETPORT_DEF_QUEUE_SIZE);
	if (!s->port) {
		vmm_printf("%s: netport alloc failed\n", __func__);
		rc = VMM_ENOMEM;
		goto lan9118_emulator_probe_freestate_failed;
	}

	s->port->mtu = LAN9118_MTU;
	s->port->link_changed = lan9118_set_link;
	s->port->can_receive = lan9118_can_receive;
	s->port->switch2port_xfer = lan9118_switch2port_xfer;
	s->port->priv = s;

	rc = vmm_netport_register(s->port);
	if (rc) {
		vmm_printf("%s: netport register failed\n", __func__);
		goto lan9118_emulator_probe_freeport_failed;
	}

	if (vmm_devtree_read_string(edev->node,
				    "switch", &attr) == VMM_OK) {
		nsw = vmm_netswitch_find((char *)attr);
		if (!nsw) {
			vmm_panic("%s: Cannot find netswitch \"%s\"\n", 
				   __func__, (char *)attr);
		}
		vmm_netswitch_port_add(nsw, s->port);
	}

	s->eeprom[0] = 0xa5;
	for (i = 0; i < 6; i++) {
		s->eeprom[i + 1] = vmm_netport_mac(s->port)[i];
	}

	s->pmt_ctrl = 1;
	s->txp = &s->tx_packet;

	INIT_TIMER_EVENT(&s->event, &gpt_event, s);

	MGETHDR(txp_mbuf(s), 0, 0);
	MEXTMALLOC(txp_mbuf(s), LAN9118_MTU, M_WAIT);

	goto lan9118_emulator_probe_done;

lan9118_emulator_probe_freeport_failed:
	vmm_netport_free(s->port);
lan9118_emulator_probe_freestate_failed:
	vmm_free(s);
lan9118_emulator_probe_done:
	return rc;
}

static int lan9118_emulator_remove(struct vmm_emudev *edev)
{
	struct lan9118_state *s = edev->priv;

	vmm_netport_unregister(s->port);
	vmm_netport_free(s->port);
	if (txp_mbuf(s)) {
		m_freem(txp_mbuf(s));
		txp_mbuf(s) = NULL;
	}
	vmm_free(s);

	edev->priv = NULL;

	return VMM_OK;
}

static struct vmm_devtree_nodeid lan9118_emuid_table[] = {
	{ 
		.type = "nic", 
		.compatible = "smsc,lan9118", 
		.data = NULL,
	},
	{ /* end of list */ },
};

static struct vmm_emulator lan9118_emulator = {
	.name = "lan9118",
	.match_table = lan9118_emuid_table,
	.endian = VMM_DEVEMU_LITTLE_ENDIAN,
	.probe = lan9118_emulator_probe,
	.read8 = lan9118_emulator_read8,
	.write8 = lan9118_emulator_write8,
	.read16 = lan9118_emulator_read16,
	.write16 = lan9118_emulator_write16,
	.read32 = lan9118_emulator_read32,
	.write32 = lan9118_emulator_write32,
	.reset = lan9118_emulator_reset,
	.remove = lan9118_emulator_remove,
};

static int __init lan9118_emulator_init(void)
{
	return vmm_devemu_register_emulator(&lan9118_emulator);
}

static void __exit lan9118_emulator_exit(void)
{
	vmm_devemu_unregister_emulator(&lan9118_emulator);
}

VMM_DECLARE_MODULE(MODULE_DESC, 
			MODULE_AUTHOR, 
			MODULE_LICENSE, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);

