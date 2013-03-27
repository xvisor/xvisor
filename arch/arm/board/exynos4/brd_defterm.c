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
#include <drv/samsung-uart.h>
#include <vmm_devtree.h>

#include <exynos/mach/map.h>

#define	EXYNOS4_DEFAULT_UART_INCLK		24000000
#define	EXYNOS4_DEFAULT_UART_BAUD		115200

static virtual_addr_t exynos4_defterm_base;
static u32 exynos4_defterm_inclk;
static u32 exynos4_defterm_baud;

int arch_defterm_putc(u8 ch)
{
	if (!samsung_lowlevel_can_putc(exynos4_defterm_base)) {
		return VMM_EFAIL;
	}
	samsung_lowlevel_putc(exynos4_defterm_base, ch);
	return VMM_OK;
}

int arch_defterm_getc(u8 * ch)
{
	if (!samsung_lowlevel_can_getc(exynos4_defterm_base)) {
		return VMM_EFAIL;
	}
	*ch = samsung_lowlevel_getc(exynos4_defterm_base);
	return VMM_OK;
}

int __init arch_defterm_init(void)
{
	int rc;
	u32 *val;
	char *console_device = NULL;
	struct vmm_devtree_node *node;

	/* find the device used as console */
	node = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPARATOR_STRING
				   VMM_DEVTREE_CHOOSEN_NODE_NAME);
	if (!node) {
		return VMM_ENODEV;
	}

	console_device = 
		vmm_devtree_attrval(node, VMM_DEVTREE_CONSOLE_ATTR_NAME);
	if (!console_device) {
		return VMM_ENODEV;
	}

	/* find the device used as console */
	node = vmm_devtree_getnode(console_device);
	if (!node) {
		return VMM_ENODEV;
	}

	/* map this console device */
	rc = vmm_devtree_regmap(node, &exynos4_defterm_base, 0);
	if (rc) {
		return rc;
	}

	/* retrieve clock frequency */
	rc = vmm_devtree_clock_frequency(node, &exynos4_defterm_inclk);
	if (rc) {
		return rc;
	}
	
	/* retrieve baud rate */
	val = vmm_devtree_attrval(node, "baudrate");
	exynos4_defterm_baud = (val) ? *val : EXYNOS4_DEFAULT_UART_BAUD;

	/* initialize the console port */
	samsung_lowlevel_init(exynos4_defterm_base,
			      exynos4_defterm_baud, exynos4_defterm_inclk);

	return VMM_OK;
}
