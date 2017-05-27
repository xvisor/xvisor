/**
 * Copyright (c) 2016 Bhargav Shah.
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
 * @file zynq-uart.h
 * @author Bhargav Shah (bhargavshah1988@gmail.com)
 * @brief Header file for zynq serial driver.
 *
 * Adapted from <u-boot>/drivers/serial/serial_zynq.c
 *
 * Copyright (C) 2012 Michal Simek <monstr@monstr.eu>
 */
#ifndef __ZYNQ_UART_H__
#define __ZYNQ_UART_H__

#include <vmm_types.h>

#define ZYNQ_UART_SR_TXEMPTY	(1 << 3)   /* TX FIFO empty */
#define ZYNQ_UART_SR_TXACTIVE	(1 << 11)  /* TX active */

#define ZYNQ_UART_SR_RXEMPTY	0x00000002 /* RX FIFO empty */
#define ZYNQ_UART_CR_TX_EN	0x00000010 /* TX enabled */
#define ZYNQ_UART_CR_RX_EN	0x00000004 /* RX enabled */
#define ZYNQ_UART_CR_TXRST	0x00000002 /* TX logic reset */
#define ZYNQ_UART_CR_RXRST	0x00000001 /* RX logic reset */
#define ZYNQ_UART_ISR_RX	0x00000001 /* Rx ISR Triggred */
#define ZYNQ_UART_ISR_RX_TOUT	0x00000100 /* Rx Timeout ISR Triggred */
#define ZYNQ_UART_RX_ISR_EN	0x00000001 /* Enable RX interrupt */
#define ZYNQ_UART_RX_ISR_TO_EN	0x00000100 /* Enable RX Timeout interrupt */

#define ZYNQ_UART_MR_PARITY_NONE	0x00000020  /* No parity mode */

struct uart_zynq {
	u32 control; /* 0x0 - Control Register [8:0] */
	u32 mode; /* 0x4 - Mode Register [10:0] */
	u32 ie;	 /* Interrupt Enable Register */
	u32 id;	 /* Interrupt Disable Register */
	u32 im;	 /* Interrupt Mask register */
	u32 isr; /* Interrupt Status register */
	u32 baud_rate_gen; /* 0x18 - Baud Rate Generator [15:0] */
	u32 rx_tout; /* 0x1C - Rx fifo timeout delay */
	u32 rxtrig; /* 0x20 - Rx fifo trigger level */
	u32 reserved2[2];
	u32 channel_sts; /* 0x2c - Channel Status [11:0] */
	u32 tx_rx_fifo; /* 0x30 - FIFO [15:0] or [7:0] */
	u32 baud_rate_divider; /* 0x34 - Baud Rate Divider [7:0] */
};

struct zynq_uart_priv {
	struct serial *p;
	struct uart_zynq *regs;
	u32 baudrate;
	u32 input_clock;
	u32 irq;
	u16 mask;
};

bool zynq_uart_lowlevel_can_getc(struct uart_zynq *regs);
u8 zynq_uart_lowlevel_getc(struct uart_zynq *regs);
bool zynq_uart_lowlevel_can_putc(struct uart_zynq *reg);
void zynq_uart_lowlevel_putc(struct uart_zynq *reg, u8 ch);
void zynq_uart_lowlevel_init(struct zynq_uart_priv *port);

#endif /* __ZYNQ_UART_ */
