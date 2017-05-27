/**
 * Copyright (c) 2016 Bhargav Shah.
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
 * @file zynq-uart.c
 * @author Bhargav Shah (bhargavshah1988@gmail.com)
 * @brief source file for zynq uart driver.
 *
 * Adapted from <u-boot>/drivers/serial/serial_zynq.c
 *
 * Copyright (C) 2012 Michal Simek <monstr@monstr.eu>
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
#include <drv/serial/zynq-uart.h>

#define MODULE_DESC			"Zynq uart driver"
#define MODULE_AUTHOR			"Bhargav Shah"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(SERIAL_IPRIORITY+1)
#define MODULE_INIT			zynq_uart_driver_init
#define MODULE_EXIT			zynq_uart_driver_exit

#define ZYNQ_UART_FIFO_TRIGGER 56
#define ZYNQ_UART_FIFO_TOUT    10

bool zynq_uart_lowlevel_can_getc(struct uart_zynq *regs)
{
	if ((vmm_readl((void*)&regs->channel_sts) & ZYNQ_UART_SR_RXEMPTY))
		return FALSE;

	return TRUE;
}

u8 zynq_uart_lowlevel_getc(struct uart_zynq *regs)
{
	/* Wait until there is no data in the FIFO */
	while (!zynq_uart_lowlevel_can_getc(regs)) ;

	/* Read IO register */
	return (char)vmm_readl((void *)(&regs->tx_rx_fifo));
}

bool zynq_uart_lowlevel_can_putc(struct uart_zynq *reg)
{
	if (!(vmm_readl(&reg->channel_sts) & ZYNQ_UART_SR_TXEMPTY))
		return FALSE;

	return TRUE;
}

void zynq_uart_lowlevel_putc(struct uart_zynq *reg, u8 ch)
{
	/* Wait until there is data in the FIFO */
	while (!zynq_uart_lowlevel_can_putc(reg)) ;

	/* Send the character */
	vmm_writel(ch, (void *)&reg->tx_rx_fifo);
}

static u32 zynq_uart_tx(struct serial *p, u8 *src, size_t len)
{
	u32 i;
	struct zynq_uart_priv *port = serial_tx_priv(p);

	for (i = 0; i < len; i++) {
		if (!zynq_uart_lowlevel_can_putc(port->regs)) {
			break;
		}
		zynq_uart_lowlevel_putc(port->regs, src[i]);
	}

	return i;
}

/* Set up the baud rate*/
static void zynq_uart_setbrg(struct zynq_uart_priv *port)
{
	/* Calculation results. */
	unsigned int calc_bauderror, bdiv, bgen;
	unsigned long calc_baud = 0;
	struct uart_zynq *regs = port->regs;

	/* Covering case where input clock is so slow */
	if (port->input_clock < 1000000 && port->baudrate > 4800)
		port->baudrate = 4800;

	/*		  master clock
	 * Baud rate = ------------------
	 *		bgen * (bdiv + 1)
	 *
	 * Find acceptable values for baud generation.
	 */
	for (bdiv = 4; bdiv < 255; bdiv++) {
		bgen = udiv32(port->input_clock,
			      (port->baudrate * (bdiv + 1)));
		if (bgen < 2 || bgen > 65535)
			continue;

		calc_baud = udiv32(port->input_clock, (bgen * (bdiv + 1)));

		/*
		 * Use first calculated baudrate with
		 * an acceptable (<3%) error
		 */
		if (port->baudrate > calc_baud)
			calc_bauderror = port->baudrate - calc_baud;
		else
			calc_bauderror = calc_baud - port->baudrate;
		if (udiv32((calc_bauderror * 100), port->baudrate) < 3)
			break;
	}

	vmm_writel(bdiv, &regs->baud_rate_divider);
	vmm_writel(bgen, &regs->baud_rate_gen);
}

void zynq_uart_lowlevel_init(struct zynq_uart_priv *port)
{
	struct uart_zynq *regs = port->regs;

	/* RX/TX enabled & reset */
	vmm_writel(ZYNQ_UART_CR_TX_EN | \
		   ZYNQ_UART_CR_RX_EN | \
		   ZYNQ_UART_CR_TXRST | \
		   ZYNQ_UART_CR_RXRST,
		   &regs->control);
	/* 8 bit, no parity */
	vmm_writel(ZYNQ_UART_MR_PARITY_NONE, &regs->mode);

	/* Set baud rate here */
	zynq_uart_setbrg(port);
}

