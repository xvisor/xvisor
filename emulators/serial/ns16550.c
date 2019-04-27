/**
 * Copyright (c) 2014 Himanshu Chauhan.
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
 * @file ns16550.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @author Anup Patel (anup@brainfault.org)
 * @brief 16550A UART Emulator
 *
 * This source has been largely adapted from KVMTOOL hw/serial.c
 *
 * The original source is licensed under GPLv2.
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_devtree.h>
#include <vmm_devemu.h>
#include <vio/vmm_vserial.h>
#include <libs/fifo.h>
#include <libs/stringlib.h>

#define MODULE_DESC			"NS16550 Class UART Emulator"
#define MODULE_AUTHOR			"Himanshu Chauhan"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(VMM_VSERIAL_IPRIORITY+1)
#define	MODULE_INIT			ns16550_emulator_init
#define	MODULE_EXIT			ns16550_emulator_exit

/*
 * This fakes a U6_16550A. The fifo len needs to be 64 as the kernel
 * expects that for autodetection.
 */
#define FIFO_LEN		64
#define FIFO_MASK		(FIFO_LEN - 1)

#define UART_IIR_TYPE_BITS	0xc0

/*
 * DLAB=0
 */
#define UART_RX		0	/* In:  Receive buffer */
#define UART_TX		0	/* Out: Transmit buffer */

#define UART_IER	1	/* Out: Interrupt Enable Register */
#define UART_IER_MSI		0x08 /* Enable Modem status interrupt */
#define UART_IER_RLSI		0x04 /* Enable receiver line status interrupt */
#define UART_IER_THRI		0x02 /* Enable Transmitter holding register int. */
#define UART_IER_RDI		0x01 /* Enable receiver data interrupt */
/*
 * Sleep mode for ST16650 and TI16750.  For the ST16650, EFR[4]=1
 */
#define UART_IERX_SLEEP		0x10 /* Enable sleep mode */

#define UART_IIR	2	/* In:  Interrupt ID Register */
#define UART_IIR_NO_INT		0x01 /* No interrupts pending */
#define UART_IIR_ID		0x0e /* Mask for the interrupt ID */
#define UART_IIR_MSI		0x00 /* Modem status interrupt */
#define UART_IIR_THRI		0x02 /* Transmitter holding register empty */
#define UART_IIR_RDI		0x04 /* Receiver data interrupt */
#define UART_IIR_RLSI		0x06 /* Receiver line status interrupt */

#define UART_IIR_BUSY		0x07 /* DesignWare APB Busy Detect */

#define UART_IIR_RX_TIMEOUT	0x0c /* OMAP RX Timeout interrupt */
#define UART_IIR_XOFF		0x10 /* OMAP XOFF/Special Character */
#define UART_IIR_CTS_RTS_DSR	0x20 /* OMAP CTS/RTS/DSR Change */

#define UART_FCR	2	/* Out: FIFO Control Register */
#define UART_FCR_ENABLE_FIFO	0x01 /* Enable the FIFO */
#define UART_FCR_CLEAR_RCVR	0x02 /* Clear the RCVR FIFO */
#define UART_FCR_CLEAR_XMIT	0x04 /* Clear the XMIT FIFO */
#define UART_FCR_DMA_SELECT	0x08 /* For DMA applications */
/*
 * Note: The FIFO trigger levels are chip specific:
 *	RX:76 = 00  01  10  11	TX:54 = 00  01  10  11
 * PC16550D:	 1   4   8  14		xx  xx  xx  xx
 * TI16C550A:	 1   4   8  14          xx  xx  xx  xx
 * TI16C550C:	 1   4   8  14          xx  xx  xx  xx
 * ST16C550:	 1   4   8  14		xx  xx  xx  xx
 * ST16C650:	 8  16  24  28		16   8  24  30	PORT_16650V2
 * NS16C552:	 1   4   8  14		xx  xx  xx  xx
 * ST16C654:	 8  16  56  60		 8  16  32  56	PORT_16654
 * TI16C750:	 1  16  32  56		xx  xx  xx  xx	PORT_16750
 * TI16C752:	 8  16  56  60		 8  16  32  56
 * Tegra:	 1   4   8  14		16   8   4   1	PORT_TEGRA
 */
