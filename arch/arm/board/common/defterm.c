/**
 * Copyright (c) 2013 Anup Patel.
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
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief arch default terminal functions using drivers
 */

#include <vmm_error.h>
#include <vmm_types.h>
#include <vmm_compiler.h>
#include <vmm_devtree.h>
#include <vmm_host_aspace.h>
#include <arch_defterm.h>

struct defterm_ops {
	int (*putc)(u8 ch);
	int (*getc)(u8 *ch);
	int (*init)(struct vmm_devtree_node *node);
};

static int unknown_defterm_putc(u8 ch)
{
	return VMM_EFAIL;
}

static int unknown_defterm_getc(u8 *ch)
{
	return VMM_EFAIL;
}

static int __init unknown_defterm_init(struct vmm_devtree_node *node)
{
	return VMM_ENODEV;
}

#if defined(CONFIG_SERIAL_PL01X)

#include <drv/pl011.h>

static virtual_addr_t pl011_defterm_base;
static u32 pl011_defterm_inclk;
static u32 pl011_defterm_baud;

static int pl011_defterm_putc(u8 ch)
{
	if (!pl011_lowlevel_can_putc(pl011_defterm_base)) {
		return VMM_EFAIL;
	}
	pl011_lowlevel_putc(pl011_defterm_base, ch);
	return VMM_OK;
}

static int pl011_defterm_getc(u8 *ch)
{
	if (!pl011_lowlevel_can_getc(pl011_defterm_base)) {
		return VMM_EFAIL;
	}
	*ch = pl011_lowlevel_getc(pl011_defterm_base);
	return VMM_OK;
}

static int __init pl011_defterm_init(struct vmm_devtree_node *node)
{
	int rc;
	u32 *val;

	rc = vmm_devtree_regmap(node, &pl011_defterm_base, 0);
	if (rc) {
		return rc;
	}

	rc = vmm_devtree_clock_frequency(node, &pl011_defterm_inclk);
	if (rc) {
		return rc;
	}

	val = vmm_devtree_attrval(node, "baudrate");
	pl011_defterm_baud = (val) ? *val : 115200;

	pl011_lowlevel_init(pl011_defterm_base,
			    pl011_defterm_baud, 
			    pl011_defterm_inclk);

	return VMM_OK;
}

static struct defterm_ops pl011_ops = {
	.putc = pl011_defterm_putc,
	.getc = pl011_defterm_getc,
	.init = pl011_defterm_init
};

#else

static struct defterm_ops pl011_ops = {
	.putc = unknown_defterm_putc,
	.getc = unknown_defterm_getc,
	.init = unknown_defterm_init
};

#endif

#if defined(CONFIG_SERIAL_8250_UART)

#include <drv/8250-uart.h>

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
	u32 *val;

	rc = vmm_devtree_regmap(node, &uart8250_port.base, 0);
	if (rc) {
		return rc;
	}

	rc = vmm_devtree_clock_frequency(node, &uart8250_port.input_clock);
	if (rc) {
		return rc;
	}

	val = vmm_devtree_attrval(node, "baudrate");
	uart8250_port.baudrate = (val) ? *val : 115200;

	val = vmm_devtree_attrval(node, "reg_align");
	uart8250_port.reg_align = (val) ? *val : 4;

	uart_8250_lowlevel_init(&uart8250_port);

	return VMM_OK;
}

static struct defterm_ops uart8250_ops = {
	.putc = uart8250_defterm_putc,
	.getc = uart8250_defterm_getc,
	.init = uart8250_defterm_init
};

#else

static struct defterm_ops uart8250_ops = {
	.putc = unknown_defterm_putc,
	.getc = unknown_defterm_getc,
	.init = unknown_defterm_init
};

#endif

#if defined(CONFIG_SERIAL_OMAP_UART)

#include <drv/omap-uart.h>

static virtual_addr_t omap_defterm_base;
static u32 omap_defterm_inclk;
static u32 omap_defterm_baud;

static int omap_defterm_putc(u8 ch)
{
	if (!omap_uart_lowlevel_can_putc(omap_defterm_base, 4)) {
		return VMM_EFAIL;
	}
	omap_uart_lowlevel_putc(omap_defterm_base, 4, ch);
	return VMM_OK;
}

static int omap_defterm_getc(u8 *ch)
{
	if (!omap_uart_lowlevel_can_getc(omap_defterm_base, 4)) {
		return VMM_EFAIL;
	}
	*ch = omap_uart_lowlevel_getc(omap_defterm_base, 4);
	return VMM_OK;
}

static int __init omap_defterm_init(struct vmm_devtree_node *node)
{
	int rc;
	u32 *val;

	rc = vmm_devtree_regmap(node, &omap_defterm_base, 0);
	if (rc) {
		return rc;
	}

	rc = vmm_devtree_clock_frequency(node, &omap_defterm_inclk);
	if (rc) {
		return rc;
	}

	val = vmm_devtree_attrval(node, "baudrate");
	omap_defterm_baud = (val) ? *val : 115200;

	omap_uart_lowlevel_init(omap_defterm_base, 4, 
				omap_defterm_baud, 
				omap_defterm_inclk);

	return VMM_OK;
}

static struct defterm_ops omapuart_ops = {
	.putc = omap_defterm_putc,
	.getc = omap_defterm_getc,
	.init = omap_defterm_init
};

#else

static struct defterm_ops omapuart_ops = {
	.putc = unknown_defterm_putc,
	.getc = unknown_defterm_getc,
	.init = unknown_defterm_init
};

#endif

#if defined(CONFIG_SERIAL_IMX)

#include <drv/imx-uart.h>

