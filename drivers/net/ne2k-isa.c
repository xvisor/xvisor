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
 * @file ne2k-isa.c
 * @version 0.1
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief Driver for NE2000 based cards on ISA host bus.
 */

#include <vmm_heap.h>
#include <vmm_host_io.h>
#include <vmm_host_aspace.h>
#include <vmm_stdio.h>
#include <vmm_error.h>
#include <vmm_string.h>
#include <vmm_modules.h>
#include <vmm_ringbuf.h>

#include <vmm_hyperthreads.h>
#include <vmm_wait.h>

#include <net/ne2k.h>

extern virtual_addr_t isa_vbase;

DEFINE_WAIT_LIST(rx_wait_queue);

#define CONFIG_DRIVER_NE2000_BASE 0x300

#define _DEBUG 1
#if _DEBUG
#define PRINTK(fmt,...) 		vmm_printf(fmt, ##args);
#else
#define PRINTK(fmt,...)
#endif

#define MODULE_VARID			ne2k_driver_module
#define MODULE_NAME			"NE2000 Based NIC Driver"
#define MODULE_AUTHOR			"Himanshu Chauhan"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			ne2k_driver_init
#define	MODULE_EXIT			ne2k_driver_exit

#define DELAY_OUTPUT			0x01
#define HAS_MISC_REG			0x02
#define USE_BIG_BUF			0x04
#define HAS_IBM_MISC			0x08
#define IS_DL10019			0x10
#define IS_DL10022			0x20
#define HAS_MII				0x40
#define USE_SHMEM			0x80	/* autodetected */

#define n2k_inb(nic, port)		(vmm_in_8(port + nic->base))
#define n2k_outb(nic, port, val)	(vmm_out_8(port + nic->base, val))

void push_packet_len(struct nic_priv_data *dp, int len);
void push_tx_done(int key, int val);

static struct hw_info hw_inf[] = {
	{ "Generic NE2000", 0x0ff0, 0x00, 0xa0, 0x0c, 0 },
	{ "QEMU NE2000", 0x0, 0x52, 0x54, 0x00, 0 },
	{ NULL, 0x00, 0x00, 0x00, 0x00, 0x00 } /* NULL terminated array */
};

#define PCNET_CMD	0x00
#define PCNET_DATAPORT	0x10	/* NatSemi-defined port window offset. */
#define PCNET_RESET	0x1f	/* Issue a read to reset, a write to clear. */
#define PCNET_MISC	0x18	/* For IBM CCAE and Socket EA cards */

static void pcnet_reset_8390(struct nic_priv_data *dp)
{
	int i, r;

	n2k_outb(dp, E8390_CMD, E8390_NODMA + E8390_PAGE0+E8390_STOP);
	n2k_outb(dp, E8390_CMD, E8390_NODMA+E8390_PAGE1+E8390_STOP);
	n2k_outb(dp, E8390_CMD, E8390_NODMA+E8390_PAGE0+E8390_STOP);
	n2k_outb(dp, E8390_NODMA+E8390_PAGE0+E8390_STOP, E8390_CMD);
	n2k_outb(dp, PCNET_RESET, n2k_inb(dp, PCNET_RESET));

	for (i = 0; i < 100; i++) {
		if ((r = (n2k_inb(dp, EN0_ISR) & ENISR_RESET)) != 0)
			break;
	}
	n2k_outb(dp, EN0_ISR, ENISR_RESET); /* Ack intr. */

	if (i == 100)
		vmm_printf("pcnet_reset_8390() did not complete.\n");
}

