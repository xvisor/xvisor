/**
 * Copyright (c) 2016 Anup Patel.
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
 * @file scif.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Header file for SuperH SCIF serial port driver.
 */

#ifndef __SCIF_H_
#define __SCIF_H_

#include <vmm_types.h>

enum {
	SCIx_PROBE_REGTYPE,

	SCIx_SCI_REGTYPE,
	SCIx_IRDA_REGTYPE,
	SCIx_SCIFA_REGTYPE,
	SCIx_SCIFB_REGTYPE,
	SCIx_SH2_SCIF_FIFODATA_REGTYPE,
	SCIx_SH3_SCIF_REGTYPE,
	SCIx_SH4_SCIF_REGTYPE,
	SCIx_SH4_SCIF_BRG_REGTYPE,
	SCIx_SH4_SCIF_NO_SCSPTR_REGTYPE,
	SCIx_SH4_SCIF_FIFODATA_REGTYPE,
	SCIx_SH7705_SCIF_REGTYPE,
	SCIx_HSCIF_REGTYPE,

	SCIx_NR_REGTYPES,
};

/*
 * SCI register subset common for all port types.
 * Not all registers will exist on all parts.
 */
enum {
	SCSMR,                          /* Serial Mode Register */
	SCBRR,                          /* Bit Rate Register */
	SCSCR,                          /* Serial Control Register */
	SCxSR,                          /* Serial Status Register */
	SCFCR,                          /* FIFO Control Register */
	SCFDR,                          /* FIFO Data Count Register */
	SCxTDR,                         /* Transmit (FIFO) Data Register */
	SCxRDR,                         /* Receive (FIFO) Data Register */
	SCLSR,                          /* Line Status Register */
	SCTFDR,                         /* Transmit FIFO Data Count Register */
	SCRFDR,                         /* Receive FIFO Data Count Register */
	SCSPTR,                         /* Serial Port Register */
	HSSRR,                          /* Sampling Rate Register */
	SCPCR,                          /* Serial Port Control Register */
	SCPDR,                          /* Serial Port Data Register */
	SCDL,                           /* BRG Frequency Division Register */
	SCCKS,                          /* BRG Clock Select Register */

	SCIx_NR_REGS,
};

enum SCI_CLKS {
        SCI_FCK,                /* Functional Clock */
        SCI_SCK,                /* Optional External Clock */
        SCI_BRG_INT,            /* Optional BRG Internal Clock Source */
        SCI_SCIF_CLK,           /* Optional BRG External Clock Source */
        SCI_NUM_CLKS
};

#define SCIF_FIFO_MAX_SIZE    16

/* Serial Control Register (SCSCR) */
#define SCSCR_TIE     (1 << 7)    /* Transmit Interrupt Enable */
#define SCSCR_RIE     (1 << 6)    /* Receive Interrupt Enable */
#define SCSCR_TE      (1 << 5)    /* Transmit Enable */
#define SCSCR_RE      (1 << 4)    /* Receive Enable */
#define SCSCR_REIE    (1 << 3)    /* Receive Error Interrupt Enable */
#define SCSCR_TOIE    (1 << 2)    /* Timeout Interrupt Enable */
#define SCSCR_CKE1    (1 << 1)    /* Clock Enable 1 */
#define SCSCR_CKE0    (1 << 0)    /* Clock Enable 0 */

#define SCSCR_CKE00    (0)
#define SCSCR_CKE01    (SCSCR_CKE0)
#define SCSCR_CKE10    (SCSCR_CKE1)
#define SCSCR_CKE11    (SCSCR_CKE1 | SCSCR_CKE0)

/* Serial Mode Register (SCSMR) */
#define SCSMR_CHR     (1 << 6)    /* 7-bit Character Length */
#define SCSMR_PE      (1 << 5)    /* Parity Enable */
#define SCSMR_ODD     (1 << 4)    /* Odd Parity */
#define SCSMR_STOP    (1 << 3)    /* Stop Bit Length */

/* Serial Status Register (SCFSR) */
#define SCFSR_ER      (1 << 7)    /* Receive Error */
#define SCFSR_TEND    (1 << 6)    /* Transmission End */
#define SCFSR_TDFE    (1 << 5)    /* Transmit FIFO Data Empty */
#define SCFSR_BRK     (1 << 4)    /* Break Detect */
#define SCFSR_FER     (1 << 3)    /* Framing Error */
#define SCFSR_PER     (1 << 2)    /* Parity Error */
#define SCFSR_RDF     (1 << 1)    /* Receive FIFO Data Full */
#define SCFSR_DR      (1 << 0)    /* Receive Data Ready */

#define SCIF_ERRORS    (SCFSR_PER | SCFSR_FER | SCFSR_ER | SCFSR_BRK)

/* Line Status Register (SCLSR) */
#define SCLSR_TO      (1 << 2)    /* Timeout */
#define SCLSR_ORER    (1 << 0)    /* Overrun Error */

/* FIFO Control Register (SCFCR) */
#define SCFCR_RTRG1    (1 << 7)    /* Receive FIFO Data Count Trigger 1 */
#define SCFCR_RTRG0    (1 << 6)    /* Receive FIFO Data Count Trigger 0 */
#define SCFCR_TTRG1    (1 << 5)    /* Transmit FIFO Data Count Trigger 1 */
#define SCFCR_TTRG0    (1 << 4)    /* Transmit FIFO Data Count Trigger 0 */
#define SCFCR_MCE      (1 << 3)    /* Modem Control Enable */
#define SCFCR_TFRST    (1 << 2)    /* Transmit FIFO Data Register Reset */
#define SCFCR_RFRST    (1 << 1)    /* Receive FIFO Data Register Reset */
#define SCFCR_LOOP     (1 << 0)    /* Loopback Test */

#define SCFCR_RTRG00    (0)
#define SCFCR_RTRG01    (SCFCR_RTRG0)
#define SCFCR_RTRG10    (SCFCR_RTRG1)
#define SCFCR_RTRG11    (SCFCR_RTRG1 | SCFCR_RTRG0)

#define SCFCR_TTRG00    (0)
#define SCFCR_TTRG01    (SCFCR_TTRG0)
#define SCFCR_TTRG10    (SCFCR_TTRG1)
#define SCFCR_TTRG11    (SCFCR_TTRG1 | SCFCR_TTRG0)

bool scif_lowlevel_can_getc(virtual_addr_t base, unsigned long regtype);
u8 scif_lowlevel_getc(virtual_addr_t base, unsigned long regtype);
bool scif_lowlevel_can_putc(virtual_addr_t base, unsigned long regtype);
void scif_lowlevel_putc(virtual_addr_t base, unsigned long regtype, u8 ch);
void scif_lowlevel_init(virtual_addr_t base, unsigned long regtype,
			u32 baudrate, u32 input_clock, bool use_internal_clock);

#endif /* __SCIF_H_ */
