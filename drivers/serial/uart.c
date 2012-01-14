/**
 * Copyright (c) 2010 Anup Patel.
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
 * @file uart.c
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief source file for UART serial port driver.
 */

#include <arch_math.h>
#include <vmm_error.h>
#include <vmm_host_io.h>
#include <vmm_heap.h>
#include <vmm_string.h>
#include <vmm_modules.h>
#include <vmm_devtree.h>
#include <vmm_devdrv.h>
#include <vmm_chardev.h>
#include <serial/uart.h>

#define MODULE_VARID			uart_driver_module
#define MODULE_NAME			"Generic UART Driver"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			uart_driver_init
#define	MODULE_EXIT			uart_driver_exit

struct uart_port {
	virtual_addr_t base;
	u32 baudrate;
	u32 input_clock;
	u32 reg_align;
};

bool uart_lowlevel_can_getc(virtual_addr_t base, u32 reg_align)
{
	if (vmm_in_8((u8 *)REG_UART_LSR(base,reg_align)) & UART_LSR_DR) {
		return TRUE;
	}
	return FALSE;
}

u8 uart_lowlevel_getc(virtual_addr_t base, u32 reg_align)
{
	while (uart_lowlevel_can_getc(base, reg_align) == FALSE);

	return (vmm_in_8((u8 *)REG_UART_RBR(base,reg_align)));
}

bool uart_lowlevel_can_putc(virtual_addr_t base, u32 reg_align)
{
	if (vmm_in_8((u8 *)REG_UART_LSR(base,reg_align)) & UART_LSR_THRE) {
		return TRUE;
	}
	return FALSE;
}

void uart_lowlevel_putc(virtual_addr_t base, u32 reg_align, u8 ch)
{
	while (uart_lowlevel_can_putc(base, reg_align) == FALSE);

	vmm_out_8((u8 *)REG_UART_THR(base,reg_align), ch);
}

void uart_lowlevel_init(const char *compatible, virtual_addr_t base, 
		u32 reg_align, u32 baudrate, u32 input_clock)
{
	u16 bdiv;
	bdiv = arch_udiv32(input_clock, (16 * baudrate));

	if(!vmm_strcmp(compatible, "st16654")) {
		/* set interrupt enable reg */
		vmm_out_8((u8 *)REG_UART_IER(base,reg_align), 0x00);
		/* set mode select to disabled before dll/dlh */
		vmm_out_8((u8 *)REG_UART_MDR1(base,reg_align), 0x07);
		/* set DLAB bit */
		vmm_out_8((u8 *)REG_UART_LCR(base,reg_align), 0x83);
		/* clear DLL */
		vmm_out_8((u8 *)REG_UART_DLL(base,reg_align), 0x00);
		/* clear DLM */
		vmm_out_8((u8 *)REG_UART_DLM(base,reg_align), 0x00); 
		/* clear DLAB; set 8 bits, no parity */
		vmm_out_8((u8 *)REG_UART_LCR(base,reg_align), 0x03);
		/* no modem control DTR RTS */
		vmm_out_8((u8 *)REG_UART_MCR(base,reg_align), 0x03);
		/* enable FIFO */
		vmm_out_8((u8 *)REG_UART_FCR(base,reg_align), 0x07);
		/* set DLAB bit */
		vmm_out_8((u8 *)REG_UART_LCR(base,reg_align), 0x83);
		/* set baudrate divisor */
		vmm_out_8((u8 *)REG_UART_DLL(base,reg_align), bdiv & 0xFF);
		/* set baudrate divisor */
		vmm_out_8((u8 *)REG_UART_DLM(base,reg_align), (bdiv >> 8) & 0xFF); 
		/* clear DLAB; set 8 bits, no parity */
		vmm_out_8((u8 *)REG_UART_LCR(base,reg_align), 0x03);
		/* set mode select to 16x mode  */
		vmm_out_8((u8 *)REG_UART_MDR1(base,reg_align), 0x00);
	} else {
		/* set DLAB bit */
		vmm_out_8((u8 *)REG_UART_LCR(base,reg_align), 0x80);
		/* set baudrate divisor */
		vmm_out_8((u8 *)REG_UART_DLL(base,reg_align), bdiv & 0xFF);
		/* set baudrate divisor */
		vmm_out_8((u8 *)REG_UART_DLM(base,reg_align), (bdiv >> 8) & 0xFF); 
		/* clear DLAB; set 8 bits, no parity */
		vmm_out_8((u8 *)REG_UART_LCR(base,reg_align), 0x03);
		/* disable FIFO */
		vmm_out_8((u8 *)REG_UART_FCR(base,reg_align), 0x01);
		/* no modem control DTR RTS */
		vmm_out_8((u8 *)REG_UART_MCR(base,reg_align), 0x00);
		/* clear line status */
		vmm_in_8((u8 *)REG_UART_LSR(base,reg_align));
		/* read receive buffer */
		vmm_in_8((u8 *)REG_UART_RBR(base,reg_align));
		/* set scratchpad */
		vmm_out_8((u8 *)REG_UART_SCR(base,reg_align), 0x00);
		/* set interrupt enable reg */
		vmm_out_8((u8 *)REG_UART_IER(base,reg_align), 0x0F);
	}
}

