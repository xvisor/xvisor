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
 * @version 1.0
 * @author Pranav Sawargaonkar (pranav.sawargaonkar@gmail.com)
 * @brief default serial terminal source
 */

#include <vmm_types.h>
#include <vmm_error.h>
#include <vmm_host_io.h>
#include <serial/uart.h>
#include <omap3/config.h>

int vmm_defterm_putc(u8 ch)
{
	if (!uart_lowlevel_can_putc(OMAP3_UART_BASE, 4)) {
		return VMM_EFAIL;
	}
	uart_lowlevel_putc(OMAP3_UART_BASE, 4, ch);
	return VMM_OK;
}

int vmm_defterm_getc(u8 *ch)
{
	if (!uart_lowlevel_can_getc(OMAP3_UART_BASE, 4)) {
		return VMM_EFAIL;
	}
	*ch = uart_lowlevel_getc(OMAP3_UART_BASE, 4);
	return VMM_OK;
}

int vmm_defterm_init(void)
{
	uart_lowlevel_init(OMAP3_UART_BASE, 4,
			   OMAP3_UART_BAUD, OMAP3_UART_INCLK);
	return VMM_OK;
}
