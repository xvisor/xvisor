/**
 * Copyright (c) 2012 Pranavkumar Sawargaonkar.
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
 * You should have received a copy of the GNU General Public
 * License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * @file smc91c111.c
 * @author Pranavkumar Sawargaonkar (pranav.sawargaonkar@gmail.com)
 * @brief SMSC 91C111 Ethernet Interface Emulator.
 *
 * The source has been largely adapted from QEMU hw/smc91c111.c
 *
 * SMSC 91C111 Ethernet interface emulation
 *
 * Copyright (c) 2005 CodeSourcery, LLC.
 * Written by Paul Brook
 *
 * This code is licensed under the GPL
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_timer.h>
#include <vmm_modules.h>
#include <vmm_devtree.h>
#include <vmm_host_io.h>
#include <vmm_devemu.h>
#include <vmm_manager.h>
#include <vmm_scheduler.h>
#include <vmm_stdio.h>
#include <net/vmm_protocol.h>
#include <net/vmm_mbuf.h>
#include <net/vmm_net.h>
#include <net/vmm_netswitch.h>
#include <net/vmm_netport.h>
#include <libs/stringlib.h>
#include <libs/mathlib.h>

#define MODULE_DESC                     "SMSC91C111 Emulator"
#define MODULE_AUTHOR                   "Pranavkumar Sawargaonkar"
#define MODULE_LICENSE                  "GPL"
#define MODULE_IPRIORITY                (VMM_NET_CLASS_IPRIORITY+1)
#define MODULE_INIT                     smc91c111_emulator_init
#define MODULE_EXIT                     smc91c111_emulator_exit

#define SMC91C111_MTU     2048

#ifdef DEBUG_LAN9118
#define DPRINTF(fmt, ...) \
	do { vmm_printf(fmt , ## __VA_ARGS__); } while (0)
#else
#define DPRINTF(fmt, ...) do {} while(0)
#endif

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

/* Number of 2k memory pages available.  */
#define NUM_PACKETS 4

struct smc91c111_state_info {
	struct vmm_netport *port;
	vmm_spinlock_t lock;
	struct vmm_guest *guest;
	u32 irq;
	bool link_down;

	u16 tcr;
	u16 rcr;
	u16 cr;
	u16 ctr;
	u16 gpr;
	u16 ptr;
	u16 ercv;
	int bank;
	int packet_num;
	int tx_alloc;
	/* Bitmask of allocated packets.  */
	int allocated;
	int tx_fifo_len;
	int tx_fifo[NUM_PACKETS];
	int rx_fifo_len;
	int rx_fifo[NUM_PACKETS];
	int tx_fifo_done_len;
	int tx_fifo_done[NUM_PACKETS];
	/* Packet buffer memory.  */
	u8 data[NUM_PACKETS][2048];
	u8 int_level;
	u8 int_mask;
	u8 mac[6];
};

typedef struct smc91c111_state_info smc91c111_state;

#define RCR_SOFT_RST  0x8000
#define RCR_STRIP_CRC 0x0200
#define RCR_RXEN      0x0100

#define TCR_EPH_LOOP  0x2000
#define TCR_NOCRC     0x0100
#define TCR_PAD_EN    0x0080
#define TCR_FORCOL    0x0004
#define TCR_LOOP      0x0002
#define TCR_TXEN      0x0001

#define INT_MD        0x80
#define INT_ERCV      0x40
#define INT_EPH       0x20
#define INT_RX_OVRN   0x10
#define INT_ALLOC     0x08
#define INT_TX_EMPTY  0x04
#define INT_TX        0x02
#define INT_RCV       0x01

#define CTR_AUTO_RELEASE  0x0800
#define CTR_RELOAD        0x0002
#define CTR_STORE         0x0001

#define RS_ALGNERR      0x8000
#define RS_BRODCAST     0x4000
#define RS_BADCRC       0x2000
#define RS_ODDFRAME     0x1000
#define RS_TOOLONG      0x0800
#define RS_TOOSHORT     0x0400
#define RS_MULTICAST    0x0001

