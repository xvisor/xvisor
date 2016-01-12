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
 * @file samsung-uart.c
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @brief source file for Samsung serial port driver.
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
#include <drv/samsung-uart.h>

#define MODULE_DESC			"Samsung Serial Driver"
#define MODULE_AUTHOR			"Jean-Christophe Dubois"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(SERIAL_IPRIORITY+1)
#define	MODULE_INIT			samsung_driver_init
#define	MODULE_EXIT			samsung_driver_exit

bool samsung_lowlevel_can_getc(virtual_addr_t base)
{
	u32 ufstat = vmm_in_le32((void *)(base + S3C2410_UFSTAT));

	if (ufstat & (S5PV210_UFSTAT_RXFULL | S5PV210_UFSTAT_RXMASK)) {
		return TRUE;
	} else {
		return FALSE;
	}
}

u8 samsung_lowlevel_getc(virtual_addr_t base)
{
	u8 data;

	/* Wait until there is data in the FIFO */
	while (!samsung_lowlevel_can_getc(base)) ;

	data = vmm_in_8((void *)(base + S3C2410_URXH));

	return data;
}

bool samsung_lowlevel_can_putc(virtual_addr_t base)
{
	u32 ufcon = vmm_in_le32((void *)(base + S3C2410_UFCON));

	if (ufcon & S3C2410_UFCON_FIFOMODE) {
		u32 ufstat = vmm_in_le32((void *)(base + S3C2410_UFSTAT));

		return (ufstat & S5PV210_UFSTAT_TXFULL) ? FALSE : TRUE;
	} else {
		u32 utrstat = vmm_in_le32((void *)(base + S3C2410_UTRSTAT));

		return (utrstat & S3C2410_UTRSTAT_TXE) ? TRUE : FALSE;
	}
}

void samsung_lowlevel_putc(virtual_addr_t base, u8 ch)
{
	/* Wait until there is space in the FIFO */
	while (!samsung_lowlevel_can_putc(base)) ;

	/* Send the character */
	vmm_out_8((void *)(base + S3C2410_UTXH), ch);
}

void samsung_lowlevel_init(virtual_addr_t base, u32 baudrate, u32 input_clock)
{
	unsigned int divider;
	unsigned int temp;
	unsigned int remainder;

	/* First, disable everything */
	vmm_out_le16((void *)(base + S3C2410_UCON), 0);

	/*
	 * Set baud rate
	 *
	 * UBRDIV  = (UART_CLK / (16 * BAUD_RATE)) - 1
	 * DIVSLOT = MOD(UART_CLK / BAUD_RATE, 16)
	 */
	temp = udiv32(input_clock, baudrate);
	divider =  udiv32(temp, 16) - 1;
	remainder = umod32(temp, 16);

	vmm_out_le16((void *)(base + S3C2410_UBRDIV), (u16)
		     divider);
	vmm_out_8((void *)(base + S3C2443_DIVSLOT), (u8)
		  remainder);

	/* Set the UART to be 8 bits, 1 stop bit, no parity */
	vmm_out_le32((void *)(base + S3C2410_ULCON),
		     S3C2410_LCON_CS8 | S3C2410_LCON_PNONE);

	/* enable FIFO, set RX and TX trigger */
	vmm_out_le32((void *)(base + S3C2410_UFCON), S3C2410_UFCON_DEFAULT);

	/* enable the UART */
	vmm_out_le32((void *)(base + S3C2410_UCON), S3C2410_UCON_DEFAULT);
}

struct samsung_port {
	struct serial *p;
	virtual_addr_t base;
	u32 baudrate;
	u32 input_clock;
	u32 irq;
	u16 mask;
};

static vmm_irq_return_t samsung_irq_handler(int irq_no, void *dev)
{
	u16 data;
	struct samsung_port *port = (struct samsung_port *)dev;

	/* Get masked interrupt status */
	data = vmm_in_le16((void *)(port->base + S3C64XX_UINTP));

	/* handle RX FIFO not empty */
	if (data & S3C64XX_UINTM_RXD_MSK) {
		/* Mask RX interrupts till RX FIFO is empty */
		while (samsung_lowlevel_can_getc(port->base)) {
			u8 ch = samsung_lowlevel_getc(port->base);
			serial_rx(port->p, &ch, 1);
		}
	}

	/* Clear all interrupts */
	vmm_out_le16((void *)(port->base + S3C64XX_UINTP), data);

	return VMM_IRQ_HANDLED;
}