int get_prom(struct nic_priv_data *dp, u8* mac_addr)
{
	u8 prom[32];
	int i, j;
	struct {
		unsigned char value, offset;
	} program_seq[] = {
		{E8390_NODMA+E8390_PAGE0+E8390_STOP, E8390_CMD}, /* Select page 0*/
		{0x48, EN0_DCFG},		/* Set byte-wide (0x48) access. */
		{0x00, EN0_RCNTLO},		/* Clear the count regs. */
		{0x00, EN0_RCNTHI},
		{0x00, EN0_IMR},		/* Mask completion irq. */
		{0xFF, EN0_ISR},
		{E8390_RXOFF, EN0_RXCR},	/* 0x20 Set to monitor */
		{E8390_TXOFF, EN0_TXCR},	/* 0x02 and loopback mode. */
		{32, EN0_RCNTLO},
		{0x00, EN0_RCNTHI},
		{0x00, EN0_RSARLO},		/* DMA starting at 0x0000. */
		{0x00, EN0_RSARHI},
		{E8390_RREAD+E8390_START, E8390_CMD},
	};

	pcnet_reset_8390(dp);

	for (i = 0; i < sizeof (program_seq) / sizeof (program_seq[0]); i++)
		n2k_outb (dp, program_seq[i].value, program_seq[i].offset);

	for (i = 0; i < 32; i++) {
		prom[i] = n2k_inb (dp, PCNET_DATAPORT);
	}

	for (i = 0;; i++) {
		if (hw_info[i].dev_name == NULL) break;

		if ((prom[0] == hw_info[i].a0) &&
			(prom[2] == hw_info[i].a1) &&
			(prom[4] == hw_info[i].a2)) {
			vmm_printf("%s detected.\n", hw_info[i].dev_name);
			break;
		}
	}

	if ((prom[28] == 0x57) && (prom[30] == 0x57)) {
		vmm_printf ("MAC address is ");
		for (j = 0; j < 6; j++) {
			if (j) vmm_printf(":");
			mac_addr[j] = prom[j << 1];
			vmm_printf ("%02x", mac_addr[j]);
		}
		vmm_printf ("\n");
	}
	return VMM_OK;
}

static bool dp83902a_init(struct nic_priv_data *dp)
{
	return true;
}

static void
dp83902a_stop(struct nic_priv_data *dp)
{
	n2k_outb(dp, DP_CR, DP_CR_PAGE0 | DP_CR_NODMA | DP_CR_STOP);	/* Brutal */
	n2k_outb(dp, DP_ISR, 0xFF);		/* Clear any pending interrupts */
	n2k_outb(dp, DP_IMR, 0x00);		/* Disable all interrupts */

	dp->running = false;
}

/*
 * This function is called to "start up" the interface. It may be called
 * multiple times, even when the hardware is already running. It will be
 * called whenever something "hardware oriented" changes and should leave
 * the hardware ready to send/receive packets.
 */
static void
dp83902a_start(struct nic_priv_data *dp, u8 * enaddr)
{
	int i;

	n2k_outb(dp, DP_CR, DP_CR_PAGE0 | DP_CR_NODMA | DP_CR_STOP); /* Brutal */
	n2k_outb(dp, DP_DCR, DP_DCR_INIT);
	n2k_outb(dp, DP_RBCH, 0);		/* Remote byte count */
	n2k_outb(dp, DP_RBCL, 0);
	n2k_outb(dp, DP_RCR, DP_RCR_MON);	/* Accept no packets */
	n2k_outb(dp, DP_TCR, DP_TCR_LOCAL);	/* Transmitter [virtually] off */
	n2k_outb(dp, DP_TPSR, dp->tx_buf1);	/* Transmitter start page */
	dp->tx1 = dp->tx2 = 0;
	dp->tx_next = dp->tx_buf1;
	dp->tx_started = false;
	dp->running = true;
	n2k_outb(dp, DP_PSTART, dp->rx_buf_start); /* Receive ring start page */
	n2k_outb(dp, DP_BNDRY, dp->rx_buf_end - 1); /* Receive ring boundary */
	n2k_outb(dp, DP_PSTOP, dp->rx_buf_end);	/* Receive ring end page */
	dp->rx_next = dp->rx_buf_start - 1;
	dp->running = true;
	n2k_outb(dp, DP_ISR, 0xFF);		/* Clear any pending interrupts */
	n2k_outb(dp, DP_IMR, DP_IMR_All);	/* Enable all interrupts */
	n2k_outb(dp, DP_CR, DP_CR_NODMA | DP_CR_PAGE1 | DP_CR_STOP);	/* Select page 1 */
	n2k_outb(dp, DP_P1_CURP, dp->rx_buf_start);	/* Current page - next free page for Rx */
	dp->running = true;
	for (i = 0; i < ETHER_ADDR_LEN; i++) {
		n2k_outb(dp, DP_P1_PAR0+i, enaddr[i]);
	}
	/* Enable and start device */
	n2k_outb(dp, DP_CR, DP_CR_PAGE0 | DP_CR_NODMA | DP_CR_START);
	n2k_outb(dp, DP_TCR, DP_TCR_NORMAL); /* Normal transmit operations */
	n2k_outb(dp, DP_RCR, 0x00); /* No broadcast, no errors, no multicast */
	dp->running = true;
}

