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
 * @file pl01x.c
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief source file for PrimeCell PL011/PL010 serial port driver.
 */

#include <arch_math.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_string.h>
#include <vmm_host_io.h>
#include <vmm_host_irq.h>
#include <vmm_scheduler.h>
#include <vmm_completion.h>
#include <vmm_modules.h>
#include <vmm_devtree.h>
#include <vmm_devdrv.h>
#include <vmm_chardev.h>
#include <serial/pl01x.h>

#define MODULE_VARID			pl01x_driver_module
#define MODULE_NAME			"PL011/PL010 Serial Driver"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			pl01x_driver_init
#define	MODULE_EXIT			pl01x_driver_exit

bool pl01x_lowlevel_can_getc(virtual_addr_t base, u32 type)
{
	if(vmm_readl((void*)(base + UART_PL01x_FR)) & UART_PL01x_FR_RXFE) {
		return FALSE;
	}
	return TRUE;
}

u8 pl01x_lowlevel_getc(virtual_addr_t base, u32 type)
{
	unsigned int data;

	/* Wait until there is data in the FIFO */
	while (pl01x_lowlevel_can_getc(base, type) != TRUE);

	data = vmm_readl((void*)(base + UART_PL01x_DR));

	/* Check for an error flag */
	if (data & 0xFFFFFF00) {
		/* Clear the error */
		vmm_writel(0xFFFFFFFF, (void*)(base + UART_PL01x_ECR));
		return -1;
	}

	return (char)data;
}

bool pl01x_lowlevel_can_putc(virtual_addr_t base, u32 type)
{
	if(vmm_readl((void*)(base + UART_PL01x_FR)) & UART_PL01x_FR_TXFF) {
		return FALSE;
	}
	return TRUE;
}

void pl01x_lowlevel_putc(virtual_addr_t base, u32 type, u8 ch)
{
	/* Wait until there is space in the FIFO */
	while (pl01x_lowlevel_can_putc(base, type) != TRUE);

	/* Send the character */
	vmm_writel(ch, (void*)(base + UART_PL01x_DR));
}

void pl01x_lowlevel_init(virtual_addr_t base, u32 type, 
						u32 baudrate, u32 input_clock)
{
	unsigned int divider;
	unsigned int temp;
	unsigned int remainder;
	unsigned int fraction;

	if(type==PL01X_TYPE_1) {
		/* First, disable everything */
		vmm_writel(0x0, (void*)(base + UART_PL011_CR));

		/*
		 * Set baud rate
		 *
		 * IBRD = UART_CLK / (16 * BAUD_RATE)
		 * FBRD = RND((64 * MOD(UART_CLK,(16 * BAUD_RATE))) 
		 * 	  / (16 * BAUD_RATE))
		 */
		temp = 16 * baudrate;
		divider = arch_udiv32(input_clock, temp);
		remainder = arch_umod32(input_clock, temp);
		temp = arch_udiv32((8 * remainder), baudrate);
		fraction = (temp >> 1) + (temp & 1);

		vmm_writel(divider, (void*)(base + UART_PL011_IBRD));
		vmm_writel(fraction, (void*)(base + UART_PL011_FBRD));

		/* Set the UART to be 8 bits, 1 stop bit, 
		 * no parity, fifo enabled 
		 */
		vmm_writel((UART_PL011_LCRH_WLEN_8 | UART_PL011_LCRH_FEN),
			(void*)(base + UART_PL011_LCRH));

		/* Finally, enable the UART */
		vmm_writel((UART_PL011_CR_UARTEN | 
				UART_PL011_CR_TXE | 
				UART_PL011_CR_RXE),
			(void*)(base + UART_PL011_CR));
	} else {
		/* First, disable everything */
		vmm_writel(0x0, (void*)(base + UART_PL010_CR));

		/* Set baud rate */
		switch (baudrate) {
		case 9600:
			divider = UART_PL010_BAUD_9600;
			break;

		case 19200:
			divider = UART_PL010_BAUD_9600;
			break;

		case 38400:
			divider = UART_PL010_BAUD_38400;
			break;

		case 57600:
			divider = UART_PL010_BAUD_57600;
			break;

		case 115200:
			divider = UART_PL010_BAUD_115200;
			break;

		default:
			divider = UART_PL010_BAUD_38400;
		}

		vmm_writel(((divider & 0xf00) >> 8), 
					(void*)(base + UART_PL010_LCRM));
		vmm_writel((divider & 0xff), (void*)(base + UART_PL010_LCRL));

		/* Set the UART to be 8 bits, 1 stop bit, 
		 * no parity, fifo enabled */
		vmm_writel((UART_PL010_LCRH_WLEN_8 | UART_PL010_LCRH_FEN),
					(void*)(base + UART_PL010_LCRH));

		/* Finally, enable the UART */
		vmm_writel((UART_PL010_CR_UARTEN), 
					(void*)(base + UART_PL010_CR));
	}
}

struct pl01x_port {
	struct vmm_completion read_done;
	virtual_addr_t base;
	u32 baudrate;
	u32 input_clock;
	u32 type;
	u32 irq;
};

static int pl01x_irq_handler(u32 irq_no, arch_regs_t * regs, void *dev)
{
	unsigned int data;
	struct pl01x_port *port = (struct pl01x_port *)dev;

	/* Get masked interrupt status */
	data = vmm_readl((void*)(port->base + UART_PL011_MIS));

	/* Only handle RX FIFO not empty */
	if(data & UART_PL011_MIS_RXMIS) {
		/* Mask RX interrupts till RX FIFO is empty */
		vmm_writel(0x0, (void*)(port->base + UART_PL011_IMSC));
		/* Signal work completions to all sleeping threads */
		vmm_completion_complete_all(&port->read_done);
	}

	/* Clear all interrupts */
	vmm_writel(data, (void*)(port->base + UART_PL011_ICR));

	return VMM_OK;
}

