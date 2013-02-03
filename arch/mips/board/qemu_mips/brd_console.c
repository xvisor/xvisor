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
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief main source file for serial port in qemu-mips board.
 */

#include <vmm_types.h>
#include <vmm_error.h>
#include <vmm_host_aspace.h>
#include <drv/8250-uart.h>

static struct uart_8250_port uart_port;
extern virtual_addr_t isa_vbase;

int arch_defterm_getc(u8 *ch)
{
	if (uart_port.base) {
		if (!uart_8250_lowlevel_can_getc(&uart_port);
			return VMM_EFAIL;
		}
		*ch = uart_8250_lowlevel_getc(&uart_port);
	}
	return VMM_OK;
}

int arch_defterm_putc(u8 ch)
{
	if (uart_port.base) {
		if (!uart_8250_lowlevel_can_putc(&uart_port);
			return VMM_EFAIL;
		}
		uart_8250_lowlevel_putc(&uart_port, ch);
	}
	return VMM_OK;
}

int arch_defterm_init(void)
{
	uart_port.base = isa_vbase + 0x3F8;
	uart_port.reg_align = 1;
	uart_port.baudrate = 115200;
	uart_port.input_clock = 1843200;
	uart_8250_lowlevel_init(&uart_port);

	return VMM_OK;
}