#define UART_FCR_R_TRIG_00	0x00
#define UART_FCR_R_TRIG_01	0x40
#define UART_FCR_R_TRIG_10	0x80
#define UART_FCR_R_TRIG_11	0xc0
#define UART_FCR_T_TRIG_00	0x00
#define UART_FCR_T_TRIG_01	0x10
#define UART_FCR_T_TRIG_10	0x20
#define UART_FCR_T_TRIG_11	0x30

#define UART_FCR_TRIGGER_MASK	0xC0 /* Mask for the FIFO trigger range */
#define UART_FCR_TRIGGER_1	0x00 /* Mask for trigger set at 1 */
#define UART_FCR_TRIGGER_4	0x40 /* Mask for trigger set at 4 */
#define UART_FCR_TRIGGER_8	0x80 /* Mask for trigger set at 8 */
#define UART_FCR_TRIGGER_14	0xC0 /* Mask for trigger set at 14 */
/* 16650 definitions */
#define UART_FCR6_R_TRIGGER_8	0x00 /* Mask for receive trigger set at 1 */
#define UART_FCR6_R_TRIGGER_16	0x40 /* Mask for receive trigger set at 4 */
#define UART_FCR6_R_TRIGGER_24  0x80 /* Mask for receive trigger set at 8 */
#define UART_FCR6_R_TRIGGER_28	0xC0 /* Mask for receive trigger set at 14 */
#define UART_FCR6_T_TRIGGER_16	0x00 /* Mask for transmit trigger set at 16 */
#define UART_FCR6_T_TRIGGER_8	0x10 /* Mask for transmit trigger set at 8 */
#define UART_FCR6_T_TRIGGER_24  0x20 /* Mask for transmit trigger set at 24 */
#define UART_FCR6_T_TRIGGER_30	0x30 /* Mask for transmit trigger set at 30 */
#define UART_FCR7_64BYTE	0x20 /* Go into 64 byte mode (TI16C750 and
					some Freescale UARTs) */

#define UART_FCR_R_TRIG_SHIFT		6
#define UART_FCR_R_TRIG_BITS(x)		\
	(((x) & UART_FCR_TRIGGER_MASK) >> UART_FCR_R_TRIG_SHIFT)
#define UART_FCR_R_TRIG_MAX_STATE	4

#define UART_LCR	3	/* Out: Line Control Register */
/*
 * Note: if the word length is 5 bits (UART_LCR_WLEN5), then setting
 * UART_LCR_STOP will select 1.5 stop bits, not 2 stop bits.
 */
#define UART_LCR_DLAB		0x80 /* Divisor latch access bit */
#define UART_LCR_SBC		0x40 /* Set break control */
#define UART_LCR_SPAR		0x20 /* Stick parity (?) */
#define UART_LCR_EPAR		0x10 /* Even parity select */
#define UART_LCR_PARITY		0x08 /* Parity Enable */
#define UART_LCR_STOP		0x04 /* Stop bits: 0=1 bit, 1=2 bits */
#define UART_LCR_WLEN5		0x00 /* Wordlength: 5 bits */
#define UART_LCR_WLEN6		0x01 /* Wordlength: 6 bits */
#define UART_LCR_WLEN7		0x02 /* Wordlength: 7 bits */
#define UART_LCR_WLEN8		0x03 /* Wordlength: 8 bits */

/*
 * Access to some registers depends on register access / configuration
 * mode.
 */
#define UART_LCR_CONF_MODE_A	UART_LCR_DLAB	/* Configutation mode A */
#define UART_LCR_CONF_MODE_B	0xBF		/* Configutation mode B */

#define UART_MCR	4	/* Out: Modem Control Register */
#define UART_MCR_CLKSEL		0x80 /* Divide clock by 4 (TI16C752, EFR[4]=1) */
#define UART_MCR_TCRTLR		0x40 /* Access TCR/TLR (TI16C752, EFR[4]=1) */
#define UART_MCR_XONANY		0x20 /* Enable Xon Any (TI16C752, EFR[4]=1) */
#define UART_MCR_AFE		0x20 /* Enable auto-RTS/CTS (TI16C550C/TI16C750) */
#define UART_MCR_LOOP		0x10 /* Enable loopback test mode */
#define UART_MCR_OUT2		0x08 /* Out2 complement */
#define UART_MCR_OUT1		0x04 /* Out1 complement */
#define UART_MCR_RTS		0x02 /* RTS complement */
#define UART_MCR_DTR		0x01 /* DTR complement */

