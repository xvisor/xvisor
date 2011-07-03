/**
 * Copyright (c) 2010 Himanshu Chauhan.
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
 * @file brd_console.c
 * @version 0.1
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief main source file for serial port in qemu-mips board.
 */

#include <vmm_types.h>
#include <vmm_error.h>
#include <vmm_host_aspace.h>
#include <serial/uart.h>

static virtual_addr_t uart_base = 0;
extern virtual_addr_t isa_vbase;

int vmm_defterm_getc(u8 *ch)
{
	if (uart_base) {
		if (!uart_lowlevel_can_getc(uart_base, 1)) {
			return VMM_EFAIL;
		}
		*ch = uart_lowlevel_getc(uart_base, 1);	
		if (*ch == '\r') *ch = '\n';
		uart_lowlevel_putc(uart_base, 1, *ch);
	}
	return VMM_OK;
}

int vmm_defterm_putc(u8 ch)
{
	if (uart_base) {
		if (!uart_lowlevel_can_putc(uart_base, 1)) {
			return VMM_EFAIL;
		}
		uart_lowlevel_putc(uart_base, 1, ch);
	}
	return VMM_OK;
}

int vmm_defterm_init(void)
{
	uart_base = isa_vbase + 0x3F8;
	uart_lowlevel_init(uart_base, 1, 115200, 1843200);

	return VMM_OK;
}