/* Update interrupt status.  */
static void smc91c111_update(smc91c111_state *s)
{
	int level;

	if (s->tx_fifo_len == 0)
		s->int_level |= INT_TX_EMPTY;
	if (s->tx_fifo_done_len != 0)
		s->int_level |= INT_TX;
	level = (s->int_level & s->int_mask) != 0;

	vmm_devemu_emulate_irq(s->guest, s->irq, level);

}

/* Try to allocate a packet.  Returns 0x80 on failure.  */
static int smc91c111_allocate_packet(smc91c111_state *s)
{
	int i;
	if (s->allocated == (1 << NUM_PACKETS) - 1) {
		return 0x80;
	}

	for (i = 0; i < NUM_PACKETS; i++) {
		if ((s->allocated & (1 << i)) == 0)
			break;
	}
	s->allocated |= 1 << i;
	return i;
}


/* Process a pending TX allocate.  */
static void smc91c111_tx_alloc(smc91c111_state *s)
{
	s->tx_alloc = smc91c111_allocate_packet(s);
	if (s->tx_alloc == 0x80)
		return;
	s->int_level |= INT_ALLOC;
	smc91c111_update(s);
}

/* Remove and item from the RX FIFO.  */
static void smc91c111_pop_rx_fifo(smc91c111_state *s)
{
	int i;

	s->rx_fifo_len--;
	if (s->rx_fifo_len) {
		for (i = 0; i < s->rx_fifo_len; i++)
			s->rx_fifo[i] = s->rx_fifo[i + 1];
		s->int_level |= INT_RCV;
	} else {
		s->int_level &= ~INT_RCV;
	}
	smc91c111_update(s);
}

/* Remove an item from the TX completion FIFO.  */
static void smc91c111_pop_tx_fifo_done(smc91c111_state *s)
{
	int i;

	if (s->tx_fifo_done_len == 0)
		return;
	s->tx_fifo_done_len--;
	for (i = 0; i < s->tx_fifo_done_len; i++)
		s->tx_fifo_done[i] = s->tx_fifo_done[i + 1];
}

/* Release the memory allocated to a packet.  */
static void smc91c111_release_packet(smc91c111_state *s, int packet)
{
	s->allocated &= ~(1 << packet);
	if (s->tx_alloc == 0x80)
		smc91c111_tx_alloc(s);
}

/* Flush the TX FIFO.  */
static void smc91c111_do_tx(smc91c111_state *s)
{
	int i;
	int len;
	int control;
	int packetnum;
	u8 *p;
	struct vmm_mbuf *mbuf;
	u8 *buf;

	if ((s->tcr & TCR_TXEN) == 0)
		return;
	if (s->tx_fifo_len == 0)
		return;
	for (i = 0; i < s->tx_fifo_len; i++) {
		packetnum = s->tx_fifo[i];
		p = &s->data[packetnum][0];
		/* Set status word.  */
		*(p++) = 0x01;
		*(p++) = 0x40;
		len = *(p++);
		len |= ((int)*(p++)) << 8;
		len -= 6;
		control = p[len + 1];
		if (control & 0x20)
			len++;
		/* ??? This overwrites the data following the buffer.
		   Don't know what real hardware does.  */
		if (len < 64 && (s->tcr & TCR_PAD_EN)) {
			memset(p + len, 0, 64 - len);
			len = 64;
		}
#if 0
		{
			int add_crc;

			/* The card is supposed to append the CRC to the frame.
			   However none of the other network traffic has the CRC
			   appended.  Suspect this is low level ethernet detail we
			   don't need to worry about.  */
			add_crc = (control & 0x10) || (s->tcr & TCR_NOCRC) == 0;
			if (add_crc) {
				u32 crc;

				crc = crc32(~0, p, len);
				memcpy(p + len, &crc, 4);
				len += 4;
			}
		}
#endif
		if (s->ctr & CTR_AUTO_RELEASE)
			/* Race?  */
			smc91c111_release_packet(s, packetnum);
		else if (s->tx_fifo_done_len < NUM_PACKETS)
			s->tx_fifo_done[s->tx_fifo_done_len++] = packetnum;

		MGETHDR(mbuf, 0, 0);
		MEXTMALLOC(mbuf, SMC91C111_MTU, M_WAIT);
		mbuf->m_len = len;
		buf = mtod(mbuf, u8 *);
		memcpy(buf, p, len);

		vmm_port2switch_xfer_mbuf(s->port, mbuf);


	}
	s->tx_fifo_len = 0;
	smc91c111_update(s);
}

