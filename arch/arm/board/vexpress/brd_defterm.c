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
#include <vmm_devtree.h>
#include <vmm_host_aspace.h>
#include <drv/pl011.h>

static virtual_addr_t v2m_defterm_base;
static u32 v2m_defterm_inclk;
static u32 v2m_defterm_baud;

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

	rc = vmm_devtree_regmap(node, &v2m_defterm_base, 0);
	if (rc) {
		return rc;
	}

	val = vmm_devtree_attrval(node, VMM_DEVTREE_CLOCK_RATE_ATTR_NAME);
	v2m_defterm_inclk = (val) ? *val : 24000000;

	val = vmm_devtree_attrval(node, "baudrate");
	v2m_defterm_baud = (val) ? *val : 115200;

	pl011_lowlevel_init(v2m_defterm_base,
			    v2m_defterm_baud, 
			    v2m_defterm_inclk);
	return VMM_OK;
}