#define UART_LSR	5	/* In:  Line Status Register */
#define UART_LSR_FIFOE		0x80 /* Fifo error */
#define UART_LSR_TEMT		0x40 /* Transmitter empty */
#define UART_LSR_THRE		0x20 /* Transmit-hold-register empty */
#define UART_LSR_BI		0x10 /* Break interrupt indicator */
#define UART_LSR_FE		0x08 /* Frame error indicator */
#define UART_LSR_PE		0x04 /* Parity error indicator */
#define UART_LSR_OE		0x02 /* Overrun error indicator */
#define UART_LSR_DR		0x01 /* Receiver data ready */
#define UART_LSR_BRK_ERROR_BITS	0x1E /* BI, FE, PE, OE bits */

#define UART_MSR	6	/* In:  Modem Status Register */
#define UART_MSR_DCD		0x80 /* Data Carrier Detect */
#define UART_MSR_RI		0x40 /* Ring Indicator */
#define UART_MSR_DSR		0x20 /* Data Set Ready */
#define UART_MSR_CTS		0x10 /* Clear to Send */
#define UART_MSR_DDCD		0x08 /* Delta DCD */
#define UART_MSR_TERI		0x04 /* Trailing edge ring indicator */
#define UART_MSR_DDSR		0x02 /* Delta DSR */
#define UART_MSR_DCTS		0x01 /* Delta CTS */
#define UART_MSR_ANY_DELTA	0x0F /* Any of the delta bits! */

#define UART_SCR	7	/* I/O: Scratch Register */

/*
 * DLAB=1
 */
#define UART_DLL	0	/* Out: Divisor Latch Low */
#define UART_DLM	1	/* Out: Divisor Latch High */
#define UART_DIV_MAX	0xFFFF	/* Max divisor value */

/*
 * LCR=0xBF (or DLAB=1 for 16C660)
 */
#define UART_EFR	2	/* I/O: Extended Features Register */
#define UART_XR_EFR	9	/* I/O: Extended Features Register (XR17D15x) */
#define UART_EFR_CTS		0x80 /* CTS flow control */
#define UART_EFR_RTS		0x40 /* RTS flow control */
#define UART_EFR_SCD		0x20 /* Special character detect */
#define UART_EFR_ECB		0x10 /* Enhanced control bit */
/*
 * the low four bits control software flow control
 */

/*
 * LCR=0xBF, TI16C752, ST16650, ST16650A, ST16654
 */
#define UART_XON1	4	/* I/O: Xon character 1 */
#define UART_XON2	5	/* I/O: Xon character 2 */
#define UART_XOFF1	6	/* I/O: Xoff character 1 */
#define UART_XOFF2	7	/* I/O: Xoff character 2 */

/*
 * EFR[4]=1 MCR[6]=1, TI16C752
 */
#define UART_TI752_TCR	6	/* I/O: transmission control register */
#define UART_TI752_TLR	7	/* I/O: trigger level register */

/*
 * LCR=0xBF, XR16C85x
 */
#define UART_TRG	0	/* FCTR bit 7 selects Rx or Tx
				 * In: Fifo count
				 * Out: Fifo custom trigger levels */
/*
 * These are the definitions for the Programmable Trigger Register
 */
#define UART_TRG_1		0x01
#define UART_TRG_4		0x04
#define UART_TRG_8		0x08
#define UART_TRG_16		0x10
#define UART_TRG_32		0x20
#define UART_TRG_64		0x40
#define UART_TRG_96		0x60
#define UART_TRG_120		0x78
#define UART_TRG_128		0x80