/* Add a packet to the TX FIFO.  */
static void smc91c111_queue_tx(smc91c111_state *s, int packet)
{
	if (s->tx_fifo_len == NUM_PACKETS)
		return;
	s->tx_fifo[s->tx_fifo_len++] = packet;
	smc91c111_do_tx(s);
}

static int smc91c111_reset(smc91c111_state *s)
{
	s->bank = 0;
	s->tx_fifo_len = 0;
	s->tx_fifo_done_len = 0;
	s->rx_fifo_len = 0;
	s->allocated = 0;
	s->packet_num = 0;
	s->tx_alloc = 0;
	s->tcr = 0;
	s->rcr = 0;
	s->cr = 0xa0b1;
	s->ctr = 0x1210;
	s->ptr = 0;
	s->ercv = 0x1f;
	s->int_level = INT_TX_EMPTY;
	s->int_mask = 0;
	smc91c111_update(s);

	return VMM_OK;
}

static int smc91c111_emulator_reset(struct vmm_emudev *edev)
{
	smc91c111_state *s = edev->priv;

	return smc91c111_reset(s);
}

#define SET_LOW(name, val) s->name = (s->name & 0xff00) | val
#define SET_HIGH(name, val) s->name = (s->name & 0xff) | (val << 8)

static void smc91c111_writeb(void *opaque, physical_addr_t offset,
		u32 value)
{
	smc91c111_state *s = (smc91c111_state *)opaque;

	offset = offset & 0xf;
	if (offset == 14) {
		s->bank = value;
		return;
	}
	if (offset == 15)
		return;
	switch (s->bank) {
		case 0:
			switch (offset) {
				case 0: /* TCR */
					SET_LOW(tcr, value);
					return;
				case 1:
					SET_HIGH(tcr, value);
					return;
				case 4: /* RCR */
					SET_LOW(rcr, value);
					return;
				case 5:
					SET_HIGH(rcr, value);
					if (s->rcr & RCR_SOFT_RST)
						smc91c111_reset(s);
					return;
				case 10: case 11: /* RPCR */
					/* Ignored */
					return;
				case 12: case 13: /* Reserved */
					return;
			}
			break;

		case 1:
			switch (offset) {
				case 0: /* CONFIG */
					SET_LOW(cr, value);
					return;
				case 1:
					SET_HIGH(cr,value);
					return;
				case 2: case 3: /* BASE */
				case 4: case 5: case 6: case 7: case 8: case 9: /* IA */
					/* Not implemented.  */
					return;
				case 10: /* Genral Purpose */
					SET_LOW(gpr, value);
					return;
				case 11:
					SET_HIGH(gpr, value);
					return;
				case 12: /* Control */
					if (value & 1)
						vmm_printf("smc91c111:EEPROM store not implemented\n");
					if (value & 2)
						vmm_printf("smc91c111:EEPROM reload not implemented\n");
					value &= ~3;
					SET_LOW(ctr, value);
					return;
				case 13:
					SET_HIGH(ctr, value);
					return;
			}
			break;

		case 2:
			switch (offset) {
				case 0: /* MMU Command */
					switch (value >> 5) {
						case 0: /* no-op */
							break;
						case 1: /* Allocate for TX.  */
							s->tx_alloc = 0x80;
							s->int_level &= ~INT_ALLOC;
							smc91c111_update(s);
							smc91c111_tx_alloc(s);
							break;
						case 2: /* Reset MMU.  */
							s->allocated = 0;
							s->tx_fifo_len = 0;
							s->tx_fifo_done_len = 0;
							s->rx_fifo_len = 0;
							s->tx_alloc = 0;
							break;
						case 3: /* Remove from RX FIFO.  */
							smc91c111_pop_rx_fifo(s);
							break;
						case 4: /* Remove from RX FIFO and release.  */
							if (s->rx_fifo_len > 0) {
								smc91c111_release_packet(s, s->rx_fifo[0]);
							}
							smc91c111_pop_rx_fifo(s);
							break;
						case 5: /* Release.  */
							smc91c111_release_packet(s, s->packet_num);
							break;
						case 6: /* Add to TX FIFO.  */
							smc91c111_queue_tx(s, s->packet_num);
							break;
						case 7: /* Reset TX FIFO.  */
							s->tx_fifo_len = 0;
							s->tx_fifo_done_len = 0;
							break;
					}
					return;
				case 1:
					/* Ignore.  */
					return;
				case 2: /* Packet Number Register */
					s->packet_num = value;
					return;
				case 3: case 4: case 5:
					/* Should be readonly, but linux writes to them anyway. Ignore.  */
					return;
				case 6: /* Pointer */
					SET_LOW(ptr, value);
					return;
				case 7:
					SET_HIGH(ptr, value);
					return;
				case 8: case 9: case 10: case 11: /* Data */
					{
						int p;
						int n;

						if (s->ptr & 0x8000)
							n = s->rx_fifo[0];
						else
							n = s->packet_num;
						p = s->ptr & 0x07ff;
						if (s->ptr & 0x4000) {
							s->ptr = (s->ptr & 0xf800) | ((s->ptr + 1) & 0x7ff);
						} else {
							p += (offset & 3);
						}
						s->data[n][p] = value;
					}
					return;
				case 12: /* Interrupt ACK.  */
					s->int_level &= ~(value & 0xd6);
					if (value & INT_TX)
						smc91c111_pop_tx_fifo_done(s);
					smc91c111_update(s);
					return;
				case 13: /* Interrupt mask.  */
					s->int_mask = value;
					smc91c111_update(s);
					return;
			}
			break;

		case 3:
			switch (offset) {
				case 0: case 1: case 2: case 3: case 4: case 5: case 6: case 7:
					/* Multicast table.  */
					/* Not implemented.  */
					return;
				case 8: case 9: /* Management Interface.  */
					/* Not implemented.  */
					return;
				case 12: /* Early receive.  */
					s->ercv = value & 0x1f;
				case 13:
					/* Ignore.  */
					return;
			}
			break;
	}
	vmm_printf("smc91c111_write: Bad reg %d:%x\n", s->bank, (int)offset);
}