/*
 * This routine is called to start the transmitter. It is split out from the
 * data handling routine so it may be called either when data becomes first
 * available or when an Tx interrupt occurs
 */
static void dp83902a_start_xmit(struct nic_priv_data *dp, int start_page, int len)
{
	n2k_outb(dp, DP_ISR, (DP_ISR_TxP | DP_ISR_TxE));
	n2k_outb(dp, DP_CR, DP_CR_PAGE0 | DP_CR_NODMA | DP_CR_START);
	n2k_outb(dp, DP_TBCL, len & 0xFF);
	n2k_outb(dp, DP_TBCH, len >> 8);
	n2k_outb(dp, DP_TPSR, start_page);
	n2k_outb(dp, DP_CR, DP_CR_NODMA | DP_CR_TXPKT | DP_CR_START);

	dp->tx_started = true;
}

/*
 * This routine is called to send data to the hardware. It is known a-priori
 * that there is free buffer space (dp->tx_next).
 */
static void dp83902a_send(struct nic_priv_data *dp, u8 *data, int total_len, u32 key)
{
	int len, start_page, pkt_len, i;
	volatile int isr;

	len = pkt_len = total_len;
	if (pkt_len < IEEE_8023_MIN_FRAME)
		pkt_len = IEEE_8023_MIN_FRAME;

	start_page = dp->tx_next;
	if (dp->tx_next == dp->tx_buf1) {
		dp->tx1 = start_page;
		dp->tx1_len = pkt_len;
		dp->tx1_key = key;
		dp->tx_next = dp->tx_buf2;
	} else {
		dp->tx2 = start_page;
		dp->tx2_len = pkt_len;
		dp->tx2_key = key;
		dp->tx_next = dp->tx_buf1;
	}


	n2k_outb(dp, DP_ISR, DP_ISR_RDC);	/* Clear end of DMA */
	{
		/*
		 * Dummy read. The manual sez something slightly different,
		 * but the code is extended a bit to do what Hitachi's monitor
		 * does (i.e., also read data).
		 */

		u16 tmp;
		int len = 1;

		n2k_outb(dp, DP_RSAL, 0x100 - len);
		n2k_outb(dp, DP_RSAH, (start_page - 1) & 0xff);
		n2k_outb(dp, DP_RBCL, len);
		n2k_outb(dp, DP_RBCH, 0);
		n2k_outb(dp, DP_CR, DP_CR_PAGE0 | DP_CR_RDMA | DP_CR_START);
		DP_IN_DATA(dp->data, tmp);
	}

	/* Send data to device buffer(s) */
	n2k_outb(dp, DP_RSAL, 0);
	n2k_outb(dp, DP_RSAH, start_page);
	n2k_outb(dp, DP_RBCL, pkt_len & 0xFF);
	n2k_outb(dp, DP_RBCH, pkt_len >> 8);
	n2k_outb(dp, DP_CR, DP_CR_WDMA | DP_CR_START);

	/* Put data into buffer */
	while (len > 0) {
		DP_OUT_DATA(dp->data, *data++);
		len--;
	}

	if (total_len < pkt_len) {
		/* Padding to 802.3 length was required */
		for (i = total_len; i < pkt_len;) {
			i++;
			DP_OUT_DATA(dp->data, 0);
		}
	}

	/* Wait for DMA to complete */
	do {
		isr = n2k_inb(dp, DP_ISR);
	} while ((isr & DP_ISR_RDC) == 0);

	/* Then disable DMA */
	n2k_outb(dp, DP_CR, DP_CR_PAGE0 | DP_CR_NODMA | DP_CR_START);

	/* Start transmit if not already going */
	if (!dp->tx_started) {
		if (start_page == dp->tx1) {
			dp->tx_int = 1; /* Expecting interrupt from BUF1 */
		} else {
			dp->tx_int = 2; /* Expecting interrupt from BUF2 */
		}
		dp83902a_start_xmit(dp, start_page, pkt_len);
	}
}

