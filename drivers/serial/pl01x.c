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

#include <vmm_error.h>
#include <vmm_host_io.h>
#include <vmm_heap.h>
#include <vmm_string.h>
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

struct pl01x_port {
	virtual_addr_t base;
	u32 baudrate;
	u32 input_clock;
	u32 type;
};

typedef struct pl01x_port pl01x_port_t;

bool pl01x_lowlevel_can_getc(virtual_addr_t base, u32 type)
{
	if(vmm_readl((void*)(base + UART_PL01x_FR)) & UART_PL01x_FR_RXFE) {
		return FALSE;
	}
	return TRUE;
}

char pl01x_lowlevel_getc(virtual_addr_t base, u32 type)
{
	unsigned int data;

	/* Wait until there is data in the FIFO */
	while (vmm_readl((void*)(base + UART_PL01x_FR)) & UART_PL01x_FR_RXFE);

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

void pl01x_lowlevel_putc(virtual_addr_t base, u32 type, char ch)
{
	if(ch=='\n') {
		/* Wait until there is space in the FIFO */
		while (vmm_readl((void*)(base + UART_PL01x_FR)) & UART_PL01x_FR_TXFF);

		/* Send the character */
		vmm_writel('\r', (void*)(base + UART_PL01x_DR));
	}

	/* Wait until there is space in the FIFO */
	while (vmm_readl((void*)(base + UART_PL01x_FR)) & UART_PL01x_FR_TXFF);

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
		divider = input_clock / temp;
		remainder = input_clock % temp;
		temp = (8 * remainder) / baudrate;
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

static u32 pl01x_read(vmm_chardev_t *cdev, 
				char *dest, size_t offset, size_t len)
{
	u32 i;
	pl01x_port_t *port;

	if(!cdev || !dest) {
		return 0;
	}
	if(!cdev->priv) {
		return 0;
	}

	port = cdev->priv;

	for(i = 0; i < len; i++) {
		if (!pl01x_lowlevel_can_getc(port->base, port->type)) {
			break;
		}
		dest[i] = pl01x_lowlevel_getc(port->base, port->type);
	}

	return i;
}

static u32 pl01x_write(vmm_chardev_t *cdev, 
				char *src, size_t offset, size_t len)
{
	u32 i;
	pl01x_port_t *port;

	if(!cdev || !src) {
		return 0;
	}
	if(!cdev->priv) {
		return 0;
	}

	port = cdev->priv;

	for(i = 0; i < len; i++) {
		if (!pl01x_lowlevel_can_putc(port->base, port->type)) {
			break;
		}
		pl01x_lowlevel_putc(port->base, port->type, src[i]);
	}

	return i;
}

static int pl01x_driver_probe(vmm_device_t *dev,const vmm_devid_t *devid)
{
	int rc;
	const char *attr;
	vmm_chardev_t *cd;
	pl01x_port_t *port;
	
	cd = vmm_malloc(sizeof(vmm_chardev_t));
	if(!cd) {
		rc = VMM_EFAIL;
		goto free_nothing;
	}

	port = vmm_malloc(sizeof(pl01x_port_t));
	if(!port) {
		rc = VMM_EFAIL;
		goto free_chardev;
	}

	vmm_strcpy(cd->name, dev->node->name);
	cd->dev = dev;
	cd->ioctl = NULL;
	cd->read = pl01x_read;
	cd->write = pl01x_write;
	cd->priv = port;

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

static int pl01x_driver_remove(vmm_device_t *dev)
{
	int rc;
	vmm_chardev_t *cd =(vmm_chardev_t*)dev->priv;

	rc = vmm_chardev_unregister(cd);
	vmm_free(cd->priv);
	vmm_free(cd);
	dev->priv = NULL;

	return rc;
}

static vmm_devid_t pl01x_devid_table[] = {
	{ .type = "serial", .compatible = "pl010" },
	{ .type = "serial", .compatible = "pl011" },
	{ /* end of list */ },
};

static vmm_driver_t pl01x_driver = {
	.name = "pl01x_serial",
	.match_table = pl01x_devid_table,
	.probe = pl01x_driver_probe,
	.remove = pl01x_driver_remove,
};

static int pl01x_driver_init(void)
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