static u32 smc91c111_readb(void *opaque, physical_addr_t offset)
{
	smc91c111_state *s = (smc91c111_state *)opaque;

	offset = offset & 0xf;
	if (offset == 14) {
		return s->bank;
	}
	if (offset == 15)
		return 0x33;
	switch (s->bank) {
		case 0:
			switch (offset) {
				case 0: /* TCR */
					return s->tcr & 0xff;
				case 1:
					return s->tcr >> 8;
				case 2: /* EPH Status */
					return 0;
				case 3:
					return 0x40;
				case 4: /* RCR */
					return s->rcr & 0xff;
				case 5:
					return s->rcr >> 8;
				case 6: /* Counter */
				case 7:
					/* Not implemented.  */
					return 0;
				case 8: /* Memory size.  */
					return NUM_PACKETS;
				case 9: /* Free memory available.  */
					{
						int i;
						int n;
						n = 0;
						for (i = 0; i < NUM_PACKETS; i++) {
							if (s->allocated & (1 << i))
								n++;
						}
						return n;
					}
				case 10: case 11: /* RPCR */
					/* Not implemented.  */
					return 0;
				case 12: case 13: /* Reserved */
					return 0;
			}
			break;

		case 1:
			switch (offset) {
				case 0: /* CONFIG */
					return s->cr & 0xff;
				case 1:
					return s->cr >> 8;
				case 2: case 3: /* BASE */
					/* Not implemented.  */
					return 0;
				case 4: case 5: case 6: case 7: case 8: case 9: /* IA */
					//TBD
					//return s->conf.macaddr.a[offset - 4];
					return s->mac[offset -4];
					vmm_printf("Mac address read offset %d\n", offset);
					return 0;
				case 10: /* General Purpose */
					return s->gpr & 0xff;
				case 11:
					return s->gpr >> 8;
				case 12: /* Control */
					return s->ctr & 0xff;
				case 13:
					return s->ctr >> 8;
			}
			break;

		case 2:
			switch (offset) {
				case 0: case 1: /* MMUCR Busy bit.  */
					return 0;
				case 2: /* Packet Number.  */
					return s->packet_num;
				case 3: /* Allocation Result.  */
					return s->tx_alloc;
				case 4: /* TX FIFO */
					if (s->tx_fifo_done_len == 0)
						return 0x80;
					else
						return s->tx_fifo_done[0];
				case 5: /* RX FIFO */
					if (s->rx_fifo_len == 0)
						return 0x80;
					else
						return s->rx_fifo[0];
				case 6: /* Pointer */
					return s->ptr & 0xff;
				case 7:
					return (s->ptr >> 8) & 0xf7;
				case 8: case 9: case 10: case 11: /* Data */
					{
						int p;
						int n;

						if (s->ptr & 0x8000)
							n = s->rx_fifo[0];
						else
							n = s->packet_num;
						p = s->ptr & 0x07ff;
						if (s->ptr & 0x4000) {
							s->ptr = (s->ptr & 0xf800) | ((s->ptr + 1) & 0x07ff);
						} else {
							p += (offset & 3);
						}
						return s->data[n][p];
					}
				case 12: /* Interrupt status.  */
					return s->int_level;
				case 13: /* Interrupt mask.  */
					return s->int_mask;
			}
			break;

		case 3:
			switch (offset) {
				case 0: case 1: case 2: case 3: case 4: case 5: case 6: case 7:
					/* Multicast table.  */
					/* Not implemented.  */
					return 0;
				case 8: /* Management Interface.  */
					/* Not implemented.  */
					return 0x30;
				case 9:
					return 0x33;
				case 10: /* Revision.  */
					return 0x91;
				case 11:
					return 0x33;
				case 12:
					return s->ercv;
				case 13:
					return 0;
			}
			break;
	}
	vmm_printf("smc91c111_read: Bad reg %d:%x\n", s->bank, (int)offset);
	return 0;
}