#define UART_FCTR	1	/* Feature Control Register */
#define UART_FCTR_RTS_NODELAY	0x00  /* RTS flow control delay */
#define UART_FCTR_RTS_4DELAY	0x01
#define UART_FCTR_RTS_6DELAY	0x02
#define UART_FCTR_RTS_8DELAY	0x03
#define UART_FCTR_IRDA		0x04  /* IrDa data encode select */
#define UART_FCTR_TX_INT	0x08  /* Tx interrupt type select */
#define UART_FCTR_TRGA		0x00  /* Tx/Rx 550 trigger table select */
#define UART_FCTR_TRGB		0x10  /* Tx/Rx 650 trigger table select */
#define UART_FCTR_TRGC		0x20  /* Tx/Rx 654 trigger table select */
#define UART_FCTR_TRGD		0x30  /* Tx/Rx 850 programmable trigger select */
#define UART_FCTR_SCR_SWAP	0x40  /* Scratch pad register swap */
#define UART_FCTR_RX		0x00  /* Programmable trigger mode select */
#define UART_FCTR_TX		0x80  /* Programmable trigger mode select */

/*
 * LCR=0xBF, FCTR[6]=1
 */
#define UART_EMSR	7	/* Extended Mode Select Register */
#define UART_EMSR_FIFO_COUNT	0x01  /* Rx/Tx select */
#define UART_EMSR_ALT_COUNT	0x02  /* Alternating count select */

/*
 * The Intel XScale on-chip UARTs define these bits
 */
#define UART_IER_DMAE	0x80	/* DMA Requests Enable */
#define UART_IER_UUE	0x40	/* UART Unit Enable */
#define UART_IER_NRZE	0x20	/* NRZ coding Enable */
#define UART_IER_RTOIE	0x10	/* Receiver Time Out Interrupt Enable */

#define UART_IIR_TOD	0x08	/* Character Timeout Indication Detected */

#define UART_FCR_PXAR1	0x00	/* receive FIFO threshold = 1 */
#define UART_FCR_PXAR8	0x40	/* receive FIFO threshold = 8 */
#define UART_FCR_PXAR16	0x80	/* receive FIFO threshold = 16 */
#define UART_FCR_PXAR32	0xc0	/* receive FIFO threshold = 32 */

struct ns16550_state {
	struct vmm_guest *guest;
	struct vmm_vserial *vser;
	vmm_spinlock_t lock;

	u32 irq;
	u32 reg_shift;
	u32 reg_io_width;

	u8 dll;
	u8 dlm;
	u8 iir;
	u8 ier;
	u8 fcr;
	u8 lcr;
	u8 mcr;
	u8 lsr;
	u8 msr;
	u8 scr;
	u8 irq_state;

	struct fifo *recv_fifo;
	struct fifo *xmit_fifo;
};

static void __ns16550_irq_raise(struct ns16550_state *s)
{
	vmm_devemu_emulate_irq(s->guest, s->irq, 1);
}

static void __ns16550_irq_lower(struct ns16550_state *s)
{
	vmm_devemu_emulate_irq(s->guest, s->irq, 0);
}

static void __ns16550_flush_tx(struct ns16550_state *s)
{
	u8 data;

	s->lsr |= UART_LSR_TEMT | UART_LSR_THRE;

	while (!fifo_isempty(s->xmit_fifo)) {
		fifo_dequeue(s->xmit_fifo, &data);
		vmm_vserial_receive(s->vser, &data, 1);
	}
}

static void __ns16550_update_irq(struct ns16550_state *s)
{
	u8 iir = 0;

	/* Handle clear rx */
	if (s->lcr & UART_FCR_CLEAR_RCVR) {
		s->lcr &= ~UART_FCR_CLEAR_RCVR;
		fifo_clear(s->recv_fifo);
		s->lsr &= ~UART_LSR_DR;
	}

	/* Handle clear tx */
	if (s->lcr & UART_FCR_CLEAR_XMIT) {
		s->lcr &= ~UART_FCR_CLEAR_XMIT;
		fifo_clear(s->xmit_fifo);
		s->lsr |= UART_LSR_TEMT | UART_LSR_THRE;
	}

	/* Data ready and rcv interrupt enabled ? */
	if ((s->ier & UART_IER_RDI) && (s->lsr & UART_LSR_DR))
		iir |= UART_IIR_RDI;

	/* Transmitter empty and interrupt enabled ? */
	if ((s->ier & UART_IER_THRI) && (s->lsr & UART_LSR_TEMT))
		iir |= UART_IIR_THRI;

	/* Now update the irq line, if necessary */
	if (!iir) {
		s->iir = UART_IIR_NO_INT;
		if (s->irq_state)
			__ns16550_irq_lower(s);
	} else {
		s->iir = iir;
		if (!s->irq_state)
			__ns16550_irq_raise(s);
	}
	s->irq_state = iir;

	/*
	 * If the kernel disabled the tx interrupt, we know that there
	 * is nothing more to transmit, so we can reset our tx logic
	 * here.
	 */
	if (!(s->ier & UART_IER_THRI))
		__ns16550_flush_tx(s);
}

