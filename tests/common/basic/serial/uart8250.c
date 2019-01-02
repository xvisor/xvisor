/**
 * Copyright (c) 2019 Anup Patel.
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
 * @file uart8250.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Source file for UART 8250 serial port driver.
 */

#include <arch_io.h>
#include <arch_math.h>
#include <serial/uart8250.h>

#define UART_RBR_OFFSET		0	/* In:  Recieve Buffer Register */
#define UART_THR_OFFSET		0	/* Out: Transmitter Holding Register */
#define UART_DLL_OFFSET		0	/* Out: Divisor Latch Low */
#define UART_IER_OFFSET		1	/* I/O: Interrupt Enable Register */
#define UART_DLM_OFFSET		1	/* Out: Divisor Latch High */
#define UART_FCR_OFFSET		2	/* Out: FIFO Control Register */
#define UART_IIR_OFFSET		2	/* I/O: Interrupt Identification Register */
#define UART_LCR_OFFSET		3	/* Out: Line Control Register */
#define UART_MCR_OFFSET		4	/* Out: Modem Control Register */
#define UART_LSR_OFFSET		5	/* In:  Line Status Register */
#define UART_MSR_OFFSET		6	/* In:  Modem Status Register */
#define UART_SCR_OFFSET		7	/* I/O: Scratch Register */
#define UART_MDR1_OFFSET	8	/* I/O:  Mode Register */

#define UART_LSR_FIFOE		0x80    /* Fifo error */
#define UART_LSR_TEMT		0x40    /* Transmitter empty */
#define UART_LSR_THRE		0x20    /* Transmit-hold-register empty */
#define UART_LSR_BI		0x10    /* Break interrupt indicator */
#define UART_LSR_FE		0x08    /* Frame error indicator */
#define UART_LSR_PE		0x04    /* Parity error indicator */
#define UART_LSR_OE		0x02    /* Overrun error indicator */
#define UART_LSR_DR		0x01    /* Receiver data ready */
#define UART_LSR_BRK_ERROR_BITS	0x1E    /* BI, FE, PE, OE bits */

static u32 get_reg(virtual_addr_t base,
		   u32 reg_shift, u32 reg_width, u32 num)
{
	u32 offset = num << reg_shift;

	if (reg_width == 1)
		return arch_readb((void *)(base + offset));
	else if (reg_width == 2)
		return arch_readw((void *)(base + offset));
	else
		return arch_readl((void *)(base + offset));
}

static void set_reg(virtual_addr_t base,
		    u32 reg_shift, u32 reg_width, u32 num, u32 val)
{
	u32 offset = num << reg_shift;

	if (reg_width == 1)
		arch_writeb(val, (void *)(base + offset));
	else if (reg_width == 2)
		arch_writew(val, (void *)(base + offset));
	else
		arch_writel(val, (void *)(base + offset));
}

bool uart8250_can_getc(virtual_addr_t base, u32 reg_shift, u32 reg_width)
{
	if (!(get_reg(base, reg_shift, reg_width, UART_LSR_OFFSET) &
	      UART_LSR_DR))
		return FALSE;
	return TRUE;
}

char uart8250_getc(virtual_addr_t base, u32 reg_shift, u32 reg_width)
{
	while (!(get_reg(base, reg_shift, reg_width, UART_LSR_OFFSET) &
		 UART_LSR_DR)) ;
	return get_reg(base, reg_shift, reg_width, UART_RBR_OFFSET);
}

void uart8250_putc(virtual_addr_t base,
		   u32 reg_shift, u32 reg_width, char ch)
{
	while ((get_reg(base, reg_shift, reg_width, UART_LSR_OFFSET) &
	        UART_LSR_THRE) == 0) ;

	set_reg(base, reg_shift, reg_width, UART_THR_OFFSET, ch);
}

void uart8250_init(virtual_addr_t base,
		   u32 reg_shift, u32 reg_width,
		   u32 baudrate, u32 input_clock)
{
	u16 bdiv = input_clock / (16 * baudrate);
	u8 bdiv_l = bdiv & 0xff;
	u8 bdiv_u = (bdiv >> 8) & 0xff;

	/* Disable all interrupts */
	set_reg(base, reg_shift, reg_width, UART_IER_OFFSET, 0x00);
	/* Enable DLAB */
	set_reg(base, reg_shift, reg_width, UART_LCR_OFFSET, 0x80);
	/* Set divisor low byte */
	set_reg(base, reg_shift, reg_width, UART_DLL_OFFSET, bdiv_l);
	/* Set divisor high byte */
	set_reg(base, reg_shift, reg_width, UART_DLM_OFFSET, bdiv_u);
	/* 8 bits, no parity, one stop bit */
	set_reg(base, reg_shift, reg_width, UART_LCR_OFFSET, 0x03);
	/* Enable FIFO */
	set_reg(base, reg_shift, reg_width, UART_FCR_OFFSET, 0x01);
	/* No modem control DTR RTS */
	set_reg(base, reg_shift, reg_width, UART_MCR_OFFSET, 0x00);
	/* Clear line status */
	get_reg(base, reg_shift, reg_width, UART_LSR_OFFSET);
	/* Read receive buffer */
	get_reg(base, reg_shift, reg_width, UART_RBR_OFFSET);
	/* Set scratchpad */
	set_reg(base, reg_shift, reg_width, UART_SCR_OFFSET, 0x00);
}
