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

#define SCIF_FIFO_MAX_SIZE    16

/* Register offsets */
#define SCIF_SCSMR     (0x00)    /* Serial mode register           */
#define SCIF_SCBRR     (0x04)    /* Bit rate register              */
#define SCIF_SCSCR     (0x08)    /* Serial control register        */
#define SCIF_SCFTDR    (0x0C)    /* Transmit FIFO data register    */
#define SCIF_SCFSR     (0x10)    /* Serial status register         */
#define SCIF_SCFRDR    (0x14)    /* Receive FIFO data register     */
#define SCIF_SCFCR     (0x18)    /* FIFO control register          */
#define SCIF_SCFDR     (0x1C)    /* FIFO data count register       */
#define SCIF_SCSPTR    (0x20)    /* Serial port register           */
#define SCIF_SCLSR     (0x24)    /* Line status register           */
#define SCIF_DL        (0x30)    /* Frequency division register    */
#define SCIF_CKS       (0x34)    /* Clock Select register          */

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

bool scif_lowlevel_can_getc(virtual_addr_t base);
u8 scif_lowlevel_getc(virtual_addr_t base);
bool scif_lowlevel_can_putc(virtual_addr_t base);
void scif_lowlevel_putc(virtual_addr_t base, u8 ch);
void scif_lowlevel_init(virtual_addr_t base, u32 baudrate,
			u32 input_clock, bool use_internal_clock);

#endif /* __SCIF_H_ */