/*
 * This function is called when a packet has been received. It's job is
 * to prepare to unload the packet from the hardware. Once the length of
 * the packet is known, the upper layer of the driver can be told. When
 * the upper layer is ready to unload the packet, the internal function
 * 'dp83902a_recv' will be called to actually fetch it from the hardware.
 */
static void dp83902a_RxEvent(struct nic_priv_data *dp)
{
	u8 rsr;
	u8 rcv_hdr[4];
	int i, len, pkt, cur = 0;

	rsr = n2k_inb(dp, DP_RSR);

	if (!(rsr & 0x01)) {
		return;
	}

	while (true) {
		/* Read incoming packet header */
		n2k_outb(dp, DP_CR, DP_CR_PAGE1 | DP_CR_NODMA | DP_CR_START);
		n2k_outb(dp, DP_P1_CURP, cur);
		n2k_outb(dp, DP_P1_CR, DP_CR_PAGE0 | DP_CR_NODMA | DP_CR_START);
		pkt = n2k_inb(dp, DP_BNDRY);

		pkt += 1;
		if (pkt == dp->rx_buf_end)
			pkt = dp->rx_buf_start;

		if (pkt == cur) {
			break;
		}
		n2k_outb(dp, DP_RBCL, sizeof(rcv_hdr));
		n2k_outb(dp, DP_RBCH, 0);
		n2k_outb(dp, DP_RSAL, 0);
		n2k_outb(dp, DP_RSAH, pkt);
		if (dp->rx_next == pkt) {
			if (cur == dp->rx_buf_start)
				n2k_outb(dp, DP_BNDRY, dp->rx_buf_end - 1);
			else
				n2k_outb(dp, DP_BNDRY, cur - 1); /* Update pointer */
			return;
		}
		dp->rx_next = pkt;
		n2k_outb(dp, DP_ISR, DP_ISR_RDC); /* Clear end of DMA */
		n2k_outb(dp, DP_CR, DP_CR_RDMA | DP_CR_START);

		/* read header (get data size)*/
		for (i = 0; i < sizeof(rcv_hdr);) {
			DP_IN_DATA(dp->data, rcv_hdr[i++]);
		}

		len = ((rcv_hdr[3] << 8) | rcv_hdr[2]) - sizeof(rcv_hdr);

		/* FIXME: Tell that data has been read */
		push_packet_len(dp, len);

		if (rcv_hdr[1] == dp->rx_buf_start)
			n2k_outb(dp, DP_BNDRY, dp->rx_buf_end - 1);
		else
			n2k_outb(dp, DP_BNDRY, rcv_hdr[1] - 1); /* Update pointer */
	}
}

/*
 * This function is called as a result of the "eth_drv_recv()" call above.
 * It's job is to actually fetch data for a packet from the hardware once
 * memory buffers have been allocated for the packet. Note that the buffers
 * may come in pieces, using a scatter-gather list. This allows for more
 * efficient processing in the upper layers of the stack.
 */
