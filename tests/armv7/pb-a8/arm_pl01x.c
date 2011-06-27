/**
 * Copyright (c) 2011 Anup Patel.
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
 * @file arm_pl01x.c
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief source file for PrimeCell PL011/PL010 serial port driver.
 */

#include <arm_pl01x.h>

u32 arm_readl(void * addr)
{
	return *((u32 *)addr);
}

void arm_writel(u32 data, void * addr)
{
	*((u32 *)addr) = data;
}

char arm_pl01x_dprobe(u32 base, u32 type)
{
	if(arm_readl((void*)(base + UART_PL01x_FR)) & UART_PL01x_FR_TXFF) {
		return 1;
	}
	return 0;
}

void arm_pl01x_putc(u32 base, u32 type, char ch)
{
	if(ch=='\n') {
		/* Wait until there is space in the FIFO */
		while (arm_readl((void*)(base + UART_PL01x_FR)) & UART_PL01x_FR_TXFF);

		/* Send the character */
		arm_writel('\r', (void*)(base + UART_PL01x_DR));
	}

	/* Wait until there is space in the FIFO */
	while (arm_readl((void*)(base + UART_PL01x_FR)) & UART_PL01x_FR_TXFF);

	/* Send the character */
	arm_writel(ch, (void*)(base + UART_PL01x_DR));
}

void arm_pl01x_puts(u32 base, u32 type, const char * str)
{
	while (*str) {
		arm_pl01x_putc(base, type, *str);
		str++;
	}
}

char arm_pl01x_getc(u32 base, u32 type)
{
	char data;

	/* Wait until there is data in the FIFO */
	while (arm_readl((void*)(base + UART_PL01x_FR)) & UART_PL01x_FR_RXFE);

	data = arm_readl((void*)(base + UART_PL01x_DR));

	/* Check for an error flag */
	if (data & 0xFFFFFF00) {
		/* Clear the error */
		arm_writel(0xFFFFFFFF, (void*)(base + UART_PL01x_ECR));
		return -1;
	}

	/* FIXME: Hack required for qemu */
	if (data == '\r')
		data = '\n';

	/* FIXME: Hack required for qemu */
	arm_pl01x_putc(base, type, data);

	return data;
}

void arm_pl01x_gets(u32 base, u32 type, char *s, int maxwidth, char endchar)
{
	char *retval;
	char ch;
	retval = s;
	ch = arm_pl01x_getc(base, type);
	while (ch != endchar && maxwidth > 0) {
		*retval = ch;
		retval++;
		maxwidth--;
		if (maxwidth == 0)
			break;
		ch = arm_pl01x_getc(base, type);
	}
	*retval = '\0';
	return;
}

void arm_pl01x_init(u32 base, u32 type, u32 baudrate, u32 input_clock)
{
	unsigned int divider;
	unsigned int temp;
	unsigned int remainder;
	unsigned int fraction;

	if(type==PL01X_TYPE_1) {
		/* First, disable everything */
		arm_writel(0x0, (void*)(base + UART_PL011_CR));

		/*
		 * Set baud rate
		 *
		 * IBRD = UART_CLK / (16 * BAUD_RATE)
		 * FBRD = RND((64 * MOD(UART_CLK,(16 * BAUD_RATE))) 
		 * 	  / (16 * BAUD_RATE))
		 */
		temp = 16 * baudrate;
		divider = input_clock / temp;
		remainder = input_clock % temp;
		temp = (8 * remainder) / baudrate;
		fraction = (temp >> 1) + (temp & 1);

		arm_writel(divider, (void*)(base + UART_PL011_IBRD));
		arm_writel(fraction, (void*)(base + UART_PL011_FBRD));

		/* Set the UART to be 8 bits, 1 stop bit, 
		 * no parity, fifo enabled 
		 */
		arm_writel((UART_PL011_LCRH_WLEN_8 | UART_PL011_LCRH_FEN),
			(void*)(base + UART_PL011_LCRH));

		/* Finally, enable the UART */
		arm_writel((UART_PL011_CR_UARTEN | 
				UART_PL011_CR_TXE | 
				UART_PL011_CR_RXE),
			(void*)(base + UART_PL011_CR));
	} else {
		/* First, disable everything */
		arm_writel(0x0, (void*)(base + UART_PL010_CR));

		/* Set baud rate */
		switch (baudrate) {
		case 9600:
			divider = UART_PL010_BAUD_9600;
			break;

		case 19200:
			divider = UART_PL010_BAUD_9600;
			break;

		case 38400:
			divider = UART_PL010_BAUD_38400;
			break;

		case 57600:
			divider = UART_PL010_BAUD_57600;
			break;

		case 115200:
			divider = UART_PL010_BAUD_115200;
			break;

		default:
			divider = UART_PL010_BAUD_38400;
		}

		arm_writel(((divider & 0xf00) >> 8), 
					(void*)(base + UART_PL010_LCRM));
		arm_writel((divider & 0xff), (void*)(base + UART_PL010_LCRL));

		/* Set the UART to be 8 bits, 1 stop bit, 
		 * no parity, fifo enabled */
		arm_writel((UART_PL010_LCRH_WLEN_8 | UART_PL010_LCRH_FEN),
					(void*)(base + UART_PL010_LCRH));

		/* Finally, enable the UART */
		arm_writel((UART_PL010_CR_UARTEN), 
					(void*)(base + UART_PL010_CR));
	}
}

