/**
 * Copyright (c) 2013 Jean-Christophe Dubois.
 * All rights reserved.
 *
 * Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWide.
 * Modified by Jimmy Durand Wesolowski <jimmy.durand-wesolowski@openwide.fr>
 * to allow a full UART initialization without the need of a bootloader.
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
 * @file imx-uart.c
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @brief source file for imx serial port driver.
 *
 * based on linux/drivers/tty/serial/imx.c
 *
 *  Driver for Motorola IMX serial ports
 *
 *  Based on drivers/char/serial.c, by Linus Torvalds, Theodore Ts'o.
 *
 *  Author: Sascha Hauer <sascha@saschahauer.de>
 *  Copyright (C) 2004 Pengutronix
 *
 *  Copyright (C) 2009 emlix GmbH
 *  Author: Fabian Godehardt (added IrDA support for iMX)
 *
 * The original source is licensed under GPL.
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_host_io.h>
#include <vmm_host_irq.h>
#include <vmm_modules.h>
#include <vmm_devtree.h>
#include <vmm_devdrv.h>
#include <libs/stringlib.h>
#include <libs/mathlib.h>
#include <drv/serial.h>
#include <drv/clk.h>
#include <drv/imx-uart.h>

#define MODULE_DESC                     "IMX Serial Driver"
#define MODULE_AUTHOR                   "Jean-Christophe Dubois"
#define MODULE_LICENSE                  "GPL"
#define MODULE_IPRIORITY                (SERIAL_IPRIORITY+1)
#define MODULE_INIT                     imx_driver_init
#define MODULE_EXIT                     imx_driver_exit

/*
 * This determines how often we check the modem status signals
 * for any change.  They generally aren't connected to an IRQ
 * so we have to poll them.  We also check immediately before
 * filling the TX fifo incase CTS has been dropped.
 */
#define MCTRL_TIMEOUT	(250*HZ/1000)

struct imx_port {
	struct serial *p;
	virtual_addr_t base;
	u32 baudrate;
	u32 input_clock;
	u32 irq;
	u16 mask;
};

bool imx_lowlevel_can_getc(virtual_addr_t base)
{
	u32 status = vmm_readl((void *)(base + IMX21_UTS));

	if (status & UTS_RXEMPTY) {
		return FALSE;
	} else {
		return TRUE;
	}
}

u8 imx_lowlevel_getc(virtual_addr_t base)
{
	u8 data;

	/* Wait until there is data in the FIFO */
	while (!imx_lowlevel_can_getc(base)) ;

	data = vmm_readl((void *)(base + URXD0));

	return data;
}

bool imx_lowlevel_can_putc(virtual_addr_t base)
{
	u32 status = vmm_readl((void *)(base + IMX21_UTS));

	if (status & UTS_TXFULL) {
		return FALSE;
	} else {
		return TRUE;
	}
}

void imx_lowlevel_putc(virtual_addr_t base, u8 ch)
{
	/* Wait until there is space in the FIFO */
	while (!imx_lowlevel_can_putc(base)) ;

	/* Send the character */
	vmm_writel(ch, (void *)(base + URTX0));
}

