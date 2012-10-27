/**
 * Copyright (c) 2012 Jean-Christophe Dubois.
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
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @brief default serial terminal
 */

#include <vmm_error.h>
#include <vmm_types.h>
#include <vmm_compiler.h>
#include <vmm_host_aspace.h>
#include <versatile_board.h>
#include <drv/pl011.h>

#define	VERSATILE_DEFAULT_UART_BASE			VERSATILE_UART0_BASE
#define	VERSATILE_DEFAULT_UART_INCLK			24000000
#define	VERSATILE_DEFAULT_UART_BAUD			115200

static virtual_addr_t versatile_defterm_base;

int arch_defterm_putc(u8 ch)
{
	if (!pl011_lowlevel_can_putc(versatile_defterm_base)) {
		return VMM_EFAIL;
	}
	pl011_lowlevel_putc(versatile_defterm_base, ch);
	return VMM_OK;
}

int arch_defterm_getc(u8 * ch)
{
	if (!pl011_lowlevel_can_getc(versatile_defterm_base)) {
		return VMM_EFAIL;
	}
	*ch = pl011_lowlevel_getc(versatile_defterm_base);
	return VMM_OK;
}

int __init arch_defterm_init(void)
{
	versatile_defterm_base = vmm_host_iomap(VERSATILE_DEFAULT_UART_BASE, 0x1000);
	pl011_lowlevel_init(versatile_defterm_base,
			    VERSATILE_DEFAULT_UART_BAUD, VERSATILE_DEFAULT_UART_INCLK);
	return VMM_OK;
}