static void dp83902a_recv(struct nic_priv_data *dp, int len)
{
	u8 *base = dp->base;
	int i, mlen;
	u8 saved_char = 0;
	bool saved;

	/* Read incoming packet data */
	DP_OUT(base, DP_CR, DP_CR_PAGE0 | DP_CR_NODMA | DP_CR_START);
	DP_OUT(base, DP_RBCL, len & 0xFF);
	DP_OUT(base, DP_RBCH, len >> 8);
	DP_OUT(base, DP_RSAL, 4);		/* Past header */
	DP_OUT(base, DP_RSAH, dp->rx_next);
	DP_OUT(base, DP_ISR, DP_ISR_RDC); /* Clear end of DMA */
	DP_OUT(base, DP_CR, DP_CR_RDMA | DP_CR_START);

	saved = false;
	for (i = 0; i < 1; i++) {
		mlen = len;
		while (0 < mlen) {
			/* Saved byte from previous loop? */
			if (saved) {
				vmm_ringbuf_enqueue(dp->rx_rb, &saved_char, TRUE);
				mlen--;
				saved = false;
				continue;
			}
			
			{
				u8 tmp;
				DP_IN_DATA(dp->data, tmp);
				vmm_ringbuf_enqueue(dp->rx_rb, &tmp, TRUE);
				mlen--;
			}
		}
	}
}

static void
dp83902a_TxEvent(struct nic_priv_data *dp)
{
	u8 *base = dp->base;
	u8 tsr;
	u32 key;

	DP_IN(base, DP_TSR, tsr);
	if (dp->tx_int == 1) {
		key = dp->tx1_key;
		dp->tx1 = 0;
	} else {
		key = dp->tx2_key;
		dp->tx2 = 0;
	}
	/* Start next packet if one is ready */
	dp->tx_started = false;
	if (dp->tx1) {
		dp83902a_start_xmit(dp, dp->tx1, dp->tx1_len);
		dp->tx_int = 1;
	} else if (dp->tx2) {
		dp83902a_start_xmit(dp, dp->tx2, dp->tx2_len);
		dp->tx_int = 2;
	} else {
		dp->tx_int = 0;
	}
	
	/* FIXME: Tell higher level we sent this packet */
	push_tx_done(key, 0);
}

/*
 * Read the tally counters to clear them. Called in response to a CNT
 * interrupt.
 */
static void
dp83902a_ClearCounters(struct nic_priv_data *dp)
{
	u8 *base = dp->base;
	u8 cnt1, cnt2, cnt3;

	DP_IN(base, DP_FER, cnt1);
	DP_IN(base, DP_CER, cnt2);
	DP_IN(base, DP_MISSED, cnt3);
	DP_OUT(base, DP_ISR, DP_ISR_CNT);
}

/*
 * Deal with an overflow condition. This code follows the procedure set
 * out in section 7.0 of the datasheet.
 */
static void
dp83902a_Overflow(struct nic_priv_data *dp)
{
	u8 *base = dp->base;
	u8 isr;

	/* Issue a stop command and wait 1.6ms for it to complete. */
	DP_OUT(base, DP_CR, DP_CR_STOP | DP_CR_NODMA);

	/* Clear the remote byte counter registers. */
	DP_OUT(base, DP_RBCL, 0);
	DP_OUT(base, DP_RBCH, 0);

	/* Enter loopback mode while we clear the buffer. */
	DP_OUT(base, DP_TCR, DP_TCR_LOCAL);
	DP_OUT(base, DP_CR, DP_CR_START | DP_CR_NODMA);

	/*
	 * Read in as many packets as we can and acknowledge any and receive
	 * interrupts. Since the buffer has overflowed, a receive event of
	 * some kind will have occured.
	 */
	dp83902a_RxEvent(dp);
	DP_OUT(base, DP_ISR, DP_ISR_RxP|DP_ISR_RxE);

	/* Clear the overflow condition and leave loopback mode. */
	DP_OUT(base, DP_ISR, DP_ISR_OFLW);
	DP_OUT(base, DP_TCR, DP_TCR_NORMAL);

	/*
	 * If a transmit command was issued, but no transmit event has occured,
	 * restart it here.
	 */
	DP_IN(base, DP_ISR, isr);
	if (dp->tx_started && !(isr & (DP_ISR_TxP|DP_ISR_TxE))) {
		DP_OUT(base, DP_CR, DP_CR_NODMA | DP_CR_TXPKT | DP_CR_START);
	}
}