static int ns16550_reg_write(struct ns16550_state *s, u32 addr,
			     u32 src_mask, u32 val, u32 io_width)
{
	u8 data8;
	int ret = VMM_OK;
	bool update_irq = FALSE;

	if (s->reg_io_width != io_width)
		return VMM_EINVALID;

	addr >>= s->reg_shift;
	addr &= 7;

	vmm_spin_lock(&s->lock);

	switch (addr) {
	case UART_TX:
		update_irq = TRUE;

		if (s->lcr & UART_LCR_DLAB) {
			s->dll = val;
			break;
		}

		/* Loopback mode */
		if (s->mcr & UART_MCR_LOOP) {
			if (!fifo_isfull(s->recv_fifo)) {
				data8 = val;
				fifo_enqueue(s->recv_fifo, &data8, FALSE);
				s->lsr |= UART_LSR_DR;
			}
			break;
		}

		if (!fifo_isfull(s->xmit_fifo)) {
			data8 = val;
			fifo_enqueue(s->xmit_fifo, &data8, FALSE);
			s->lsr &= ~UART_LSR_TEMT;
			if (fifo_avail(s->xmit_fifo) == FIFO_LEN / 2)
				s->lsr &= ~UART_LSR_THRE;
			__ns16550_flush_tx(s);
		} else {
			/* Should never happpen */
			s->lsr &= ~(UART_LSR_TEMT | UART_LSR_THRE);
		}
		break;
	case UART_IER:
		if (!(s->lcr & UART_LCR_DLAB))
			s->ier = val & 0x0f;
		else
			s->dlm = val;
		update_irq = TRUE;
		break;
	case UART_FCR:
		s->fcr = val;
		update_irq = TRUE;
		break;
	case UART_LCR:
		s->lcr = val;
		update_irq = TRUE;
		break;
	case UART_MCR:
		s->mcr = val;
		update_irq = TRUE;
		break;
	case UART_LSR:
		/* Factory test */
		break;
	case UART_MSR:
		/* Not used */
		break;
	case UART_SCR:
		s->scr = val;
		break;
	default:
		ret = VMM_EINVALID;
		break;
	}

	if (update_irq)
		__ns16550_update_irq(s);

	vmm_spin_unlock(&s->lock);

	return ret;
}

static void __ns16550_recv(struct ns16550_state *s, u32 *dst)
{
	u8 data8 = 0;

	*dst = 0;

	if (fifo_isempty(s->recv_fifo)) {
		s->lsr &= ~UART_LSR_DR;
		return;
	}

	/* Break issued ? */
	if (s->lsr & UART_LSR_BI) {
		s->lsr &= ~UART_LSR_BI;
		return;
	}

	fifo_dequeue(s->recv_fifo, &data8);
	*dst = data8;

	if (fifo_isempty(s->recv_fifo)) {
		s->lsr &= ~UART_LSR_DR;
	}
}

static int ns16550_reg_read(struct ns16550_state *s,
			    u32 addr, u32 *dst, u32 io_width)
{
	int ret = VMM_OK;
	bool update_irq = FALSE;

	if (s->reg_io_width != io_width)
		return VMM_EINVALID;

	addr >>= s->reg_shift;
	addr &= 7;

	vmm_spin_lock(&s->lock);

	switch (addr) {
	case UART_RX:
		if (s->lcr & UART_LCR_DLAB)
			*dst = s->dll;
		else
			__ns16550_recv(s, dst);
		update_irq = TRUE;
		break;
	case UART_IER:
		if (s->lcr & UART_LCR_DLAB)
			*dst = s->dlm;
		else
			*dst = s->ier;
		break;
	case UART_IIR:
		*dst = s->iir | UART_IIR_TYPE_BITS;
		break;
	case UART_LCR:
		*dst = s->lcr;
		break;
	case UART_MCR:
		*dst = s->mcr;
		break;
	case UART_LSR:
		*dst = s->lsr;
		break;
	case UART_MSR:
		*dst = s->msr;
		break;
	case UART_SCR:
		*dst = s->scr;
		break;
	default:
		ret = VMM_EINVALID;
		break;
	}

	if (update_irq)
		__ns16550_update_irq(s);

	vmm_spin_unlock(&s->lock);

	return ret;
}