static u32 uart_read(struct vmm_chardev *cdev, 
		     u8 *dest, size_t offset, size_t len, bool block)
{
	u32 i;
	struct uart_port *port;

	if (!(cdev && dest && cdev->priv)) {
		return 0;
	}

	port = cdev->priv;

	for(i = 0; i < len; i++) {
		if (!block && 
		    !uart_lowlevel_can_getc(port->base, port->reg_align)) {
			break;
		}

		dest[i] = uart_lowlevel_getc(port->base, port->reg_align);
	}

	return i;
}

static u32 uart_write(struct vmm_chardev *cdev, 
		      u8 *src, size_t offset, size_t len, bool block)
{
	u32 i;
	struct uart_port *port;

	if (!(cdev && src && cdev->priv)) {
		return 0;
	}

	port = cdev->priv;

	for(i = 0; i < len; i++) {
		if (!block && 
		    !uart_lowlevel_can_putc(port->base, port->reg_align)) {
			break;
		}

		uart_lowlevel_putc(port->base, port->reg_align, src[i]);
	}

	return i;
}

static int uart_driver_probe(struct vmm_device *dev,const struct vmm_devid *devid)
{
	int rc;
	const char *attr;
	struct vmm_chardev *cd;
	struct uart_port *port;
	
	cd = vmm_malloc(sizeof(struct vmm_chardev));
	if(!cd) {
		rc = VMM_EFAIL;
		goto free_nothing;
	}
	vmm_memset(cd, 0, sizeof(struct vmm_chardev));

	port = vmm_malloc(sizeof(struct uart_port));
	if(!port) {
		rc = VMM_EFAIL;
		goto free_chardev;
	}
	vmm_memset(port, 0, sizeof(struct uart_port));

	vmm_strcpy(cd->name, dev->node->name);
	cd->dev = dev;
	cd->ioctl = NULL;
	cd->read = uart_read;
	cd->write = uart_write;
	cd->priv = port;

	rc = vmm_devdrv_ioremap(dev, &port->base, 0);
	if(rc) {
		goto free_port;
	}

	attr = vmm_devtree_attrval(dev->node, "reg_align");
	if (attr) {
		port->reg_align = *((u32 *)attr);
	} else {
		port->reg_align = 1;
	}

	attr = vmm_devtree_attrval(dev->node, "reg_offset");
	if (attr) {
		port->base += *((u32 *)attr);
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

	attr = vmm_devtree_attrval(dev->node, "compatible");
	/* Call low-level init function */
	uart_lowlevel_init(attr, port->base, 
				port->reg_align, 
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

static int uart_driver_remove(struct vmm_device *dev)
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

static struct vmm_devid uart_devid_table[] = {
	{ .type = "serial", .compatible = "ns8250"},
	{ .type = "serial", .compatible = "ns16450"},
	{ .type = "serial", .compatible = "ns16550a"},
	{ .type = "serial", .compatible = "ns16550"},
	{ .type = "serial", .compatible = "ns16750"},
	{ .type = "serial", .compatible = "ns16850"},
	{ .type = "serial", .compatible = "st16654"},
	{ /* end of list */ },
};

static struct vmm_driver uart_driver = {
	.name = "uart_serial",
	.match_table = uart_devid_table,
	.probe = uart_driver_probe,
	.remove = uart_driver_remove,
};

static int __init uart_driver_init(void)
{
	return vmm_devdrv_register_driver(&uart_driver);
}

static void uart_driver_exit(void)
{
	vmm_devdrv_unregister_driver(&uart_driver);
}

VMM_DECLARE_MODULE(MODULE_VARID, 
			MODULE_NAME, 
			MODULE_AUTHOR, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
