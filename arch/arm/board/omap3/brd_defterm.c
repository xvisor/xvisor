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

#include <vmm_error.h>
#include <vmm_devtree.h>
#include <vmm_host_io.h>
#include <drv/omap-uart.h>
#include <omap3_plat.h>

static virtual_addr_t omap3_defterm_base;
static u32 omap3_defterm_inclk;
static u32 omap3_defterm_baud;

int arch_defterm_putc(u8 ch)
{
	if (!omap_uart_lowlevel_can_putc(omap3_defterm_base, 4)) {
		return VMM_EFAIL;
	}
	omap_uart_lowlevel_putc(omap3_defterm_base, 4, ch);
	return VMM_OK;
}

int arch_defterm_getc(u8 *ch)
{
	if (!omap_uart_lowlevel_can_getc(omap3_defterm_base, 4)) {
		return VMM_EFAIL;
	}
	*ch = omap_uart_lowlevel_getc(omap3_defterm_base, 4);
	return VMM_OK;
}

int __init arch_defterm_init(void)
{
	int rc;
	u32 *val;
	const char *attr;
	struct vmm_devtree_node *node;

	node = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPARATOR_STRING
				   VMM_DEVTREE_CHOOSEN_NODE_NAME);
	if (!node) {
		return VMM_ENODEV;
	}

	attr = vmm_devtree_attrval(node, VMM_DEVTREE_CONSOLE_ATTR_NAME);
	if (!attr) {
		return VMM_ENODEV;
	}
   
	node = vmm_devtree_getnode(attr);
	if (!node) {
		return VMM_ENODEV;
	}

	rc = vmm_devtree_regmap(node, &omap3_defterm_base, 0);
	if (rc) {
		return rc;
	}

	val = vmm_devtree_attrval(node, VMM_DEVTREE_CLOCK_RATE_ATTR_NAME);
	omap3_defterm_inclk = (val) ? *val : 24000000;

	val = vmm_devtree_attrval(node, "baudrate");
	omap3_defterm_baud = (val) ? *val : 115200;

	omap_uart_lowlevel_init(omap3_defterm_base, 4, 
				omap3_defterm_baud, 
				omap3_defterm_inclk);

	return VMM_OK;
}