static bool ns16550_can_send(struct vmm_vserial *vser)
{
	bool ret;
	struct ns16550_state *s = vmm_vserial_priv(vser);

	vmm_spin_lock(&s->lock);

	if (s->mcr & UART_MCR_LOOP)
		ret = FALSE;
	else
		ret = fifo_isfull(s->recv_fifo) ? FALSE : TRUE;

	vmm_spin_unlock(&s->lock);

	return ret;
}

static int ns16550_send(struct vmm_vserial *vser, u8 data)
{
	struct ns16550_state *s = vmm_vserial_priv(vser);

	vmm_spin_lock(&s->lock);

	if (s->mcr & UART_MCR_LOOP) {
		vmm_spin_unlock(&s->lock);
		return VMM_OK;
	}

	fifo_enqueue(s->recv_fifo, &data, FALSE);

	s->lsr |= UART_LSR_DR;

	__ns16550_update_irq(s);

	vmm_spin_unlock(&s->lock);


	return VMM_OK;
}

static int ns16550_emulator_reset(struct vmm_emudev *edev)
{
	struct ns16550_state *s = edev->priv;

	vmm_spin_lock(&s->lock);

	s->ier = 0;
	s->iir = UART_IIR_NO_INT;
	s->lcr = 0;
	s->lsr = UART_LSR_TEMT | UART_LSR_THRE;
	s->msr = UART_MSR_DCD | UART_MSR_DSR | UART_MSR_CTS;
	s->dll = 0x0C;
	s->mcr = UART_MCR_OUT2;
	s->scr = 0;
	s->irq_state = 0;

	fifo_clear(s->recv_fifo);
	fifo_clear(s->xmit_fifo);

	__ns16550_irq_lower(s);

	vmm_spin_unlock(&s->lock);

	return VMM_OK;
}

static int ns16550_emulator_read8(struct vmm_emudev *edev,
				  physical_addr_t offset,
				  u8 *dst)
{
	int rc;
	u32 regval = 0x0;

	rc = ns16550_reg_read(edev->priv, offset, &regval, 1);
	if (!rc) {
		*dst = regval & 0xFF;
	}

	return rc;
}

static int ns16550_emulator_read16(struct vmm_emudev *edev,
				   physical_addr_t offset,
				   u16 *dst)
{
	int rc;
	u32 regval = 0x0;

	rc = ns16550_reg_read(edev->priv, offset, &regval, 2);
	if (!rc) {
		*dst = regval & 0xFFFF;
	}

	return rc;
}

static int ns16550_emulator_read32(struct vmm_emudev *edev,
				   physical_addr_t offset,
				   u32 *dst)
{
	return ns16550_reg_read(edev->priv, offset, dst, 4);
}

static int ns16550_emulator_write8(struct vmm_emudev *edev,
				   physical_addr_t offset,
				   u8 src)
{
	return ns16550_reg_write(edev->priv, offset, 0xFFFFFF00, src, 1);
}

static int ns16550_emulator_write16(struct vmm_emudev *edev,
				    physical_addr_t offset,
				    u16 src)
{
	return ns16550_reg_write(edev->priv, offset, 0xFFFF0000, src, 2);
}

static int ns16550_emulator_write32(struct vmm_emudev *edev,
				    physical_addr_t offset,
				    u32 src)
{
	return ns16550_reg_write(edev->priv, offset, 0x00000000, src, 4);
}