static u32 samsung_tx(struct serial *p, u8 *src, size_t len)
{
	u32 i;
	struct samsung_port *port = serial_tx_priv(p);

	for (i = 0; i < len; i++) {
		if (!samsung_lowlevel_can_putc(port->base)) {
			break;
		}
		samsung_lowlevel_putc(port->base, src[i]);
	}

	return i;
}

static int samsung_driver_probe(struct vmm_device *dev,
				const struct vmm_devtree_nodeid *devid)
{
	u32 ucon;
	int rc = VMM_EFAIL;
	struct samsung_port *port = NULL;

	port = vmm_zalloc(sizeof(struct samsung_port));
	if (!port) {
		rc = VMM_ENOMEM;
		goto free_nothing;
	}

	rc = vmm_devtree_request_regmap(dev->of_node, &port->base, 0,
					"Samsung UART");
	if (rc) {
		goto free_port;
	}

	/* Make sure all interrupts except Rx are masked. */
	port->mask = S3C64XX_UINTM_RXD_MSK;
	port->mask = ~port->mask;
	vmm_out_le16((void *)(port->base + S3C64XX_UINTM), port->mask);

	if (vmm_devtree_read_u32(dev->of_node, "baudrate",
				 &port->baudrate)) {
		port->baudrate = 115200;
	}

	rc = vmm_devtree_clock_frequency(dev->of_node, &port->input_clock);
	if (rc) {
		goto free_reg;
	}

	/* Setup interrupt handler */
	port->irq = vmm_devtree_irq_parse_map(dev->of_node, 0);
	if (!port->irq) {
		rc = VMM_ENODEV;
		goto free_reg;
	}
	if ((rc = vmm_host_irq_register(port->irq, dev->name,
					samsung_irq_handler, port))) {
		goto free_reg;
	}

	/* Call low-level init function */
	samsung_lowlevel_init(port->base, port->baudrate, port->input_clock);

	/* Create Serial Port */
	port->p = serial_create(dev, 256, samsung_tx, port);
	if (VMM_IS_ERR_OR_NULL(port->p)) {
		rc = VMM_PTR_ERR(port->p);
		goto free_irq;
	}

	/* Save port pointer */
	dev->priv = port;

	/* Unmask RX interrupt */
	ucon = vmm_in_le32((void *)(port->base + S3C2410_UCON));
	ucon |= S3C2410_UCON_RXIRQMODE;
	vmm_out_le32((void *)(port->base + S3C2410_UCON), ucon);

	return VMM_OK;

 free_irq:
	vmm_host_irq_unregister(port->irq, port);
 free_reg:
	vmm_devtree_regunmap_release(dev->of_node, port->base, 0);
 free_port:
	vmm_free(port);
 free_nothing:
	return rc;
}

static int samsung_driver_remove(struct vmm_device *dev)
{
	struct samsung_port *port = dev->priv;

	if (!port) {
		return VMM_OK;
	}

	serial_destroy(port->p);
	vmm_host_irq_unregister(port->irq, port);
	vmm_devtree_regunmap_release(dev->of_node, port->base, 0);
	vmm_free(port);
	dev->priv = NULL;

	return VMM_OK;
}

static struct vmm_devtree_nodeid samsung_devid_table[] = {
	{ .compatible = "samsung,exynos4210-uart" },
	{ /* end of list */ },
};

static struct vmm_driver samsung_driver = {
	.name = "samsung_serial",
	.match_table = samsung_devid_table,
	.probe = samsung_driver_probe,
	.remove = samsung_driver_remove,
};

static int __init samsung_driver_init(void)
{
	return vmm_devdrv_register_driver(&samsung_driver);
}

static void __exit samsung_driver_exit(void)
{
	vmm_devdrv_unregister_driver(&samsung_driver);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
