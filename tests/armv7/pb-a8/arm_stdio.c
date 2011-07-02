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
 * @file arm_stdio.c
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief source file for common input/output functions
 */

#include <arm_pl01x.h>
#include <arm_stdio.h>

#define	PBA8_UART_BASE			0x10009000
#define	PBA8_UART_TYPE			PL01X_TYPE_1
#define	PBA8_UART_INCLK			24000000
#define	PBA8_UART_BAUD			115200

void arm_putc(char ch)
{
	arm_pl01x_putc(PBA8_UART_BASE, PBA8_UART_TYPE, ch);
}

char arm_getc(void)
{
	char ret = arm_pl01x_getc(PBA8_UART_BASE, PBA8_UART_TYPE);
#if 0
	/* Hack code to run test code directly on QEMU */
	if (ret == '\r')
		ret = '\n';
	arm_putc(ret);
#endif
	return ret;
}

void arm_stdio_init(void)
{
	arm_pl01x_init(PBA8_UART_BASE, 
			PBA8_UART_TYPE, 
			PBA8_UART_BAUD, 
			PBA8_UART_INCLK);
}

void arm_puts(const char * str)
{
	while (*str) {
		arm_putc(*str);
		str++;
	}
}

void arm_gets(char *s, int maxwidth, char endchar)
{
	char *retval;
	char ch;
	retval = s;
	ch = arm_getc();
	while (ch != endchar && maxwidth > 0) {
		*retval = ch;
		retval++;
		maxwidth--;
		if (maxwidth == 0)
			break;
		ch = arm_getc();
	}
	*retval = '\0';
	return;
}