static u8 pl01x_getc_sleepable(struct pl01x_port * port)
{
	/* Wait until there is data in the FIFO */
	if (pl01x_lowlevel_can_getc(port->base, port->type) == FALSE) {
		/* Enable the RX interrupt */
		vmm_writel(UART_PL011_IMSC_RXIM, 
			   (void*)(port->base + UART_PL011_IMSC));

		/* Wait for completion */
		vmm_completion_wait(&port->read_done);
	}

	/* Read data to destination */
	return (u8)vmm_readl((void*)(port->base + UART_PL01x_DR));
}

static u32 pl01x_read(struct vmm_chardev *cdev, 
		      u8 *dest, size_t offset, size_t len, bool block)
{
	u32 i;
	struct pl01x_port *port;

	if (!(cdev && dest && cdev->priv)) {
		return 0;
	}

	port = cdev->priv;

	if (block && vmm_scheduler_orphan_context()) {
		for(i = 0; i < len; i++) {
			dest[i] = pl01x_getc_sleepable(port);
		}
	} else {
		for(i = 0; i < len; i++) {
			if (!block && 
			    !pl01x_lowlevel_can_getc(port->base, 
						     port->type)) {
				break;
			}

			dest[i] = pl01x_lowlevel_getc(port->base, port->type);
		}
	}

	return i;
}

static u32 pl01x_write(struct vmm_chardev *cdev, 
		       u8 *src, size_t offset, size_t len, bool block)
{
	u32 i;
	struct pl01x_port *port;

	if (!(cdev && src && cdev->priv)) {
		return 0;
	}

	port = cdev->priv;

	for(i = 0; i < len; i++) {
		if (!block && 
		    !pl01x_lowlevel_can_putc(port->base, port->type)) {
				break;
		}

		pl01x_lowlevel_putc(port->base, port->type, src[i]);
	}

	return i;
}

static int pl01x_driver_probe(struct vmm_device *dev,const struct vmm_devid *devid)
{
	int rc;
	const char *attr;
	struct vmm_chardev *cd;
	struct pl01x_port *port;
	
	cd = vmm_malloc(sizeof(struct vmm_chardev));
	if(!cd) {
		rc = VMM_EFAIL;
		goto free_nothing;
	}
	vmm_memset(cd, 0, sizeof(struct vmm_chardev));

	port = vmm_malloc(sizeof(struct pl01x_port));
	if(!port) {
		rc = VMM_EFAIL;
		goto free_chardev;
	}
	vmm_memset(port, 0, sizeof(struct pl01x_port));

	vmm_strcpy(cd->name, dev->node->name);
	cd->dev = dev;
	cd->ioctl = NULL;
	cd->read = pl01x_read;
	cd->write = pl01x_write;
	cd->priv = port;

	INIT_COMPLETION(&port->read_done);

	rc = vmm_devdrv_ioremap(dev, &port->base, 0);
	if(rc) {
		goto free_port;
	}

	if (vmm_strcmp(devid->compatible, "pl011")==0) {
		port->type = PL01X_TYPE_1;
	} else {
		port->type = PL01X_TYPE_0;
	}

	attr = vmm_devtree_attrval(dev->node, "baudrate");
	if(!attr) {
		rc = VMM_EFAIL;
		goto free_port;
	}
	port->baudrate = *((u32 *)attr);
	rc = vmm_devdrv_getclock(dev, &port->input_clock);
	if(rc) {
		goto free_port;
	}

	attr = vmm_devtree_attrval(dev->node, "irq");
	if(!attr) {
		rc = VMM_EFAIL;
		goto free_port;
	}
	port->irq = *((u32 *)attr);
	if((rc = vmm_host_irq_register(port->irq, pl01x_irq_handler, port))) {
		goto free_port;
	}
	if((rc = vmm_host_irq_enable(port->irq))) {
		goto free_port;
	}

	/* Call low-level init function */
	pl01x_lowlevel_init(port->base, 
				port->type, 
				port->baudrate, 
				port->input_clock);

	rc = vmm_chardev_register(cd);
	if(rc) {
		goto free_port;
	}

	return VMM_OK;

free_port:
	vmm_free(port);
free_chardev:
	vmm_free(cd);
free_nothing:
	return rc;
}

static int pl01x_driver_remove(struct vmm_device *dev)
{
	int rc = VMM_OK;
	struct vmm_chardev *cd =(struct vmm_chardev*)dev->priv;

	if (cd) {
		rc = vmm_chardev_unregister(cd);
		vmm_free(cd->priv);
		vmm_free(cd);
		dev->priv = NULL;
	}

	return rc;
}

static struct vmm_devid pl01x_devid_table[] = {
	{ .type = "serial", .compatible = "pl010" },
	{ .type = "serial", .compatible = "pl011" },
	{ /* end of list */ },
};

static struct vmm_driver pl01x_driver = {
	.name = "pl01x_serial",
	.match_table = pl01x_devid_table,
	.probe = pl01x_driver_probe,
	.remove = pl01x_driver_remove,
};

static int __init pl01x_driver_init(void)
{
	return vmm_devdrv_register_driver(&pl01x_driver);
}

static void pl01x_driver_exit(void)
{
	vmm_devdrv_unregister_driver(&pl01x_driver);
}

VMM_DECLARE_MODULE(MODULE_VARID, 
			MODULE_NAME, 
			MODULE_AUTHOR, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
