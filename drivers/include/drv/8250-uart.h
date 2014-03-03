/**
 * Copyright (c) 2010 Anup Patel.
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
 * @file uart.h
 * @author Anup Patel (anup@brainfault.org)
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief Header file for UART serial port driver.
 */

#ifndef __UART_H_
#define __UART_H_

#include <vmm_types.h>
#include <vmm_completion.h>
#include <vmm_chardev.h>

#define UART_RBR_OFFSET		0 /* In:  Recieve Buffer Register */
#define UART_THR_OFFSET		0 /* Out: Transmitter Holding Register */
#define UART_DLL_OFFSET		0 /* Out: Divisor Latch Low */
#define UART_IER_OFFSET		1 /* I/O: Interrupt Enable Register */
#define UART_DLM_OFFSET		1 /* Out: Divisor Latch High */
#define UART_FCR_OFFSET		2 /* Out: FIFO Control Register */
#define UART_IIR_OFFSET		2 /* I/O: Interrupt Identification Register */
#define UART_LCR_OFFSET		3 /* Out: Line Control Register */
#define UART_MCR_OFFSET		4 /* Out: Modem Control Register */
#define UART_LSR_OFFSET		5 /* In:  Line Status Register */
#define UART_MSR_OFFSET		6 /* In:  Modem Status Register */
#define UART_SCR_OFFSET		7 /* I/O: Scratch Register */
#define UART_MDR1_OFFSET	8 /* I/O:  Mode Register */

#define UART_LSR_FIFOE		0x80    /* Fifo error */
#define UART_LSR_TEMT		0x40    /* Transmitter empty */
#define UART_LSR_THRE		0x20    /* Transmit-hold-register empty */
#define UART_LSR_BI		0x10    /* Break interrupt indicator */
#define UART_LSR_FE		0x08    /* Frame error indicator */
#define UART_LSR_PE		0x04    /* Parity error indicator */
#define UART_LSR_OE		0x02    /* Overrun error indicator */
#define UART_LSR_DR		0x01    /* Receiver data ready */
#define UART_LSR_BRK_ERROR_BITS	0x1E    /* BI, FE, PE, OE bits */

#define UART_IIR_MSI		0x00	/* Modem status interrupt */
#define UART_IIR_NOINT		0x01	/* No interrupt */
#define UART_IIR_TYPE		0x1e	/* IT_TYPE field */
#define UART_IIR_THRI		0x02	/* THR Interrupt */
#define UART_IIR_RDI		0x04	/* RHR Interrupt */
#define UART_IIR_RLSI		0x06	/* Receiver Line Status Intr */
#define UART_IIR_RTO		0x0c	/* Receiver timeout interrupt */
#define UART_IIR_BUSY		0x07	/* Designware BUSY interrupt for LCR writes */

#define UART_IER_MSI		0x08    /* Enable Modem status interrupt */
#define UART_IER_RLSI		0x04    /* Enable receiver line status interrupt */
#define UART_IER_THRI		0x02    /* Enable Transmitter holding register int. */
#define UART_IER_RDI		0x01    /* Enable receiver data interrupt */

struct uart_8250_port {
	struct vmm_completion read_possible;
	struct vmm_chardev cd;
	virtual_addr_t base;
	u32 baudrate;
	u32 input_clock;
	u32 reg_shift;
	u32 reg_width;
	u32 irq;
	u32 ier;
	u32 lcr_last;
	char *rxbuf;
	u32 rxhead, rxtail;
	vmm_spinlock_t rxlock;
};

bool uart_8250_lowlevel_can_getc(struct uart_8250_port *port);
u8 uart_8250_lowlevel_getc(struct uart_8250_port *port);
bool uart_8250_lowlevel_can_putc(struct uart_8250_port *port);
void uart_8250_lowlevel_putc(struct uart_8250_port *port, u8 ch);
void uart_8250_lowlevel_init(struct uart_8250_port *port); 

#endif /* __UART_H_ */
