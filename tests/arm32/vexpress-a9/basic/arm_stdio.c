/**
 * Copyright (c) 2012 Sukanto Ghosh.
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
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief source file for common input/output functions
 *
 * Adapted from tests/arm32/pb-a8/basic/arm_stdio.c
 */

#include <arm_pl01x.h>
#include <arm_plat.h>
#include <arm_stdio.h>

#define	CA9X4_DEFAULT_UART_BASE			V2M_UART0
#define	CA9X4_UART_TYPE				PL01X_TYPE_1
#define	CA9X4_DEFAULT_UART_INCLK		24000000
#define	CA9X4_DEFAULT_UART_BAUD			115200

void arm_putc(char ch)
{
	if (ch == '\n') {
		arm_pl01x_putc(CA9X4_DEFAULT_UART_BASE, CA9X4_UART_TYPE, '\r');
	}
	arm_pl01x_putc(CA9X4_DEFAULT_UART_BASE, CA9X4_UART_TYPE, ch);
}

char arm_getc(void)
{
	char ch = arm_pl01x_getc(CA9X4_DEFAULT_UART_BASE, CA9X4_UART_TYPE);
	if (ch == '\r') {
		ch = '\n';
	}
	arm_putc(ch);
	return ch;
}

void arm_stdio_init(void)
{
	arm_pl01x_init(CA9X4_DEFAULT_UART_BASE, 
			CA9X4_UART_TYPE, 
			CA9X4_DEFAULT_UART_BAUD, 
			CA9X4_DEFAULT_UART_INCLK);
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

