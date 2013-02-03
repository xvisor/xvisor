/**
 * Copyright (c) 2011 Anup Patel.
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
 * @file pl011.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief source file for PrimeCell PL011 serial port driver.
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_host_io.h>
#include <vmm_host_irq.h>
#include <vmm_completion.h>
#include <vmm_modules.h>
#include <vmm_devtree.h>
#include <vmm_devdrv.h>
#include <vmm_chardev.h>
#include <libs/stringlib.h>
#include <libs/mathlib.h>
#include <drv/pl011.h>

/* Enable UART_PL011_USE_TXINTR to use TX interrupt.
 * Generally the FIFOs are small so its better to poll on Tx 
 * for smoother vmm_prints.
 */
#undef UART_PL011_USE_TXINTR

#define MODULE_DESC			"PL011 Serial Driver"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			pl011_driver_init
#define	MODULE_EXIT			pl011_driver_exit

bool pl011_lowlevel_can_getc(virtual_addr_t base)
{
	if (vmm_in_8((void *)(base + UART_PL011_FR)) & UART_PL011_FR_RXFE) {
		return FALSE;
	}
	return TRUE;
}

u8 pl011_lowlevel_getc(virtual_addr_t base)
{
	u16 data;

	/* Wait until there is data in the FIFO */
	while (!pl011_lowlevel_can_getc(base)) ;

	data = vmm_in_le16((void *)(base + UART_PL011_DR));

	/* Check for an error flag */
	if (data & 0x0F00) {
		/* Clear the error */
		vmm_out_8((void *)(base + UART_PL011_ECR), 0);
		return -1;
	}

	return (char)(data & 0xFF);
}

bool pl011_lowlevel_can_putc(virtual_addr_t base)
{
	if (vmm_in_8((void *)(base + UART_PL011_FR)) & UART_PL011_FR_TXFF) {
		return FALSE;
	}
	return TRUE;
}

void pl011_lowlevel_putc(virtual_addr_t base, u8 ch)
{
	/* Wait until there is space in the FIFO */
	while (!pl011_lowlevel_can_putc(base)) ;

	/* Send the character */
	vmm_out_8((void *)(base + UART_PL011_DR), ch);
}

void pl011_lowlevel_init(virtual_addr_t base, u32 baudrate, u32 input_clock)
{
	unsigned int divider;
	unsigned int temp;
	unsigned int remainder;
	unsigned int fraction;

	/* First, disable everything */
	vmm_out_le16((void *)(base + UART_PL011_CR), 0);

	/*
	 * Set baud rate
	 *
	 * IBRD = UART_CLK / (16 * BAUD_RATE)
	 * FBRD = RND((64 * MOD(UART_CLK,(16 * BAUD_RATE))) 
	 *        / (16 * BAUD_RATE))
	 */
	temp = 16 * baudrate;
	divider = udiv32(input_clock, temp);
	remainder = umod32(input_clock, temp);
	temp = udiv32((8 * remainder), baudrate);
	fraction = (temp >> 1) + (temp & 1);

	vmm_out_le16((void *)(base + UART_PL011_IBRD), (u16) divider);
	vmm_out_8((void *)(base + UART_PL011_FBRD), (u8) fraction);

	/* Set the UART to be 8 bits, 1 stop bit, 
	 * no parity, fifo enabled 
	 */
	vmm_out_8((void *)(base + UART_PL011_LCRH),
		  UART_PL011_LCRH_WLEN_8 | UART_PL011_LCRH_FEN);

	/* Ensure RX FIFO not empty triggered when 
	 * RX FIFO becomes 1/8 full
	 */
	temp = vmm_in_8((void *)(base + UART_PL011_IFLS));
	temp &= ~UART_PL011_IFLS_RXIFL_MASK;
	vmm_out_8((void *)(base + UART_PL011_IFLS), (u8) temp);

	/* Finally, enable the UART */
	vmm_out_le16((void *)(base + UART_PL011_CR),
		     UART_PL011_CR_UARTEN | UART_PL011_CR_TXE |
		     UART_PL011_CR_RXE);
}

struct pl011_port {
	struct vmm_chardev cd;
	struct vmm_completion read_possible;
	struct vmm_completion write_possible;
	virtual_addr_t base;
	u32 baudrate;
	u32 input_clock;
	u32 irq;
	u16 mask;
};

static vmm_irq_return_t pl011_irq_handler(u32 irq_no, 
					  arch_regs_t * regs, 
					  void *dev)
{
	u16 data;
	struct pl011_port *port = (struct pl011_port *)dev;

	/* Get masked interrupt status */
	data = vmm_in_le16((void *)(port->base + UART_PL011_MIS));

	/* handle RX FIFO not empty */
	if (data & (UART_PL011_MIS_RXMIS | UART_PL011_MIS_RTMIS)) {
		/* Mask RX interrupts till RX FIFO is empty */
		port->mask &= ~(UART_PL011_IMSC_RXIM | UART_PL011_IMSC_RTIM);
		vmm_out_le16((void *)(port->base + UART_PL011_IMSC), port->mask);
		/* Signal work completion sleeping thread */
		vmm_completion_complete(&port->read_possible);
	}

	/* handle TX FIFO not full */
	if (data & UART_PL011_MIS_TXMIS) {
		/* Mask TX interrupts till TX FIFO is full */
		port->mask &= ~UART_PL011_IMSC_TXIM;
		vmm_out_le16((void *)(port->base + UART_PL011_IMSC), port->mask);
		/* Signal work completion to sleeping thread */
		vmm_completion_complete(&port->write_possible);
	}

	/* Clear all interrupts */
	vmm_out_le16((void *)(port->base + UART_PL011_ICR), data);

	return VMM_IRQ_HANDLED;
}