static void smc91c111_writew(void *opaque, physical_addr_t offset,
		u32 value)
{
	smc91c111_writeb(opaque, offset, value & 0xff);
	smc91c111_writeb(opaque, offset + 1, value >> 8);
}

static void smc91c111_writel(void *opaque, physical_addr_t offset,
		u32 value)
{
	/* 32-bit writes to offset 0xc only actually write to the bank select
	   register (offset 0xe)  */
	if (offset != 0xc)
		smc91c111_writew(opaque, offset, value & 0xffff);
	smc91c111_writew(opaque, offset + 2, value >> 16);
}

static int smc91c111_emulator_write(struct vmm_emudev *edev,
		physical_addr_t offset,
		void *src, u32 src_len)
{
	struct smc91c111_state *opaque = edev->priv;
	u32 regval = 0;

	switch(src_len) {
		case 1:
			regval = *(u8 *) src;
			smc91c111_writeb(opaque, offset, regval);
			break;
		case 2:
			regval = *(u16 *) src;
			smc91c111_writew(opaque, offset, regval);
			break;
		case 4:
			regval = *(u32 *) src;
			smc91c111_writel(opaque, offset, regval);
			break;
		default:
			return VMM_EFAIL;

	}

	return VMM_OK;

}

static u32 smc91c111_readw(void *opaque, physical_addr_t offset)
{
	u32 val;
	val = smc91c111_readb(opaque, offset);
	val |= smc91c111_readb(opaque, offset + 1) << 8;
	return val;
}

static u32 smc91c111_readl(void *opaque, physical_addr_t offset)
{
	u32 val;
	val = smc91c111_readw(opaque, offset);
	val |= smc91c111_readw(opaque, offset + 2) << 16;
	return val;
}

static int smc91c111_emulator_read(struct vmm_emudev *edev,
		physical_addr_t offset,
		void *dst, u32 dst_len)
{
	struct smc91c111_state *opaque = edev->priv;


	*(u32 *) dst = 0;

	switch (dst_len) {
		case 1:
			*(u8 *) dst = smc91c111_readb(opaque, offset);
			break;
		case 2:
			*(u16 *) dst = vmm_cpu_to_le16(smc91c111_readw(opaque, offset));
			break;
		case 4:
			*(u32 *) dst = vmm_cpu_to_le32(
					smc91c111_readl(opaque, offset));
			break;
		default:
			return VMM_EFAIL;
	}

	//vmm_printf("%s: **************Returning 0x%x len %d offset 0x%x\n", __func__, *(u32 *) dst, dst_len, offset);

	return VMM_OK;
}


