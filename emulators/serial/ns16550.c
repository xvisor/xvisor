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
 * @file 16550a.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief 16550A UART Emulator
 *
 * The source has been largely adapted from QEMU 1.7.50 hw/char/serial.c
 * 
 * QEMU 16550A UART emulation
 *
 * Copyright (c) 2003-2004 Fabrice Bellard
 * Copyright (c) 2008 Citrix Systems, Inc.
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_devtree.h>
#include <vmm_devemu.h>
#include <vmm_timer.h>
#include <vio/vmm_vserial.h>
#include <libs/fifo.h>
#include <libs/stringlib.h>

#define MODULE_DESC			"NS16550 Class UART Emulator"
#define MODULE_AUTHOR			"Himanshu Chauhan"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(VMM_VSERIAL_IPRIORITY+1)
#define	MODULE_INIT			ns16550_emulator_init
#define	MODULE_EXIT			ns16550_emulator_exit

//#define DEBUG_SERIAL

#define UART_LCR_DLAB	0x80	/* Divisor latch access bit */

#define UART_IER_MSI	0x08	/* Enable Modem status interrupt */
#define UART_IER_RLSI	0x04	/* Enable receiver line status interrupt */
#define UART_IER_THRI	0x02	/* Enable Transmitter holding register int. */
#define UART_IER_RDI	0x01	/* Enable receiver data interrupt */

#define UART_IIR_NO_INT	0x01	/* No interrupts pending */
#define UART_IIR_ID	0x06	/* Mask for the interrupt ID */

#define UART_IIR_MSI	0x00	/* Modem status interrupt */
#define UART_IIR_THRI	0x02	/* Transmitter holding register empty */
#define UART_IIR_RDI	0x04	/* Receiver data interrupt */
#define UART_IIR_RLSI	0x06	/* Receiver line status interrupt */
#define UART_IIR_CTI    0x0C    /* Character Timeout Indication */

#define UART_IIR_FENF   0x80    /* Fifo enabled, but not functionning */
#define UART_IIR_FE     0xC0    /* Fifo enabled */

/*
 * These are the definitions for the Modem Control Register
 */
#define UART_MCR_LOOP	0x10	/* Enable loopback test mode */
#define UART_MCR_OUT2	0x08	/* Out2 complement */
#define UART_MCR_OUT1	0x04	/* Out1 complement */
#define UART_MCR_RTS	0x02	/* RTS complement */
#define UART_MCR_DTR	0x01	/* DTR complement */

/*
 * These are the definitions for the Modem Status Register
 */
#define UART_MSR_DCD	0x80	/* Data Carrier Detect */
#define UART_MSR_RI	0x40	/* Ring Indicator */
#define UART_MSR_DSR	0x20	/* Data Set Ready */
#define UART_MSR_CTS	0x10	/* Clear to Send */
#define UART_MSR_DDCD	0x08	/* Delta DCD */
#define UART_MSR_TERI	0x04	/* Trailing edge ring indicator */
#define UART_MSR_DDSR	0x02	/* Delta DSR */
#define UART_MSR_DCTS	0x01	/* Delta CTS */
#define UART_MSR_ANY_DELTA 0x0F	/* Any of the delta bits! */

#define UART_LSR_TEMT	0x40	/* Transmitter empty */
#define UART_LSR_THRE	0x20	/* Transmit-hold-register empty */
#define UART_LSR_BI	0x10	/* Break interrupt indicator */
#define UART_LSR_FE	0x08	/* Frame error indicator */
#define UART_LSR_PE	0x04	/* Parity error indicator */
#define UART_LSR_OE	0x02	/* Overrun error indicator */
#define UART_LSR_DR	0x01	/* Receiver data ready */
#define UART_LSR_INT_ANY 0x1E	/* Any of the lsr-interrupt-triggering status bits */

/* Interrupt trigger levels. The byte-counts are for 16550A - in newer UARTs the byte-count for each ITL is higher. */

#define UART_FCR_ITL_1      0x00 /* 1 byte ITL */
#define UART_FCR_ITL_2      0x40 /* 4 bytes ITL */
#define UART_FCR_ITL_3      0x80 /* 8 bytes ITL */
#define UART_FCR_ITL_4      0xC0 /* 14 bytes ITL */

