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
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief Header file for UART serial port driver.
 */

#ifndef __UART_H_
#define __UART_H_

#include <vmm_types.h>

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

#define UART_LSR_THRE		0x20	/* Transmit-hold-register empty */
#define UART_LSR_DR		0x01	/* Receiver data ready */

#define REG_UART_RBR(base,align)	((base)+UART_RBR_OFFSET*(align))
#define REG_UART_THR(base,align)	((base)+UART_THR_OFFSET*(align))
#define REG_UART_DLL(base,align)	((base)+UART_DLL_OFFSET*(align))
#define REG_UART_IER(base,align)	((base)+UART_IER_OFFSET*(align))
#define REG_UART_DLM(base,align)	((base)+UART_DLM_OFFSET*(align))
#define REG_UART_IIR(base,align)	((base)+UART_IIR_OFFSET*(align))
#define REG_UART_FCR(base,align)	((base)+UART_FCR_OFFSET*(align))
#define REG_UART_LCR(base,align)	((base)+UART_LCR_OFFSET*(align))
#define REG_UART_MCR(base,align)	((base)+UART_MCR_OFFSET*(align))
#define REG_UART_LSR(base,align)	((base)+UART_LSR_OFFSET*(align))
#define REG_UART_MSR(base,align)	((base)+UART_MSR_OFFSET*(align))
#define REG_UART_SCR(base,align)	((base)+UART_SCR_OFFSET*(align))

bool uart_lowlevel_can_getc(virtual_addr_t base, u32 reg_align);
char uart_lowlevel_getc(virtual_addr_t base, u32 reg_align);
bool uart_lowlevel_can_putc(virtual_addr_t base, u32 reg_align);
void uart_lowlevel_putc(virtual_addr_t base, u32 reg_align, char ch);
void uart_lowlevel_init(virtual_addr_t base, u32 reg_align, u32 baudrate, u32 input_clock);

#endif /* __UART_H_ */