static int smc91c111_can_receive(struct vmm_netport *port)
{
	smc91c111_state *s = port->priv;

	if ((s->rcr & RCR_RXEN) == 0 || (s->rcr & RCR_SOFT_RST))
		return 1;
	if (s->allocated == (1 << NUM_PACKETS) - 1)
		return 0;
	return 1;
}

//static ssize_t smc91c111_receive(VLANClientState *nc, const u8 *buf, size_t size)
static int smc91c111_receive(smc91c111_state *s, struct vmm_mbuf *mbuf)
{
	int status;
	int packetsize;
	u32 crc;
	int packetnum;
	u8 *p;

	/* FIXME: Handle chained/fragmented mbufs */
	u8 *buf = mtod(mbuf, u8 *);
	u32 size = mbuf->m_len;

	if ((s->rcr & RCR_RXEN) == 0 || (s->rcr & RCR_SOFT_RST))
		return -1;
	/* Short packets are padded with zeros.  Receiving a packet
	   < 64 bytes long is considered an error condition.  */
	if (size < 64)
		packetsize = 64;
	else
		packetsize = (size & ~1);
	packetsize += 6;
	crc = (s->rcr & RCR_STRIP_CRC) == 0;
	if (crc)
		packetsize += 4;
	/* TODO: Flag overrun and receive errors.  */
	if (packetsize > 2048)
		return -1;
	packetnum = smc91c111_allocate_packet(s);
	if (packetnum == 0x80)
		return -1;
	s->rx_fifo[s->rx_fifo_len++] = packetnum;

	p = &s->data[packetnum][0];
	/* ??? Multicast packets?  */
	status = 0;
	if (size > 1518)
		status |= RS_TOOLONG;
	if (size & 1)
		status |= RS_ODDFRAME;
	*(p++) = status & 0xff;
	*(p++) = status >> 8;
	*(p++) = packetsize & 0xff;
	*(p++) = packetsize >> 8;
	memcpy(p, buf, size & ~1);
	p += (size & ~1);
	/* Pad short packets.  */
	if (size < 64) {
		int pad;

		if (size & 1)
			*(p++) = buf[size - 1];
		pad = 64 - size;
		memset(p, 0, pad);
		p += pad;
		size = 64;
	}
	/* It's not clear if the CRC should go before or after the last byte in
	   odd sized packets.  Linux disables the CRC, so that's no help.
	   The pictures in the documentation show the CRC aligned on a 16-bit
	   boundary before the last odd byte, so that's what we do.  */
	if (crc) {
		crc = crc32(~0, buf, size);
		*(p++) = crc & 0xff; crc >>= 8;
		*(p++) = crc & 0xff; crc >>= 8;
		*(p++) = crc & 0xff; crc >>= 8;
		*(p++) = crc & 0xff;
	}
	if (size & 1) {
		*(p++) = buf[size - 1];
		*p = 0x60;
	} else {
		*(p++) = 0;
		*p = 0x40;
	}
	/* TODO: Raise early RX interrupt?  */
	s->int_level |= INT_RCV;
	smc91c111_update(s);

	return size;
}

static void smc91c111_set_link(struct vmm_netport *port)
{
	smc91c111_state *s = (smc91c111_state *)port->priv;

	s->link_down = !(port->flags & VMM_NETPORT_LINK_UP);
}

static int smc91c111_switch2port_xfer(struct vmm_netport *port,
		struct vmm_mbuf *mbuf)
{
	int rc = VMM_OK;
	smc91c111_state *s = port->priv;
	char *buf;
	int len;

	if(mbuf->m_next) {
		/* Cannot avoid a copy in case of fragmented mbuf data */
		len = min(SMC91C111_MTU, mbuf->m_pktlen);
		buf = vmm_malloc(len);
		m_copydata(mbuf, 0, len, buf);
		m_freem(mbuf);
		MGETHDR(mbuf, 0, 0);
		MEXTADD(mbuf, buf, len, 0, 0);
	}
	DPRINTF("SMC91C111: RX(data: 0x%8X, len: %d)\n", \
			mbuf->m_data, mbuf->m_len);
	smc91c111_receive(s, mbuf);
	m_freem(mbuf);