static vmm_irq_return_t zynq_uart_irq_handler(int irq_no, void *pdev)
{
	u8 ch;
	u32 status;
	struct zynq_uart_priv *port = (struct zynq_uart_priv *)pdev;
	struct uart_zynq *regs = port->regs;

	/* Get interrupt status */
	status = vmm_readl((void *)&regs->isr);

	/* Handle RX interrupt */
	if (status & (ZYNQ_UART_ISR_RX_TOUT | ZYNQ_UART_ISR_RX)) {
		/* Pull-out bytes from RX FIFO */
		while (zynq_uart_lowlevel_can_getc(port->regs)) {
			ch = zynq_uart_lowlevel_getc(port->regs);
			serial_rx(port->p, &ch, 1);
		}
	}

	/* Clear Interupt status */
	vmm_writel(status, &regs->isr);

	return VMM_IRQ_HANDLED;
}

static int zynq_uart_driver_probe(struct vmm_device *dev,
				  const struct vmm_devtree_nodeid *devid)
{
	int rc;
	struct zynq_uart_priv *port;

	port = vmm_zalloc(sizeof(struct zynq_uart_priv));
	if (!port) {
		rc = VMM_ENOMEM;
		goto free_nothing;
	}

	rc = vmm_devtree_request_regmap(dev->of_node,
					(virtual_addr_t*)&port->regs,
					0, "Zynq UART");
	if (rc) {
		goto free_port;
	}

	if (vmm_devtree_read_u32(dev->of_node, "baudrate",
				 &port->baudrate)) {
		port->baudrate = 115200;
	}

	rc = vmm_devtree_clock_frequency(dev->of_node, &port->input_clock);
	if (rc) {
		goto free_reg;
	}

	port->irq = vmm_devtree_irq_parse_map(dev->of_node, 0);
	if (!port->irq) {
		rc = VMM_ENODEV;
		goto free_reg;
	}
	if ((rc = vmm_host_irq_register(port->irq, dev->name,
					zynq_uart_irq_handler, port))) {
		goto free_reg;
	}

	/* Call low-level init function */
	zynq_uart_lowlevel_init(port);

	/* Create Serial Port */
	port->p = serial_create(dev, 256, zynq_uart_tx, port);
	if (VMM_IS_ERR_OR_NULL(port->p)) {
		rc = VMM_PTR_ERR(port->p);
		goto free_irq;
	}

	/* set rx fifo trigger */
	vmm_writel(ZYNQ_UART_FIFO_TRIGGER, &port->regs->rxtrig);

	/* configure rx fifo timeout */
	vmm_writel(ZYNQ_UART_FIFO_TOUT, &port->regs->rx_tout);

	/* Save port pointer */
	dev->priv = port;

	/* clear all interrupts */
	vmm_writel(vmm_readl((void *)&port->regs->isr), &port->regs->isr);

	/* Unmask Rx and timeout Interrupt */
	port->mask |= ZYNQ_UART_RX_ISR_EN | ZYNQ_UART_RX_ISR_TO_EN;
	vmm_writel(port->mask, (void *)&port->regs->ie);

	return VMM_OK;

free_irq:
	vmm_host_irq_unregister(port->irq, port);
free_reg:
	vmm_devtree_regunmap_release(dev->of_node,
				     (virtual_addr_t)port->regs, 0);
free_port:
	vmm_free(port);
free_nothing:
	return rc;
}

static int zynq_serial_driver_remove(struct vmm_device *dev)
{
	struct zynq_uart_priv *port = dev->priv;

	if (!port) {
		return VMM_OK;
	}

	/* Mask RX interrupts */
	port->mask &= 0x00000000;
	vmm_writel(port->mask, (void *)&port->regs->ie);

	/* Free-up resources */
	serial_destroy(port->p);
	vmm_host_irq_unregister(port->irq, port);
	vmm_devtree_regunmap_release(dev->of_node,
				     (virtual_addr_t)port->regs, 0);
	vmm_free(port);
	dev->priv = NULL;

	return VMM_OK;
}

static struct vmm_devtree_nodeid zynq_serial_devid_table[] = {
	{ .compatible = "cdns,uart-r1p12" },
	{ .compatible = "cdns,uart-r1p8" },
	{ .compatible = "xlnx,xuartps" },
	{ /* end of list */ },
};

static struct vmm_driver zynq_serial_driver = {
	.name = "zynq_serial",
	.match_table = zynq_serial_devid_table,
	.probe = zynq_uart_driver_probe,
	.remove = zynq_serial_driver_remove,
};

static int __init zynq_uart_driver_init(void)
{
	return vmm_devdrv_register_driver(&zynq_serial_driver);
}

static void __exit zynq_uart_driver_exit(void)
{
	vmm_devdrv_unregister_driver(&zynq_serial_driver);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