void imx_lowlevel_init(virtual_addr_t base, u32 baudrate, u32 input_clock)
{
	unsigned int temp = vmm_readl((void *)(base + UCR1));
	unsigned int divider;

	/* First, disable everything */
	temp &= ~UCR1_UARTEN;
	vmm_writel(temp, (void *)base + UCR1);

	/* disable all UCR2 related interrupts */
	temp = vmm_readl((void *)(base + UCR2));
	temp &= ~(UCR2_ATEN | UCR2_ESCI | UCR2_RTSEN);
	/* Set to 8N1 */
	temp = (temp & ~(UCR2_PREN | UCR2_STPB)) | UCR2_WS;
	/* Ignore RTS */
	temp |= UCR2_IRTS;
	vmm_writel(temp, (void *)(base + UCR2));

	/* disable all UCR3 related interrupts */
	temp = vmm_readl((void *)(base + UCR3));
	vmm_writel(temp &
		   ~(UCR3_RXDSEN | UCR3_DTREN | UCR3_FRAERREN | UCR3_TIMEOUTEN |
		     UCR3_AIRINTEN | UCR3_AWAKEN | UCR3_DTRDEN),
		   (void *)(base + UCR3));

	/* disable all UCR4 related interrupts */
	temp = vmm_readl((void *)(base + UCR4));
	vmm_writel(temp &
		   ~(UCR4_DREN | UCR4_TCEN | UCR4_ENIRI | UCR4_WKEN | UCR4_BKEN
		     | UCR4_OREN), (void *)(base + UCR4));

	/* trigger interrupt when there is 1 by in the RXFIFO */
	temp = vmm_readl((void *)(base + UFCR));
	vmm_writel((temp & 0xFFC0) | 1, (void *)(base + UFCR));

	/* Divide input clock by 2 */
	temp = vmm_readl((void *)(base + UFCR)) & ~UFCR_RFDIV;
	vmm_writel(temp | UFCR_RFDIV_REG(2), (void *)(base + UFCR));
	input_clock /= 2;

	divider = udiv32(baudrate, 100) - 1;
	vmm_writel(divider, (void *)(base + UBIR));
	/* UBMR = Ref Freq / (16 * baudrate) * (UBIR + 1) - 1 */
	/* As UBIR = baudrate / 100 - 1, UBMR = Ref Freq / (16 * 100) - 1 */
	temp = udiv32(input_clock, 16 * 100) - 1;
	vmm_writel(temp, (void *)(base + UBMR));

	/* enable the UART */
	temp = UCR1_UARTEN;
	vmm_writel(temp, (void *)(base + UCR1));

	/* Enable FIFOs */
	temp = vmm_readl((void *)(base + UCR2));
	vmm_writel(temp | UCR2_SRST | UCR2_RXEN | UCR2_TXEN,
		   (void *)(base + UCR2));
}

static void imx_txint(struct imx_port *port)
{
	port->mask &= ~UCR1_TRDYEN;
	vmm_writel(port->mask, (void *)port->base + UCR1);
}

static void imx_rxint(struct imx_port *port)
{
	while (imx_lowlevel_can_getc(port->base)) {
		u8 ch = imx_lowlevel_getc(port->base);
		serial_rx(port->p, &ch, 1);
	}
}

static void imx_rtsint(struct imx_port *port)
{
}

static vmm_irq_return_t imx_irq_handler(int irq, void *dev_id)
{
	struct imx_port *port = dev_id;
	unsigned int sts;

	sts = vmm_readl((void *)port->base + USR1);

	if (sts & USR1_RRDY) {
		imx_rxint(port);
	}

	if ((sts & USR1_TRDY) && (port->mask & UCR1_TXMPTYEN)) {
		imx_txint(port);
	}

	if (sts & USR1_RTSD) {
		imx_rtsint(port);
	}

	sts &= USR1_PARITYERR |
		USR1_RTSD |
		USR1_ESCF |
		USR1_FRAMERR |
		USR1_TIMEOUT |
		USR1_AIRINT |
		USR1_AWAKE;
	if (sts) {
		vmm_writel(sts, (void *)port->base + USR1);
	}

	return VMM_IRQ_HANDLED;
}

static u32 imx_tx(struct serial *p, u8 *src, size_t len)
{
	u32 i;
	struct imx_port *port = serial_tx_priv(p);

	for (i = 0; i < len; i++) {
		if (!imx_lowlevel_can_putc(port->base)) {
			break;
		}
		imx_lowlevel_putc(port->base, src[i]);
	}

	return i;
}