	return rc;
}

static int smc91c111_emulator_probe(struct vmm_guest *guest,
		struct vmm_emudev *edev,
		const struct vmm_emuid *eid)
{
	smc91c111_state *s = NULL;
	int rc = VMM_OK;
	char tname[64];
	void *attr;
	struct vmm_netswitch *nsw;
	int i = 0;

	s = vmm_malloc(sizeof(smc91c111_state));
	if(!s) {
		vmm_printf("smc91c111 state alloc failed\n");
		rc = VMM_EFAIL;
		goto smc91c111_emulator_probe_done;
	}
	memset(s, 0, sizeof(smc91c111_state));

	attr = vmm_devtree_attrval(edev->node, "irq");
	if (attr) {
		s->irq = *((u32 *)attr);
	} else {
		vmm_printf("smc91c111: no irq node found\n");
		rc = VMM_EFAIL;
		goto smc91c111_emulator_probe_failed;
	}

	s->guest = guest;
	INIT_SPIN_LOCK(&s->lock);
	edev->priv = s;

	vmm_sprintf(tname, "%s%s%s",
			guest->node->name,
			VMM_DEVTREE_PATH_SEPARATOR_STRING,
			edev->node->name);
	s->port = vmm_netport_alloc(tname, VMM_NETPORT_DEF_QUEUE_SIZE);
	if(!s->port) {
		vmm_printf("smc91c111state->netport alloc failed\n");
		rc = VMM_EFAIL;
		goto smc91c111_emulator_probe_failed;
	}

	s->port->mtu = SMC91C111_MTU;
	s->port->link_changed = smc91c111_set_link;
	s->port->can_receive = smc91c111_can_receive;
	s->port->switch2port_xfer = smc91c111_switch2port_xfer;
	s->port->priv = s;
	vmm_netport_register(s->port);

	attr = vmm_devtree_attrval(edev->node, "switch");
	if (attr) {
		nsw = vmm_netswitch_find((char *)attr);
		if(!nsw) {
			vmm_panic("smc91c111: Cannot find netswitch \"%s\"\n", (char *)attr);
		}
		vmm_netswitch_port_add(nsw, s->port);
	}

	for (i = 0; i < 6; i++) {
		s->mac[i] = vmm_netport_mac(s->port)[i];
	}

	goto smc91c111_emulator_probe_done;

smc91c111_emulator_probe_failed:
	vmm_printf("smc91c111-probe failed\n");
	vmm_free(s);
smc91c111_emulator_probe_done:

	return rc;
}

static int smc91c111_emulator_remove(struct vmm_emudev *edev)
{
	smc91c111_state *s = edev->priv;
	int rc = VMM_OK;

	vmm_netswitch_port_remove(s->port);
	vmm_netport_unregister(s->port);
	edev->priv = NULL;
	return rc;
}

static struct vmm_emuid smc91c111_emuid_table[] = {
	{
		.type = "nic",
		.compatible = "smsc,smc91c111",
		.data = NULL,
	},
	{ /* end of list */ },
};

static struct vmm_emulator smc91c111_emulator = {
	.name = "smc91c111",
	.match_table = smc91c111_emuid_table,
	.probe = smc91c111_emulator_probe,
	.read = smc91c111_emulator_read,
	.write = smc91c111_emulator_write,
	.reset = smc91c111_emulator_reset,
	.remove = smc91c111_emulator_remove,
};

static int __init smc91c111_emulator_init(void)
{
	return vmm_devemu_register_emulator(&smc91c111_emulator);
}

static void __exit smc91c111_emulator_exit(void)
{
	vmm_devemu_unregister_emulator(&smc91c111_emulator);
}

VMM_DECLARE_MODULE(MODULE_DESC,
		MODULE_AUTHOR,
		MODULE_LICENSE,
		MODULE_IPRIORITY,
		MODULE_INIT,
		MODULE_EXIT);

