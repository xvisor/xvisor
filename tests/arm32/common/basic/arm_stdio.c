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
 * @author Anup Patel (anup@brainfault.org)
 * @brief source file for common input/output functions
 */

#include <arm_board.h>
#include <arm_stdio.h>

void arm_stdio_init(void)
{
	int rc;

	rc = arm_board_serial_init();
	if (rc) {
		while (1);
	}
}

void arm_puts(const char * str)
{
	while (*str) {
		arm_board_serial_putc(*str);
		str++;
	}
}

void arm_gets(char *s, int maxwidth, char endchar)
{
	char *retval;
	char ch;
	retval = s;
	ch = arm_board_serial_getc();
	while (ch != endchar && maxwidth > 0) {
		*retval = ch;
		retval++;
		maxwidth--;
		if (maxwidth == 0)
			break;
		ch = arm_board_serial_getc();
	}
	*retval = '\0';
	return;
}

