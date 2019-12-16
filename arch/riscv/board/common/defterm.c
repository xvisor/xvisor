/**
 * Copyright (c) 2018 Anup Patel.
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
 * @file defterm.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief arch default terminal functions using drivers
 */

#include <vmm_error.h>
#include <vmm_types.h>
#include <vmm_compiler.h>
#include <vmm_devtree.h>
#include <vmm_host_aspace.h>
#include <arch_defterm.h>

#include <cpu_sbi.h>

struct defterm_ops {
	int (*putc)(u8 ch);
	int (*getc)(u8 *ch);
	int (*init)(struct vmm_devtree_node *node);
};

static int sbi_defterm_putc(u8 ch)
{
	sbi_console_putchar((int)ch);

	return VMM_OK;
}

static int sbi_defterm_getc(u8 *ch)
{
	*ch = (u8)sbi_console_getchar();

	return VMM_OK;
}

static int __init sbi_defterm_init(struct vmm_devtree_node *node)
{
	return VMM_OK;
}

static struct defterm_ops sbi_ops = {
	.putc = sbi_defterm_putc,
	.getc = sbi_defterm_getc,
	.init = sbi_defterm_init
};

#if defined(CONFIG_SERIAL_8250_UART)

#include <drv/serial/8250-uart.h>

static struct uart_8250_port uart8250_port;

static int uart8250_defterm_putc(u8 ch)
{
	if (!uart_8250_lowlevel_can_putc(&uart8250_port)) {
		return VMM_EFAIL;
	}
	uart_8250_lowlevel_putc(&uart8250_port, ch);
	return VMM_OK;
}

static int uart8250_defterm_getc(u8 *ch)
{
	if (!uart_8250_lowlevel_can_getc(&uart8250_port)) {
		return VMM_EFAIL;
	}
	*ch = uart_8250_lowlevel_getc(&uart8250_port);
	return VMM_OK;
}

static int __init uart8250_defterm_init(struct vmm_devtree_node *node)
{
	int rc;

	rc = vmm_devtree_regmap(node, &uart8250_port.base, 0);
	if (rc) {
		return rc;
	}

	rc = vmm_devtree_clock_frequency(node,
				&uart8250_port.input_clock);
	if (rc) {
		return rc;
	}

	if (vmm_devtree_read_u32(node, "baudrate",
				 &uart8250_port.baudrate)) {
		uart8250_port.baudrate = 115200;
	}

	if (vmm_devtree_read_u32(node, "reg-shift",
				 &uart8250_port.reg_shift)) {
		uart8250_port.reg_shift = 2;
	}

	if (vmm_devtree_read_u32(node, "reg-io-width",
				 &uart8250_port.reg_width)) {
		uart8250_port.reg_width = 1;
	}

	uart_8250_lowlevel_init(&uart8250_port);

	return VMM_OK;
}

static struct defterm_ops uart8250_ops = {
	.putc = uart8250_defterm_putc,
	.getc = uart8250_defterm_getc,
	.init = uart8250_defterm_init
};

#else

#define uart8250_ops sbi_ops

#endif

static struct vmm_devtree_nodeid defterm_devid_table[] = {
	{ .compatible = "ns8250", .data = &uart8250_ops },
	{ .compatible = "ns16450", .data = &uart8250_ops },
	{ .compatible = "ns16550a", .data = &uart8250_ops },
	{ .compatible = "ns16550", .data = &uart8250_ops },
	{ .compatible = "ns16750", .data = &uart8250_ops },
	{ .compatible = "ns16850", .data = &uart8250_ops },
	{ .compatible = "snps,dw-apb-uart", .data = &uart8250_ops },
	{ /* end of list */ },
};

static const struct defterm_ops *ops = &sbi_ops;

int arch_defterm_putc(u8 ch)
{
	return ops->putc(ch);
}

int arch_defterm_getc(u8 *ch)
{
	return ops->getc(ch);
}

int __init arch_defterm_init(void)
{
	int rc = VMM_OK;
	const char *attr;
	struct vmm_devtree_node *node = NULL;
	const struct vmm_devtree_nodeid *nodeid;

	/* Find choosen console node */
	node = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPARATOR_STRING
				   VMM_DEVTREE_CHOSEN_NODE_NAME);
	if (!node) {
		goto use_sbi;
	}

	if (!vmm_devtree_is_available(node)) {
		goto use_sbi;
	}

	rc = vmm_devtree_read_string(node,
				VMM_DEVTREE_CONSOLE_ATTR_NAME, &attr);
	vmm_devtree_dref_node(node);
	if (rc) {
		goto use_sbi;
	}

	node = vmm_devtree_getnode(attr);
	if (!node) {
		goto use_sbi;
	}

	/* Find appropriate defterm ops */
	nodeid = vmm_devtree_match_node(defterm_devid_table, node);
	if (nodeid) {
		ops = nodeid->data;
	}

	rc = ops->init(node);
	vmm_devtree_dref_node(node);

	return rc;

use_sbi:
	ops = &sbi_ops;
	ops->init(NULL);
	return VMM_OK;
}
