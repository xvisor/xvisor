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
 * @brief default serial terminal for Sun4i SOC
 */

#include <vmm_types.h>
#include <vmm_error.h>
#include <vmm_compiler.h>
#include <vmm_devtree.h>
#include <vmm_host_aspace.h>
#include <drv/uart.h>

static virtual_addr_t sun4i_uart_base;
static u32 sun4i_uart_inclk;
static u32 sun4i_uart_baud;
static u32 sun4i_uart_align;

int arch_defterm_putc(u8 ch)
{
	if (!uart_lowlevel_can_putc(sun4i_uart_base, sun4i_uart_align)) {
		return VMM_EFAIL;
	}
	uart_lowlevel_putc(sun4i_uart_base, sun4i_uart_align, ch);
	return VMM_OK;
}

int arch_defterm_getc(u8 *ch)
{
	if (!uart_lowlevel_can_getc(sun4i_uart_base, sun4i_uart_align)) {
		return VMM_EFAIL;
	}
	*ch = uart_lowlevel_getc(sun4i_uart_base, sun4i_uart_align);
	return VMM_OK;
}

int __init arch_defterm_init(void)
{
	int rc;
	u32 *val;
	struct vmm_devtree_node *node;

	node = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPARATOR_STRING
				   VMM_DEVTREE_HOSTINFO_NODE_NAME
				   VMM_DEVTREE_PATH_SEPARATOR_STRING "soc"
				   VMM_DEVTREE_PATH_SEPARATOR_STRING "uart0");
	if (!node) {
		return VMM_ENODEV;
	}
	rc = vmm_devtree_regmap(node, &sun4i_uart_base, 0);
	if (rc) {
		return rc;
	}

	val = vmm_devtree_attrval(node, VMM_DEVTREE_CLOCK_RATE_ATTR_NAME);
	sun4i_uart_inclk = (val) ? *val : 24000000;

	val = vmm_devtree_attrval(node, "baudrate");
	sun4i_uart_baud = (val) ? *val : 115200;

	val = vmm_devtree_attrval(node, "reg_align");
	sun4i_uart_align = (val) ? *val : 4;

	uart_lowlevel_init(sun4i_uart_base,
			    sun4i_uart_align,
			    sun4i_uart_baud, 
			    sun4i_uart_inclk);
	return VMM_OK;
}