static virtual_addr_t imx_defterm_base;
static u32 imx_defterm_inclk;
static u32 imx_defterm_baud;

static int imx_defterm_putc(u8 ch)
{
	if (!imx_lowlevel_can_putc(imx_defterm_base)) {
		return VMM_EFAIL;
	}
	imx_lowlevel_putc(imx_defterm_base, ch);
	return VMM_OK;
}

static int imx_defterm_getc(u8 *ch)
{
	if (!imx_lowlevel_can_getc(imx_defterm_base)) {
		return VMM_EFAIL;
	}
	*ch = imx_lowlevel_getc(imx_defterm_base);
	return VMM_OK;
}

static int __init imx_defterm_init(struct vmm_devtree_node *node)
{
	int rc;
	u32 *val;

	rc = vmm_devtree_regmap(node, &imx_defterm_base, 0);
	if (rc) {
		return rc;
	}

	rc = vmm_devtree_clock_frequency(node, &imx_defterm_inclk);
	if (rc) {
		return rc;
	}

	val = vmm_devtree_attrval(node, "baudrate");
	imx_defterm_baud = (val) ? *val : 115200;

	imx_lowlevel_init(imx_defterm_base,
			  imx_defterm_baud, 
			  imx_defterm_inclk);

	return VMM_OK;
}

static struct defterm_ops imx_ops = {
	.putc = imx_defterm_putc,
	.getc = imx_defterm_getc,
	.init = imx_defterm_init
};

#else

static struct defterm_ops imx_ops = {
	.putc = unknown_defterm_putc,
	.getc = unknown_defterm_getc,
	.init = unknown_defterm_init
};

#endif

#if defined(CONFIG_SERIAL_SAMSUNG)

#include <drv/samsung-uart.h>

static virtual_addr_t samsung_defterm_base;
static u32 samsung_defterm_inclk;
static u32 samsung_defterm_baud;

static int samsung_defterm_putc(u8 ch)
{
	if (!samsung_lowlevel_can_putc(samsung_defterm_base)) {
		return VMM_EFAIL;
	}
	samsung_lowlevel_putc(samsung_defterm_base, ch);
	return VMM_OK;
}

static int samsung_defterm_getc(u8 *ch)
{
	if (!samsung_lowlevel_can_getc(samsung_defterm_base)) {
		return VMM_EFAIL;
	}
	*ch = samsung_lowlevel_getc(samsung_defterm_base);
	return VMM_OK;
}

static int __init samsung_defterm_init(struct vmm_devtree_node *node)
{
	int rc;
	u32 *val;

	/* map this console device */
	rc = vmm_devtree_regmap(node, &samsung_defterm_base, 0);
	if (rc) {
		return rc;
	}

	/* retrieve clock frequency */
	rc = vmm_devtree_clock_frequency(node, &samsung_defterm_inclk);
	if (rc) {
		return rc;
	}
	
	/* retrieve baud rate */
	val = vmm_devtree_attrval(node, "baudrate");
	samsung_defterm_baud = (val) ? *val : 115200;

	/* initialize the console port */
	samsung_lowlevel_init(samsung_defterm_base,
			      samsung_defterm_baud,
			      samsung_defterm_inclk);

	return VMM_OK;
}

static struct defterm_ops samsung_ops = {
	.putc = samsung_defterm_putc,
	.getc = samsung_defterm_getc,
	.init = samsung_defterm_init
};

#else

static struct defterm_ops samsung_ops = {
	.putc = unknown_defterm_putc,
	.getc = unknown_defterm_getc,
	.init = unknown_defterm_init
};

#endif

static struct vmm_devtree_nodeid defterm_devid_table[] = {
{.type = "serial",.compatible = "arm,pl011",.data = &pl011_ops},
{.type = "serial",.compatible = "ns8250",.data = &uart8250_ops},
{.type = "serial",.compatible = "ns16450",.data = &uart8250_ops},
{.type = "serial",.compatible = "ns16550a",.data = &uart8250_ops},
{.type = "serial",.compatible = "ns16550",.data = &uart8250_ops},
{.type = "serial",.compatible = "ns16750",.data = &uart8250_ops},
{.type = "serial",.compatible = "ns16850",.data = &uart8250_ops},
{.type = "serial",.compatible = "st16654",.data = &omapuart_ops},
{.type = "serial",.compatible = "freescale",.data = &imx_ops},
{.type = "serial",.compatible = "imx-uart",.data = &imx_ops},
{.type = "serial",.compatible = "freescale,imx-uart",.data = &imx_ops},
{.type = "serial",.compatible = "samsung",.data = &samsung_ops},
{.type = "serial",.compatible = "exynos4210-uart",.data = &samsung_ops},
{.type = "serial",.compatible = "samsung,exynos4210-uart",.data = &samsung_ops},
{ /* end of list */ },
};

static const struct defterm_ops *ops = NULL;

int arch_defterm_putc(u8 ch)
{
	return (ops) ? ops->putc(ch) : unknown_defterm_putc(ch);
}

int arch_defterm_getc(u8 *ch)
{
	return (ops) ? ops->getc(ch) : unknown_defterm_getc(ch);
}

int __init arch_defterm_init(void)
{
	const char *attr;
	struct vmm_devtree_node *node;
	const struct vmm_devtree_nodeid *nodeid;

	/* Find choosen console node */
	node = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPARATOR_STRING
				   VMM_DEVTREE_CHOSEN_NODE_NAME);
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

	/* Find appropriate defterm ops */
	nodeid = vmm_devtree_match_node(defterm_devid_table, node);
	if (nodeid) {
		ops = nodeid->data;
	} else {
		return VMM_ENODEV;
	}

	return (ops) ? ops->init(node) : unknown_defterm_init(node);
}