static void
dp83902a_poll(struct nic_priv_data *dp)
{
	u8 *base = dp->base;
	volatile u8 isr = 0;

	DP_OUT(base, DP_CR, DP_CR_NODMA | DP_CR_PAGE0 | DP_CR_START);
	while (1) {
		wait_on_event_running((isr = n2k_inb(dp, DP_ISR)) != 0)

		/*
		 * The CNT interrupt triggers when the MSB of one of the error
		 * counters is set. We don't much care about these counters, but
		 * we should read their values to reset them.
		 */
		if (isr & DP_ISR_CNT) {
			dp83902a_ClearCounters(dp);
		}
		/*
		 * Check for overflow. It's a special case, since there's a
		 * particular procedure that must be followed to get back into
		 * a running state.a
		 */
		if (isr & DP_ISR_OFLW) {
			dp83902a_Overflow(dp);
		} else {
			/*
			 * Other kinds of interrupts can be acknowledged simply by
			 * clearing the relevant bits of the ISR. Do that now, then
			 * handle the interrupts we care about.
			 */
			DP_OUT(base, DP_ISR, isr);	/* Clear set bits */
			if (!dp->running) break;	/* Is this necessary? */
			/*
			 * Check for tx_started on TX event since these may happen
			 * spuriously it seems.
			 */
			if (isr & (DP_ISR_TxP|DP_ISR_TxE) && dp->tx_started) {
				dp83902a_TxEvent(dp);
			}
			if (isr & DP_ISR_RxP) {
				dp83902a_RxEvent(dp);
			}
		}
	}
}


static int pkey = -1;

void push_packet_len(struct nic_priv_data *dp, int len)
{
	vmm_printf("pushed len = %d\n", len);
	if (len >= 2000) {
		vmm_printf("NE2000: packet too big\n");
		return;
	}
	dp83902a_recv(dp, len);

	/* FIXME: Just pass it to the upper layer*/
	//NetReceive(&pbuf[0], len);
}

void push_tx_done(int key, int val)
{
	pkey = key;
}

int ne2k_init(struct nic_priv_data *nic_data)
{
	int r;
	u8 eth_addr[6];

	if (!nic_data) {
		return VMM_EFAIL;
	}

	if (!nic_data->rx_rb) {
		nic_data->rx_rb = vmm_ringbuf_alloc(1, 2000);
		if (!nic_data->rx_rb) {
			vmm_printf("Cannot allocate receive buffer\n");
			return VMM_EFAIL;
		}
	}

	nic_data->base = (u8 *) (isa_vbase + CONFIG_DRIVER_NE2000_BASE);

	r = get_prom(nic_data, eth_addr);
	if (r != VMM_OK)
		return VMM_EFAIL;

	nic_data->data = nic_data->base + DP_DATA;
	nic_data->tx_buf1 = START_PG;
	nic_data->tx_buf2 = START_PG2;
	nic_data->rx_buf_start = RX_START;
	nic_data->rx_buf_end = RX_END;
	vmm_memcpy(nic_data->esa, eth_addr, sizeof(nic_data->esa));

	if (dp83902a_init(nic_data) != true) {
		return VMM_EFAIL;
	}

	dp83902a_start(nic_data, eth_addr);
	nic_data->initialized = 1;

	return VMM_OK;
}

void ne2k_halt(struct nic_priv_data *nic_data)
{
	if(nic_data->initialized)
		dp83902a_stop(nic_data);

	nic_data->initialized = 0;
}

int ne2k_rx(struct nic_priv_data *nic_data)
{
	dp83902a_poll(nic_data);
	return 1;
}

