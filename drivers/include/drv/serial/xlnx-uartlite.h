/**
 * Copyright (c) 2024 Xu, Zefan.
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
 * @file xlnx-uartlite.h
 * @author Xu, Zefan (ceba_robot@outlook.com)
 * @brief Header file for xlinx uartlite driver.
 */
#ifndef __XLNX_UARTLITE_H__
#define __XLNX_UARTLITE_H__

#include <vmm_types.h>

#define UARTLITE_STAT_INTR_ENABLED	(1 << 4) /* interrupts is enabled */
#define UARTLITE_STAT_TX_FIFO_FULL	(1 << 3) /* transmit FIFO is full */
#define UARTLITE_STAT_TX_FIFO_EMPTY	(1 << 2) /* transmit FIFO is empty */
#define UARTLITE_STAT_RX_FIFO_FULL	(1 << 1) /* receive FIFO is full */
#define UARTLITE_STAT_RX_FIFO_VALID_DATA	(1 << 0) /* receive FIFO has data */

#define UARTLITE_CTRL_ENABLE_INTR	(1 << 4) /* Enable interrupt */
#define UARTLITE_CTRL_RST_RX_FIFO	(1 << 1) /* Reset/clear the receive FIFO */
#define UARTLITE_CTRL_RST_TX_FIFO	(1 << 0) /* Reset/clear the transmit FIFO */

struct xlnx_uartlite {
	u32 rx_fifo;
	u32 tx_fifo;
	u32 stat_reg;
	u32 ctrl_reg;
};

struct xlnx_uartlite_priv {
	struct serial *p;
	struct xlnx_uartlite *regs;
	u32 input_clock;
	u32 irq;
};

bool xlnx_uartlite_lowlevel_can_getc(struct xlnx_uartlite *regs);
u8 xlnx_uartlite_lowlevel_getc(struct xlnx_uartlite *regs);
bool xlnx_uartlite_lowlevel_can_putc(struct xlnx_uartlite *reg);
void xlnx_uartlite_lowlevel_putc(struct xlnx_uartlite *reg, u8 ch);
void xlnx_uartlite_lowlevel_init(struct xlnx_uartlite_priv *port);

#endif /* __XLNX_UARTLITE_H__ */
