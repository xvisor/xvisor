/**
 * Copyright (c) 2011 Pranav Sawargaonkar.
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
 * @file brd_defterm.c
 * @author Pranav Sawargaonkar (pranav.sawargaonkar@gmail.com)
 * @brief default serial terminal source
 */

#include <vmm_types.h>
#include <vmm_error.h>
#include <vmm_host_io.h>
#include <vmm_host_aspace.h>
#include <serial/uart.h>
#include <omap3/config.h>

static virtual_addr_t omap3_uart_base;

int arch_defterm_putc(u8 ch)
{
	if (!uart_lowlevel_can_putc(omap3_uart_base, 4)) {
		return VMM_EFAIL;
	}
	uart_lowlevel_putc(omap3_uart_base, 4, ch);
	return VMM_OK;
}

int arch_defterm_getc(u8 *ch)
{
	if (!uart_lowlevel_can_getc(omap3_uart_base, 4)) {
		return VMM_EFAIL;
	}
	*ch = uart_lowlevel_getc(omap3_uart_base, 4);
	return VMM_OK;
}

int arch_defterm_init(void)
{
	omap3_uart_base = vmm_host_iomap(OMAP3_UART_BASE, 0x1000);
	uart_lowlevel_init("st16654", omap3_uart_base, 4,
			   OMAP3_UART_BAUD, OMAP3_UART_INCLK);
	return VMM_OK;
}