static int imx_driver_probe(struct vmm_device *dev,
			    const struct vmm_devtree_nodeid *devid)
{
	int rc = VMM_EFAIL;
	struct clk *clk_ipg = NULL;
	struct clk *clk_uart = NULL;
	struct imx_port *port = NULL;
	unsigned long old_rate = 0;

	port = vmm_zalloc(sizeof(struct imx_port));
	if (!port) {
		rc = VMM_ENOMEM;
		goto free_nothing;
	}

	rc = vmm_devtree_request_regmap(dev->of_node, &port->base, 0,
					"iMX UART");
	if (rc) {
		goto free_port;
	}

	if (vmm_devtree_read_u32(dev->of_node, "baudrate", &port->baudrate)) {
		port->baudrate = 115200;
	}

	rc = vmm_devtree_clock_frequency(dev->of_node, &port->input_clock);
	if (rc) {
		goto free_reg;
	}

	/* Setup clocks */
	clk_ipg = of_clk_get(dev->of_node, 0);
	clk_uart = of_clk_get(dev->of_node, 1);
	if (!VMM_IS_ERR_OR_NULL(clk_ipg)) {
		rc = clk_prepare_enable(clk_ipg);
		if (rc) {
			goto free_reg;
		}
	}
	if (!VMM_IS_ERR_OR_NULL(clk_uart)) {
		rc = clk_prepare_enable(clk_uart);
		if (rc) {
			goto clk_disable_unprepare_ipg;
		}
		old_rate = clk_get_rate(clk_uart);
		if (clk_set_rate(clk_uart, port->input_clock)) {
			vmm_printf("Could not set %s clock rate to %"PRId32" Hz, "
				   "actual rate: %lu Hz\n", __clk_get_name(clk_uart),
				   port->input_clock, clk_get_rate(clk_uart));
			rc = VMM_ERANGE;
			goto clk_disable_unprepare_uart;
		}
	}

	/* Register interrupt handler */
	port->irq = vmm_devtree_irq_parse_map(dev->of_node, 0);
	if (!port->irq) {
		rc = VMM_ENODEV;
		goto clk_old_rate;
	}
	if ((rc = vmm_host_irq_register(port->irq, dev->name,
					imx_irq_handler, port))) {
		goto clk_old_rate;
	}

	/* Call low-level init function */
	imx_lowlevel_init(port->base, port->baudrate, port->input_clock);

	/* Create Serial Port */
	port->p = serial_create(dev, 256, imx_tx, port);
	if (VMM_IS_ERR_OR_NULL(port->p)) {
		rc = VMM_PTR_ERR(port->p);
		goto free_irq;
	}

	/* Save port pointer */
	dev->priv = port;

	/* Unmask Rx, Mask Tx, and Enable UART */
	port->mask = vmm_readl((void *)port->base + UCR1);
	port->mask |= (UCR1_RRDYEN | UCR1_UARTEN);
	port->mask &= ~(UCR1_TRDYEN);
	vmm_writel(port->mask, (void *)port->base + UCR1);

	return rc;

free_irq:
	vmm_host_irq_unregister(port->irq, port);
clk_old_rate:
	if (!VMM_IS_ERR_OR_NULL(clk_uart)) {
		if (old_rate) {
			clk_set_rate(clk_uart, old_rate);
		}
	}
clk_disable_unprepare_uart:
	if (!VMM_IS_ERR_OR_NULL(clk_uart)) {
		clk_disable_unprepare(clk_uart);
	}
clk_disable_unprepare_ipg:
	if (!VMM_IS_ERR_OR_NULL(clk_ipg)) {
		clk_disable_unprepare(clk_ipg);
	}
free_reg:
	vmm_devtree_regunmap_release(dev->of_node, port->base, 0);
free_port:
	vmm_free(port);
free_nothing:
	return rc;
}

static int imx_driver_remove(struct vmm_device *dev)
{
	int rc = VMM_OK;
	struct imx_port *port = dev->priv;

	if (!port) {
		return VMM_OK;
	}

	serial_destroy(port->p);
	vmm_host_irq_unregister(port->irq, port);
	vmm_devtree_regunmap_release(dev->of_node, port->base, 0);
	vmm_free(port);
	dev->priv = NULL;

	return rc;
}

static struct vmm_devtree_nodeid imx_devid_table[] = {
	{.compatible = "freescale,imx-uart"},
	{ /* end of list */ },
};

static struct vmm_driver imx_driver = {
	.name = "imx_serial",
	.match_table = imx_devid_table,
	.probe = imx_driver_probe,
	.remove = imx_driver_remove,
};

static int __init imx_driver_init(void)
{
	return vmm_devdrv_register_driver(&imx_driver);
}

static void __exit imx_driver_exit(void)
{
	vmm_devdrv_unregister_driver(&imx_driver);
}

VMM_DECLARE_MODULE(MODULE_DESC,
		   MODULE_AUTHOR,
		   MODULE_LICENSE,
		   MODULE_IPRIORITY,
		   MODULE_INIT,
		   MODULE_EXIT);
