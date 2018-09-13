/**
 * Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWide.
 * Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * All rights reserved.
 * Inspired from pl01x.c, written by Anup Patel.
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
 * @file imx.c
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @brief source file for i.MX serial port driver.
 */

#include <arch_io.h>
#include <arch_math.h>
#include <serial/imx.h>

void imx_putc(virtual_addr_t base, char ch)
{
	/* Wait until there is space in the FIFO */
	while (!(arch_readw((void*)(base + USR1)) & USR1_TRDY)) {
		;
	}

	/* Send the character */
	arch_writeb(ch, (void*)(base + URTX0));
}

bool imx_can_getc(virtual_addr_t base)
{
	return (arch_readw((void*)(base + USR2)) & USR2_RDR) ? TRUE : FALSE;
}

char imx_getc(virtual_addr_t base)
{
	u16 data;

	/* Wait until there is data in the FIFO */
	while (!(arch_readw((void*)(base + USR2)) & USR2_RDR)) {
		;
	}

	data = arch_readb((void*)(base + URXD0));

	/* Check for an error flag */
	if (data & 0xFF00) {
		/* Clear the error */
		arch_writew(0x8400, (void*)(base + USR1));
		return -1;
	}

	return data;
}

void imx_init(virtual_addr_t base, u32 baudrate, u32 input_clock)
{
	unsigned int temp;

	/* First, disable everything */
	arch_writew(0x0, (void*)(base + UCR1));
	arch_writew(0x0, (void*)(base + UCR2));

	/*
	 * Set baud rate
	 *
	 * (UBMR + 1) / (UBIR + 1) = input_clock / (16 * BAUD_RATE)
	 * Set UBIR = 0xF:
	 * UBMR + 1 = input_clock / BAUD_RATE
	 */
	temp = arch_udiv32(input_clock, baudrate);
	arch_writew(0xF, (void*)(base + UBIR));
	arch_writew(temp - 1, (void*)(base + UBMR));

	/* Set the UART to be 8 bits, 1 stop bit,
	 * no parity, fifo enabled */
	arch_writel(UCR2_WS | UCR2_TXEN | UCR2_RXEN, (void*)(base + UCR2));

	/* Finally, enable the UART */
	arch_writew(UCR1_UARTEN, (void*)(base + UCR1));
}