static int ns16550_emulator_probe(struct vmm_guest *guest,
				  struct vmm_emudev *edev,
				  const struct vmm_devtree_nodeid *eid)
{
	int rc = VMM_OK;
	char name[64];
	struct ns16550_state *s;

	s = vmm_zalloc(sizeof(struct ns16550_state));
	if (!s) {
		vmm_lerror(edev->node->name,
			   "Failed to allocate serial emulator's state.\n");
		rc = VMM_EFAIL;
		goto uart16550a_emulator_probe_done;
	}

	s->guest = guest;
	INIT_SPIN_LOCK(&s->lock);

	rc = vmm_devtree_read_u32_atindex(edev->node,
					  VMM_DEVTREE_INTERRUPTS_ATTR_NAME,
					  &s->irq, 0);
	if (rc) {
		vmm_lerror(edev->node->name,
			   "Failed to get serial IRQ entry in guest DTS.\n");
		goto uart16550a_emulator_probe_freestate_fail;
	}

	if (vmm_devtree_read_u32(edev->node, "reg_shift", &s->reg_shift)) {
		s->reg_shift = 0;
	}
	if (vmm_devtree_read_u32(edev->node, "reg_io_width",
				 &s->reg_io_width)) {
		s->reg_io_width = 1;
	}

	s->recv_fifo = fifo_alloc(1, FIFO_LEN);
	if (!s->recv_fifo) {
		vmm_lerror(edev->node->name,
			   "Failed to allocate uart receive fifo.\n");
		rc = VMM_EFAIL;
		goto uart16550a_emulator_probe_freestate_fail;
	}

	s->xmit_fifo = fifo_alloc(1, FIFO_LEN);
	if (!s->xmit_fifo) {
		vmm_lerror(edev->node->name,
			   "Failed to allocate uart transmit fifo.\n");
		rc = VMM_EFAIL;
		goto uart16550a_emulator_probe_freestate_fail;
	}

	strlcpy(name, guest->name, sizeof(name));
	strlcat(name, "/", sizeof(name));
	if (strlcat(name, edev->node->name, sizeof(name)) >= sizeof(name)) {
		rc = VMM_EOVERFLOW;
		goto uart16550a_emulator_probe_freerbuf_fail;
	}
	s->vser = vmm_vserial_create(name,
				     &ns16550_can_send,
				     &ns16550_send,
				     2048, s);
	if (!(s->vser)) {
		vmm_lerror(edev->node->name,
			   "Failed to create vserial instance.\n");
		goto uart16550a_emulator_probe_freerbuf_fail;
	}

	edev->priv = s;

	goto uart16550a_emulator_probe_done;

uart16550a_emulator_probe_freerbuf_fail:
	if (s->recv_fifo)
		fifo_free(s->recv_fifo);
	if (s->xmit_fifo)
		fifo_free(s->xmit_fifo);
uart16550a_emulator_probe_freestate_fail:
	vmm_free(s);
uart16550a_emulator_probe_done:
	return rc;
}

static int ns16550_emulator_remove(struct vmm_emudev *edev)
{
	struct ns16550_state *s = edev->priv;

	if (s) {
		vmm_vserial_destroy(s->vser);
		fifo_free(s->recv_fifo);
		fifo_free(s->xmit_fifo);
		vmm_free(s);
		edev->priv = NULL;
	}

	return VMM_OK;
}

static struct vmm_devtree_nodeid ns16550_emuid_table[] = {
	{ .type = "serial", .compatible = "ns16550a", },
	{ .type = "serial", .compatible = "ns16550d", },
	{ .type = "serial", .compatible = "8250", },
	{ /* end of list */ },
};

static struct vmm_emulator ns16550_emulator = {
	.name = "ns16550_emulator",
	.match_table = ns16550_emuid_table,
	.endian = VMM_DEVEMU_LITTLE_ENDIAN,
	.probe = ns16550_emulator_probe,
	.read8 = ns16550_emulator_read8,
	.write8 = ns16550_emulator_write8,
	.read16 = ns16550_emulator_read16,
	.write16 = ns16550_emulator_write16,
	.read32 = ns16550_emulator_read32,
	.write32 = ns16550_emulator_write32,
	.reset = ns16550_emulator_reset,
	.remove = ns16550_emulator_remove,
};

static int __init ns16550_emulator_init(void)
{
	return vmm_devemu_register_emulator(&ns16550_emulator);
}

static void __exit ns16550_emulator_exit(void)
{
	vmm_devemu_unregister_emulator(&ns16550_emulator);
}

VMM_DECLARE_MODULE(MODULE_DESC,
		   MODULE_AUTHOR,
		   MODULE_LICENSE,
		   MODULE_IPRIORITY,
		   MODULE_INIT,
		   MODULE_EXIT);