int ne2k_send(struct nic_priv_data *nic_data, volatile void *packet, int length)
{
	pkey = -1;

	dp83902a_send(nic_data, (u8 *) packet, length, 666);
	/* FIXME: Put timeout here. */
	while(1) {
		dp83902a_poll(nic_data);
		if (pkey != -1) {
			vmm_printf("Packet sucesfully sent\n");
			return 0;
		}
	}
	return 0;
}

static int ne2k_read(vmm_netdev_t *ndev, 
		char *dest, size_t offset, size_t len)
{
	struct nic_priv_data *pdata;

	if(!ndev || !dest) {
		return 0;
	}
	if(!ndev->priv) {
		return 0;
	}

	pdata = ndev->priv;

	/* FIXME: Read from device buffer */
	return ne2k_rx(pdata);
}

static int ne2k_write(vmm_netdev_t *ndev,
		char *src, size_t offset, size_t len)
{
	struct nic_priv_data *pdata;

	if(!ndev || !src) {
		return VMM_EFAIL;
	}
	if(!ndev->priv) {
		return VMM_EFAIL;
	}

	pdata = ndev->priv;

	if (ne2k_send(pdata, (src + offset), len)) {
		return VMM_EFAIL;
	}

	return len;
}

static int ne2k_driver_probe(vmm_device_t *dev, const vmm_devid_t *devid)
{
	int rc;
	vmm_netdev_t *ndev;
	struct nic_priv_data *priv_data;
	
	ndev = vmm_malloc(sizeof(vmm_netdev_t));
	if(!ndev) {
		rc = VMM_EFAIL;
		goto free_nothing;
	}

	priv_data = vmm_malloc(sizeof(struct nic_priv_data));
	if(!priv_data) {
		rc = VMM_EFAIL;
		goto free_chardev;
	}

	if (ne2k_init(priv_data)) {
		rc = VMM_EFAIL;
		goto free_chardev;
	}

	priv_data->txrx_thread = vmm_hyperthread_create("ne2k-isa-driver", dp83902a_poll, priv_data);

	if (priv_data == NULL) {
		rc = VMM_EFAIL;
		goto free_chardev;
	}

	vmm_hyperthread_run(priv_data->txrx_thread);

	vmm_strcpy(ndev->name, dev->node->name);
	ndev->dev = dev;
	ndev->ioctl = NULL;
	ndev->read = ne2k_read;
	ndev->write = ne2k_write;
	ndev->priv = (void *)priv_data;

	rc = vmm_netdev_register(ndev);
	if(rc) {
		goto free_port;
	}

	dev->priv = (void *)ndev;

	return VMM_OK;

free_port:
	vmm_free(priv_data);
free_chardev:
	vmm_free(ndev);
free_nothing:
	return rc;
}

static int ne2k_driver_remove(vmm_device_t *dev)
{
	vmm_netdev_t *ndev;
	struct nic_priv_data *priv_data;

	ndev = (vmm_netdev_t *)dev->priv;
	priv_data = (struct nic_priv_data *)ndev->priv;

	vmm_ringbuf_free(priv_data->rx_rb);
	vmm_free(priv_data);
	vmm_free(ndev);

	return 0;
}

static vmm_devid_t ne2k_devid_table[] = {
	{ .type = "nic", .compatible = "ne2000"},
	{ /* end of list */ },
};

static vmm_driver_t ne2k_driver = {
	.name = "ne2k_driver",
	.match_table = ne2k_devid_table,
	.probe = ne2k_driver_probe,
	.remove = ne2k_driver_remove,
};

static int __init ne2k_driver_init(void)
{
	return vmm_devdrv_register_driver(&ne2k_driver);
}

static void ne2k_driver_exit(void)
{
	vmm_devdrv_unregister_driver(&ne2k_driver);
}

VMM_DECLARE_MODULE(MODULE_VARID,
		MODULE_NAME,
		MODULE_AUTHOR,
		MODULE_IPRIORITY,
		MODULE_INIT,
		MODULE_EXIT);
