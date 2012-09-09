/**
 * Copyright (c) 2012 Anup Patel.
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
 * @author Anup Patel (anup@brainfault.org)
 * @brief default serial terminal
 */

#include <vmm_error.h>
#include <vmm_types.h>
#include <vmm_compiler.h>
#include <vmm_host_aspace.h>
#include <serial/pl011.h>
#include <motherboard.h>

#define	V2M_DEFAULT_UART_BASE			V2M_UART0
#define	V2M_DEFAULT_UART_INCLK			24000000
#define	V2M_DEFAULT_UART_BAUD			115200

static virtual_addr_t v2m_defterm_base;

int arch_defterm_putc(u8 ch)
{
	if (!pl011_lowlevel_can_putc(v2m_defterm_base)) {
		return VMM_EFAIL;
	}
	pl011_lowlevel_putc(v2m_defterm_base, ch);
	return VMM_OK;
}

int arch_defterm_getc(u8 * ch)
{
	if (!pl011_lowlevel_can_getc(v2m_defterm_base)) {
		return VMM_EFAIL;
	}
	*ch = pl011_lowlevel_getc(v2m_defterm_base);
	return VMM_OK;
}

int __init arch_defterm_init(void)
{
	v2m_defterm_base = vmm_host_iomap(V2M_DEFAULT_UART_BASE, 0x1000);
	pl011_lowlevel_init(v2m_defterm_base,
			    V2M_DEFAULT_UART_BAUD, V2M_DEFAULT_UART_INCLK);
	return VMM_OK;
}