#define UART_FCR_DMS        0x08    /* DMA Mode Select */
#define UART_FCR_XFR        0x04    /* XMIT Fifo Reset */
#define UART_FCR_RFR        0x02    /* RCVR Fifo Reset */
#define UART_FCR_FE         0x01    /* FIFO Enable */

#define MAX_XMIT_RETRY      4

enum {
	SERIAL_LOG_LVL_ERR,
	SERIAL_LOG_LVL_INFO,
	SERIAL_LOG_LVL_DEBUG,
	SERIAL_LOG_LVL_VERBOSE
};

static int serial_default_log_lvl = SERIAL_LOG_LVL_INFO;

#define SERIAL_LOG(lvl, fmt, args...)					\
	do {								\
		if (SERIAL_LOG_##lvl <= serial_default_log_lvl) {	\
			vmm_printf("(%s:%d) " fmt, __func__,		\
				   __LINE__, ##args);			\
		}							\
	}while(0);

struct serial_set_params{
	int speed;
	int parity;
	int data_bits;
	int stop_bits;
};

struct ns16550_state {
	struct vmm_guest *guest;
	struct vmm_vserial *vser;
	vmm_spinlock_t lock;

	u16 divider;
	u8 rbr; /* receive register */
	u8 thr; /* transmit holding register */
	u8 tsr; /* transmit shift register */
	u8 ier;
	u8 iir; /* read only */
	u8 lcr;
	u8 mcr;
	u8 lsr; /* read only */
	u8 msr; /* read only */
	u8 scr;
	u8 fcr;
	u8 fcr_vmstate; /* we can't write directly this value
				it has side effects */
	/* NOTE: this hidden state is necessary for tx irq generation as
	   it can be reset while reading iir */
	int thr_ipending;
	u32 irq;
	int last_break_enable;
	int it_shift;
	int baudbase;
	int tsr_retry;
	u32 wakeup;

	/* Time when the last byte was successfully sent out of the tsr */
	u64 last_xmit_ts;
	struct fifo *recv_fifo;
	struct fifo *xmit_fifo;

	u32 fifo_sz;

	/* Interrupt trigger level for recv_fifo */
	u8 recv_fifo_itl;

	struct vmm_timer_event fifo_timeout_timer;
	int timeout_ipending;           /* timeout interrupt pending state */

	u64 char_transmit_time;    /* time to transmit a char in ticks */
	int poll_msl;

	struct vmm_timer_event modem_status_poll;
};

static int ns16550_send(struct vmm_vserial *vser, u8 data);

static void ns16550_irq_raise(struct ns16550_state *s)
{
	vmm_devemu_emulate_irq(s->guest, s->irq, 1);
}

static void ns16550_irq_lower(struct ns16550_state *s)
{
	vmm_devemu_emulate_irq(s->guest, s->irq, 0);
}

static void ns16550_update_irq(struct ns16550_state *s)
{
	u8 tmp_iir = UART_IIR_NO_INT;

	if ((s->ier & UART_IER_RLSI) && (s->lsr & UART_LSR_INT_ANY)) {
		tmp_iir = UART_IIR_RLSI;
	} else if ((s->ier & UART_IER_RDI) && s->timeout_ipending) {
		/* Note that(s->ier & UART_IER_RDI) can mask this interrupt,
		 * this is not in the specification but is observed on existing
		 * hardware.  */
		tmp_iir = UART_IIR_CTI;
	} else if ((s->ier & UART_IER_RDI) && (s->lsr & UART_LSR_DR) &&
		   (!(s->fcr & UART_FCR_FE) ||
		    s->recv_fifo->avail_count >= s->recv_fifo_itl)) {
		tmp_iir = UART_IIR_RDI;
	} else if ((s->ier & UART_IER_THRI) && s->thr_ipending) {
		tmp_iir = UART_IIR_THRI;
	} else if ((s->ier & UART_IER_MSI) && (s->msr & UART_MSR_ANY_DELTA)) {
		tmp_iir = UART_IIR_MSI;
	}

	s->iir = tmp_iir | (s->iir & 0xF0);

	if (tmp_iir != UART_IIR_NO_INT) {
		ns16550_irq_raise(s);
	} else {
		ns16550_irq_lower(s);
	}
}

static void ns16550_update_parameters(struct ns16550_state *s)
{
	int speed, data_bits, stop_bits, frame_size;
#if 0
	int parity;
	struct serial_set_param ssp;
#endif

	if (s->divider == 0)
		return;

	/* Start bit. */
	frame_size = 1;
	if (s->lcr & 0x08) {
		/* Parity bit. */
		frame_size++;
#if 0
		if (s->lcr & 0x10)
			parity = 'E';
		else
			parity = 'O';
#endif
	} else {
#if 0
		parity = 'N';
#endif
	}
	if (s->lcr & 0x04)
		stop_bits = 2;
	else
		stop_bits = 1;

	data_bits = (s->lcr & 0x03) + 5;
	frame_size += data_bits + stop_bits;
	speed = s->baudbase / s->divider;
#if 0
	ssp.speed = speed;
	ssp.parity = parity;
	ssp.data_bits = data_bits;
	ssp.stop_bits = stop_bits;
#endif
	s->char_transmit_time =  (1000000000ULL / speed) * frame_size;

	/* TODO: If backed by a real serial port, set the param as below */
#if 0
	qemu_chr_fe_ioctl(s->chr, CHR_IOCTL_SERIAL_SET_PARAMS, &ssp);
#endif
}

static void ns16550_update_msl(struct vmm_timer_event *event)
{
	u8 omsr;
#if 0
	int flags;
#endif
	struct ns16550_state *s = event->priv;

	vmm_timer_event_stop(&s->modem_status_poll);

#if 0
	if (qemu_chr_fe_ioctl(s->chr,CHR_IOCTL_SERIAL_GET_TIOCM, &flags) == -ENOTSUP) {
		s->poll_msl = -1;
		return;
	}
#endif
	omsr = s->msr;

#if 0
	s->msr = (flags & CHR_TIOCM_CTS) ? s->msr | UART_MSR_CTS : s->msr & ~UART_MSR_CTS;
	s->msr = (flags & CHR_TIOCM_DSR) ? s->msr | UART_MSR_DSR : s->msr & ~UART_MSR_DSR;
	s->msr = (flags & CHR_TIOCM_CAR) ? s->msr | UART_MSR_DCD : s->msr & ~UART_MSR_DCD;
	s->msr = (flags & CHR_TIOCM_RI) ? s->msr | UART_MSR_RI : s->msr & ~UART_MSR_RI;
#endif

	if (s->msr != omsr) {
		/* Set delta bits */
		s->msr = s->msr | ((s->msr >> 4) ^ (omsr >> 4));
		/* UART_MSR_TERI only if change was from 1 -> 0 */
		if ((s->msr & UART_MSR_TERI) && !(omsr & UART_MSR_RI))
			s->msr &= ~UART_MSR_TERI;
		ns16550_update_irq(s);
	}

	/* The real 16550A apparently has a 250ns response latency to line status changes.
	   We'll be lazy and poll only every 10ms, and only poll it at all if MSI interrupts are turned on */

	if (s->poll_msl) {
		vmm_timer_event_stop(&s->modem_status_poll);
		vmm_timer_event_start(&s->modem_status_poll, 1000000000ULL / 100);
	}
}

static bool ns16550_xmit(struct ns16550_state *s)
{
	if (s->tsr_retry <= 0) {
		if (s->fcr & UART_FCR_FE) {
			s->tsr = 0;
			if (!fifo_isfull(s->xmit_fifo))
				fifo_dequeue(s->xmit_fifo, &s->tsr);
			if (!s->xmit_fifo->avail_count) {
				s->lsr |= UART_LSR_THRE;
			}
		} else if ((s->lsr & UART_LSR_THRE)) {
			return FALSE;
		} else {
			s->tsr = s->thr;
			s->lsr |= UART_LSR_THRE;
			s->lsr &= ~UART_LSR_TEMT;
		}
	}

	if (s->mcr & UART_MCR_LOOP) {
		/* in loopback mode, say that we just received a char */
		ns16550_send(s->vser, s->tsr);
	} else {
		/* vmm_vserial_receive: guest => console (guest output)
		 * vmm_vserial_send: console => guest (guest input)
		 */
		/* assuming vmm_vserial_receive will never fail */
		vmm_vserial_receive(s->vser, &s->tsr, 1);
		s->tsr_retry = 0;
	}

	s->last_xmit_ts = vmm_timer_timestamp();

	if (s->lsr & UART_LSR_THRE) {
		s->lsr |= UART_LSR_TEMT;
		s->thr_ipending = 1;
		ns16550_update_irq(s);
	}

	return FALSE;
}

static int ns16550_reg_write(struct ns16550_state *s, u32 addr, 
			     u32 src_mask, u32 val)
{
	u8 temp;

	addr &= 7;

	SERIAL_LOG(LVL_DEBUG, "Reg: 0x%x Value: 0x%x\n", addr, val);

	switch(addr) {
	default:
	case 0:
		if (s->lcr & UART_LCR_DLAB) {
			s->divider = (s->divider & 0xff00) | val;
			ns16550_update_parameters(s);
		} else {
			s->thr = (u8) val;
			if(s->fcr & UART_FCR_FE) {
				/* xmit overruns overwrite data, so make space if needed */
				if (fifo_isfull(s->xmit_fifo)) {
					fifo_dequeue(s->xmit_fifo, &temp);
				}
				fifo_enqueue(s->xmit_fifo, &s->thr, 1);
				s->lsr &= ~UART_LSR_TEMT;
			}
			s->thr_ipending = 0;
			s->lsr &= ~UART_LSR_THRE;
			ns16550_update_irq(s);
			ns16550_xmit(s);
		}
		break;
	case 1:
		if (s->lcr & UART_LCR_DLAB) {
			s->divider = (s->divider & 0x00ff) | (val << 8);
			ns16550_update_parameters(s);
		} else {
			s->ier = val & 0x0f;
			/* If the backend device is a real serial port, turn polling of the modem
			   status lines on physical port on or off depending on UART_IER_MSI state */
			if (s->poll_msl >= 0) {
				if (s->ier & UART_IER_MSI) {
					s->poll_msl = 1;
					ns16550_update_msl(&s->modem_status_poll);
				} else {
					vmm_timer_event_stop(&s->modem_status_poll);
					s->poll_msl = 0;
				}
			}
			if (s->lsr & UART_LSR_THRE) {
				s->thr_ipending = 1;
				ns16550_update_irq(s);
			}
		}
		break;
	case 2:
		val = val & 0xFF;

		if (s->fcr == val)
			break;

		/* Did the enable/disable flag change? If so, make sure FIFOs get flushed */
		if ((val ^ s->fcr) & UART_FCR_FE)
			val |= UART_FCR_XFR | UART_FCR_RFR;

		/* FIFO clear */

		if (val & UART_FCR_RFR) {
			vmm_timer_event_stop(&s->fifo_timeout_timer);
			s->timeout_ipending=0;
			fifo_clear(s->recv_fifo);
		}

		if (val & UART_FCR_XFR) {
			fifo_clear(s->xmit_fifo);
		}

		if (val & UART_FCR_FE) {
			s->iir |= UART_IIR_FE;
			/* Set recv_fifo trigger Level */
			switch (val & 0xC0) {
			case UART_FCR_ITL_1:
				s->recv_fifo_itl = 1;
				break;
			case UART_FCR_ITL_2:
				s->recv_fifo_itl = 4;
				break;
			case UART_FCR_ITL_3:
				s->recv_fifo_itl = 8;
				break;
			case UART_FCR_ITL_4:
				s->recv_fifo_itl = 14;
				break;
			}
		} else
			s->iir &= ~UART_IIR_FE;

		/* Set fcr - or at least the bits in it that are supposed to "stick" */
		s->fcr = val & 0xC9;
		ns16550_update_irq(s);
		break;
	case 3:
		{
			int break_enable;
			s->lcr = val;
			ns16550_update_parameters(s);
			break_enable = (val >> 6) & 1;
			if (break_enable != s->last_break_enable) {
				s->last_break_enable = break_enable;
#if 0 /* When hardware backed, enable break on it */
				qemu_chr_fe_ioctl(s->chr, CHR_IOCTL_SERIAL_SET_BREAK,
						  &break_enable);
#endif
			}
		}
		break;
	case 4:
		{
#if 0
			int flags;
#endif
			int old_mcr = s->mcr;
			s->mcr = val & 0x1f;
			if (val & UART_MCR_LOOP)
				break;

			if (s->poll_msl >= 0 && old_mcr != s->mcr) {
#if 0
				qemu_chr_fe_ioctl(s->chr,CHR_IOCTL_SERIAL_GET_TIOCM, &flags);

				flags &= ~(CHR_TIOCM_RTS | CHR_TIOCM_DTR);

				if (val & UART_MCR_RTS)
					flags |= CHR_TIOCM_RTS;
				if (val & UART_MCR_DTR)
					flags |= CHR_TIOCM_DTR;

				qemu_chr_fe_ioctl(s->chr,CHR_IOCTL_SERIAL_SET_TIOCM, &flags);
#endif
				/* Update the modem status after a one-character-send wait-time, since there may be a response
				   from the device/computer at the other end of the serial line */
				vmm_timer_event_stop(&s->modem_status_poll);
				vmm_timer_event_start(&s->modem_status_poll, s->char_transmit_time);
			}
		}
		break;
	case 5:
		break;
	case 6:
		break;
	case 7:
		s->scr = val;
		break;
	}

	return VMM_OK;
}

static int ns16550_reg_read(struct ns16550_state *s, u32 addr, u32 *dst)
{
	u32 ret;

	addr &= 7;
	switch(addr) {
	default:
	case 0:
		if (s->lcr & UART_LCR_DLAB) {
			ret = s->divider & 0xff;
		} else {
			if(s->fcr & UART_FCR_FE) {
				ret = 0;
				if (!fifo_isempty(s->recv_fifo))
					fifo_dequeue(s->recv_fifo, &ret);
				if (s->recv_fifo->avail_count == 0) {
					s->lsr &= ~(UART_LSR_DR | UART_LSR_BI);
				} else {
					vmm_timer_event_stop(&s->fifo_timeout_timer);
					vmm_timer_event_start(&s->fifo_timeout_timer, s->char_transmit_time * 4);
				}
				s->timeout_ipending = 0;
			} else {
				ret = s->rbr;
				s->lsr &= ~(UART_LSR_DR | UART_LSR_BI);
			}
			ns16550_update_irq(s);
#if 0 /* TODO: ??? */
			if (!(s->mcr & UART_MCR_LOOP)) {
				/* in loopback mode, don't receive any data */
				qemu_chr_accept_input(s->chr);
			}
#endif
		}
		break;
	case 1:
		if (s->lcr & UART_LCR_DLAB) {
			ret = (s->divider >> 8) & 0xff;
		} else {
			ret = s->ier;
		}
		break;
	case 2:
		ret = s->iir;
		if ((ret & UART_IIR_ID) == UART_IIR_THRI) {
			s->thr_ipending = 0;
			ns16550_update_irq(s);
		}
		break;
	case 3:
		ret = s->lcr;
		break;
	case 4:
		ret = s->mcr;
		break;
	case 5:
		ret = s->lsr;
		/* Clear break and overrun interrupts */
		if (s->lsr & (UART_LSR_BI|UART_LSR_OE)) {
			s->lsr &= ~(UART_LSR_BI|UART_LSR_OE);
			ns16550_update_irq(s);
		}
		break;
	case 6:
		if (s->mcr & UART_MCR_LOOP) {
			/* in loopback, the modem output pins are connected to the
			   inputs */
			ret = (s->mcr & 0x0c) << 4;
			ret |= (s->mcr & 0x02) << 3;
			ret |= (s->mcr & 0x01) << 5;
		} else {
			if (s->poll_msl >= 0)
				ns16550_update_msl(&s->modem_status_poll);
			ret = s->msr;
			/* Clear delta bits & msr int after read, if they were set */
			if (s->msr & UART_MSR_ANY_DELTA) {
				s->msr &= 0xF0;
				ns16550_update_irq(s);
			}
		}
		break;
	case 7:
		ret = s->scr;
		break;
	}

	*dst = ret;

	return VMM_OK;
}

static bool ns16550_can_send(struct vmm_vserial *vser)
{
	struct ns16550_state *s = vmm_vserial_priv(vser);

	if(s->fcr & UART_FCR_FE) {
		if (s->recv_fifo->avail_count < s->fifo_sz) {
			/*
			 * Advertise (fifo.itl - fifo.count) bytes when count < ITL, and 1
			 * if above. If UART_FIFO_LENGTH - fifo.count is advertised the
			 * effect will be to almost always fill the fifo completely before
			 * the guest has a chance to respond, effectively overriding the ITL
			 * that the guest has set.
			 */
			return (s->recv_fifo->avail_count <= s->recv_fifo_itl) ?
				s->recv_fifo_itl - s->recv_fifo->avail_count : 1;
		} else {
			return 0;
		}
	} else {
		return !(s->lsr & UART_LSR_DR);
	}

	return 0;
}

#if 0
static void ns16550_receive_break(struct ns16550_state *s)
{
	s->rbr = 0;
	/* When the LSR_DR is set a null byte is pushed into the fifo */
	recv_fifo_put(s, '\0');
	s->lsr |= UART_LSR_BI | UART_LSR_DR;
	serial_update_irq(s);
}
#endif

/* There's data in recv_fifo and s->rbr has not been read for 4 char transmit times */
static void ns16550_fifo_timeout_int(struct vmm_timer_event *event)
{
	struct ns16550_state *s = event->priv;
	if (s->recv_fifo->avail_count) {
		s->timeout_ipending = 1;
		ns16550_update_irq(s);
	}
}

static inline void ns16550_recv_fifo_put(struct ns16550_state *s, u8 chr)
{
	/* Receive overruns do not overwrite FIFO contents. */
	if (!fifo_isfull(s->recv_fifo)) {
		fifo_enqueue(s->recv_fifo, &chr, TRUE);
	} else {
		s->lsr |= UART_LSR_OE;
	}
}

static int ns16550_send(struct vmm_vserial *vser, u8 data)
{
	struct ns16550_state *s = vmm_vserial_priv(vser);

	if(s->fcr & UART_FCR_FE) {
		ns16550_recv_fifo_put(s, data);
		s->lsr |= UART_LSR_DR;

		/* call the timeout receive callback in 4 char transmit time */
		vmm_timer_event_stop(&s->fifo_timeout_timer); 
		vmm_timer_event_start(&s->fifo_timeout_timer, (s->char_transmit_time * 4));
	} else {
		if (s->lsr & UART_LSR_DR)
			s->lsr |= UART_LSR_OE;
		s->rbr = data;
		s->lsr |= UART_LSR_DR;
	}
	ns16550_update_irq(s);

	return VMM_OK;
}

#if 0
static void ns16550_event(void *opaque, int event)
{
	struct ns16550_state *s = opaque;
	if (event == CHR_EVENT_BREAK)
		serial_receive_break(s);
}
#endif

static int ns16550_emulator_reset(struct vmm_emudev *edev)
{
	struct ns16550_state *s = edev->priv;

	vmm_spin_lock(&s->lock);

	s->rbr = 0;
	s->ier = 0;
	s->iir = UART_IIR_NO_INT;
	s->lcr = 0;
	s->lsr = UART_LSR_TEMT | UART_LSR_THRE;
	s->msr = UART_MSR_DCD | UART_MSR_DSR | UART_MSR_CTS;
	/* Default to 9600 baud, 1 start bit, 8 data bits, 1 stop bit, no parity. */
	s->divider = 0x0C;
	s->mcr = UART_MCR_OUT2;
	s->scr = 0;
	s->tsr_retry = 0;
	s->char_transmit_time = (1000000000ULL / 9600) * 10;
	s->poll_msl = 0;

	fifo_clear(s->recv_fifo);
	fifo_clear(s->xmit_fifo);

	s->last_xmit_ts = vmm_timer_timestamp();

	s->thr_ipending = 0;
	s->last_break_enable = 0;
	ns16550_irq_lower(s);

	vmm_spin_unlock(&s->lock);

	return VMM_OK;
}

/* Change the main reference oscillator frequency. */
void ns16550_set_frequency(struct ns16550_state *s, u32 frequency)
{
	s->baudbase = frequency;
	ns16550_update_parameters(s);
}

static int ns16550_emulator_read8(struct vmm_emudev *edev,
				  physical_addr_t offset, 
				  u8 *dst)
{
	int rc;
	u32 regval = 0x0;

	rc = ns16550_reg_read(edev->priv, offset, &regval);
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

	rc = ns16550_reg_read(edev->priv, offset, &regval);
	if (!rc) {
		*dst = regval & 0xFFFF;
	}

	return rc;
}

static int ns16550_emulator_read32(struct vmm_emudev *edev,
				   physical_addr_t offset, 
				   u32 *dst)
{
	return ns16550_reg_read(edev->priv, offset, dst);
}

static int ns16550_emulator_write8(struct vmm_emudev *edev,
				   physical_addr_t offset, 
				   u8 src)
{
	return ns16550_reg_write(edev->priv, offset, 0xFFFFFF00, src);
}

static int ns16550_emulator_write16(struct vmm_emudev *edev,
				    physical_addr_t offset, 
				    u16 src)
{
	return ns16550_reg_write(edev->priv, offset, 0xFFFF0000, src);
}

static int ns16550_emulator_write32(struct vmm_emudev *edev,
				    physical_addr_t offset, 
				    u32 src)
{
	return ns16550_reg_write(edev->priv, offset, 0x00000000, src);
}

static int ns16550_emulator_probe(struct vmm_guest *guest,
				  struct vmm_emudev *edev,
				  const struct vmm_devtree_nodeid *eid)
{
	int rc = VMM_OK;
	char name[64];
	const char *attr;
	struct ns16550_state *s;

	SERIAL_LOG(LVL_VERBOSE, "enter\n");

	s = vmm_zalloc(sizeof(struct ns16550_state));
	if (!s) {
		SERIAL_LOG(LVL_ERR, "Failed to allocate serial emulator's state.\n");
		rc = VMM_EFAIL;
		goto uart16550a_emulator_probe_done;
	}

	s->guest = guest;
	INIT_SPIN_LOCK(&s->lock);

	rc = vmm_devtree_irq_get(edev->node, &s->irq, 0);
	if (rc) {
		SERIAL_LOG(LVL_ERR, "Failed to get serial IRQ entry in guest DTS.\n");
		goto uart16550a_emulator_probe_freestate_fail;
	}

	attr = vmm_devtree_attrval(edev->node, "fifo_size");
	if (attr) {
		s->fifo_sz = *((u32 *)attr);
		SERIAL_LOG(LVL_VERBOSE, "Serial FIFO size is %d bytes.\n", s->fifo_sz);
	} else {
		SERIAL_LOG(LVL_ERR, "Failed to get fifo size in guest DTS.\n");
		rc = VMM_EFAIL;
		goto uart16550a_emulator_probe_freestate_fail;
	}

	attr = vmm_devtree_attrval(edev->node, "baudbase");
	if (attr) {
		s->baudbase = *((u32 *)attr);
	} else {
		s->baudbase = 9600;
	}

	SERIAL_LOG(LVL_VERBOSE, "Serial baudrate: %d\n", s->baudbase);

	s->recv_fifo = fifo_alloc(1, s->fifo_sz);
	if (!s->recv_fifo) {
		SERIAL_LOG(LVL_ERR, "Failed to allocate uart receive fifo.\n");
		rc = VMM_EFAIL;
		goto uart16550a_emulator_probe_freestate_fail;
	}

	s->xmit_fifo = fifo_alloc(1, s->fifo_sz);
	if (!s->xmit_fifo) {
		SERIAL_LOG(LVL_ERR, "Failed to allocate uart transmit fifo.\n");
		rc = VMM_EFAIL;
		goto uart16550a_emulator_probe_freestate_fail;
	}

	INIT_TIMER_EVENT(&s->modem_status_poll, ns16550_update_msl, s);
	INIT_TIMER_EVENT(&s->fifo_timeout_timer, ns16550_fifo_timeout_int, s);

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
		SERIAL_LOG(LVL_ERR, "Failed to create vserial instance.\n");
		goto uart16550a_emulator_probe_freerbuf_fail;
	}

	s->it_shift = 2;

	edev->priv = s;

	SERIAL_LOG(LVL_VERBOSE, "Success.\n");

	goto uart16550a_emulator_probe_done;

uart16550a_emulator_probe_freerbuf_fail:
	if (s->recv_fifo) fifo_free(s->recv_fifo);
	if (s->xmit_fifo) fifo_free(s->xmit_fifo);
uart16550a_emulator_probe_freestate_fail:
	vmm_free(s);
uart16550a_emulator_probe_done:
	return rc;
}

static int ns16550_emulator_remove(struct vmm_emudev *edev)
{
	struct ns16550_state *s = edev->priv;

	if (s) {
		vmm_timer_event_stop(&s->modem_status_poll);
		vmm_timer_event_stop(&s->fifo_timeout_timer);
		vmm_vserial_destroy(s->vser);
		fifo_free(s->recv_fifo);
		fifo_free(s->xmit_fifo);
		vmm_free(s);
		edev->priv = NULL;
	}

	return VMM_OK;
}

static struct vmm_devtree_nodeid ns16550_emuid_table[] = {
	{ .type = "serial", 
	  .compatible = "ns16550a", 
	},
	{ .type = "serial", 
	  .compatible = "8250", 
	},
	{ .type = "serial", 
	  .compatible = "ns16550d", 
	},
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