static u8 pl011_getc_sleepable(struct pl011_port *port)
{
	/* Wait until there is data in the FIFO */
	while (!pl011_lowlevel_can_getc(port->base)) {
		/* Enable the RX interrupt */
		port->mask |= (UART_PL011_IMSC_RXIM | UART_PL011_IMSC_RTIM);
		vmm_out_le16((void *)(port->base + UART_PL011_IMSC),
			     port->mask);

		/* Wait for completion */
		vmm_completion_wait(&port->read_possible);
	}

	/* Read data to destination */
	return pl011_lowlevel_getc(port->base);
}

static u32 pl011_read(struct vmm_chardev *cdev,
		      u8 * dest, u32 len, bool sleep)
{
	u32 i;
	struct pl011_port *port;

	if (!(cdev && dest && cdev->priv)) {
		return 0;
	}

	port = cdev->priv;

	if (sleep) {
		for (i = 0; i < len; i++) {
			dest[i] = pl011_getc_sleepable(port);
		}
	} else {
		for (i = 0; i < len; i++) {
			if (!pl011_lowlevel_can_getc(port->base)) {
				break;
			}
			dest[i] = pl011_lowlevel_getc(port->base);
		}
	}

	return i;
}

#if defined(UART_PL011_USE_TXINTR)
static void pl011_putc_sleepable(struct pl011_port *port, u8 ch)
{
	/* Wait until there is space in the FIFO */
	if (!pl011_lowlevel_can_putc(port->base)) {
		/* Enable the TX interrupt */
		port->mask |= UART_PL011_IMSC_TXIM;
		vmm_out_le16((void *)(port->base + UART_PL011_IMSC),
			     port->mask);

		/* Wait for completion */
		vmm_completion_wait(&port->write_possible);
	}

	/* Write data to FIFO */
	vmm_out_8((void *)(port->base + UART_PL011_DR), ch);
}
#endif

static u32 pl011_write(struct vmm_chardev *cdev,
		       u8 * src, u32 len, bool sleep)
{
	u32 i;
	struct pl011_port *port;

	if (!(cdev && src && cdev->priv)) {
		return 0;
	}

	port = cdev->priv;

#if defined(UART_PL011_USE_TXINTR)
	if (sleep) {
		for (i = 0; i < len; i++) {
			pl011_putc_sleepable(port, src[i]);
		}
	} else {
		for (i = 0; i < len; i++) {
			if (!pl011_lowlevel_can_putc(port->base)) {
				break;
			}
			pl011_lowlevel_putc(port->base, src[i]);
		}
	}
#else
	for (i = 0; i < len; i++) {
		if (!pl011_lowlevel_can_putc(port->base)) {
			break;
		}
		pl011_lowlevel_putc(port->base, src[i]);
	}
#endif

	return i;
}

static int pl011_driver_probe(struct vmm_device *dev,
			      const struct vmm_devid *devid)
{
	int rc;
	const char *attr;
	struct pl011_port *port;

	port = vmm_zalloc(sizeof(struct pl011_port));
	if (!port) {
		rc = VMM_ENOMEM;
		goto free_nothing;
	}

	strcpy(port->cd.name, dev->node->name);
	port->cd.dev = dev;
	port->cd.ioctl = NULL;
	port->cd.read = pl011_read;
	port->cd.write = pl011_write;
	port->cd.priv = port;

	INIT_COMPLETION(&port->read_possible);
	INIT_COMPLETION(&port->write_possible);

	rc = vmm_devtree_regmap(dev->node, &port->base, 0);
	if (rc) {
		goto free_port;
	}

	attr = vmm_devtree_attrval(dev->node, "baudrate");
	if (!attr) {
		rc = VMM_EFAIL;
		goto free_reg;
	}
	port->baudrate = *((u32 *) attr);
	port->input_clock = vmm_devdrv_clock_get_rate(dev);

	attr = vmm_devtree_attrval(dev->node, "irq");
	if (!attr) {
		rc = VMM_EFAIL;
		goto free_reg;
	}
	port->irq = *((u32 *) attr);
	if ((rc = vmm_host_irq_register(port->irq, dev->node->name, 
					pl011_irq_handler, port))) {
		goto free_reg;
	}

	/* Call low-level init function */
	pl011_lowlevel_init(port->base, port->baudrate, port->input_clock);

	rc = vmm_chardev_register(&port->cd);
	if (rc) {
		goto free_irq;
	}

	dev->priv = port;

	return VMM_OK;

free_irq:
	vmm_host_irq_unregister(port->irq, port);
free_reg:
	vmm_devtree_regunmap(dev->node, port->base, 0);
free_port:
	vmm_free(port);
free_nothing:
	return rc;
}

static int pl011_driver_remove(struct vmm_device *dev)
{
	struct pl011_port *port = dev->priv;

	if (port) {
		vmm_chardev_unregister(&port->cd);
		vmm_host_irq_unregister(port->irq, port);
		vmm_devtree_regunmap(dev->node, port->base, 0);
		vmm_free(port);
		dev->priv = NULL;
	}

	return VMM_OK;
}

static struct vmm_devid pl011_devid_table[] = {
	{.type = "serial",.compatible = "arm,pl011"},
	{ /* end of list */ },
};

static struct vmm_driver pl011_driver = {
	.name = "pl011_serial",
	.match_table = pl011_devid_table,
	.probe = pl011_driver_probe,
	.remove = pl011_driver_remove,
};

static int __init pl011_driver_init(void)
{
	return vmm_devdrv_register_driver(&pl011_driver);
}

static void __exit pl011_driver_exit(void)
{
	vmm_devdrv_unregister_driver(&pl011_driver);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
