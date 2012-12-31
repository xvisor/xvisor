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
 * @author Anup Patel (anup@brainfault.org)
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief source file for UART serial port driver.
 */

#include <vmm_error.h>
#include <vmm_host_io.h>
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
#include <drv/uart.h>

#define MODULE_DESC			"Generic UART Driver"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			uart_driver_init
#define	MODULE_EXIT			uart_driver_exit

#undef UART_POLLING

struct uart_port {
	struct vmm_completion read_possible;
	struct vmm_chardev cd;
	virtual_addr_t base;
	u32 baudrate;
	u32 input_clock;
	u32 reg_align;
	u32 irq;
	u32 ier;
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
	while (!uart_lowlevel_can_getc(base, reg_align));

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
	while (!uart_lowlevel_can_putc(base, reg_align));

	vmm_out_8((u8 *)REG_UART_THR(base,reg_align), ch);
}

void uart_lowlevel_init(virtual_addr_t base, u32 reg_align, 
			u32 baudrate, u32 input_clock)
{
	u16 bdiv;
	bdiv = udiv32(input_clock, (16 * baudrate));

	/* set DLAB bit */
	vmm_out_8((u8 *)REG_UART_LCR(base,reg_align), 0x80);
	/* set baudrate divisor */
	vmm_out_8((u8 *)REG_UART_DLL(base,reg_align), bdiv & 0xFF);
	/* set baudrate divisor */
	vmm_out_8((u8 *)REG_UART_DLM(base,reg_align), (bdiv >> 8) & 0xFF); 
	/* clear DLAB; set 8 bits, no parity */
	vmm_out_8((u8 *)REG_UART_LCR(base,reg_align), 0x03);
	/* enable FIFO */
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
	vmm_out_8((u8 *)REG_UART_IER(base,reg_align), 0x00);
}

#ifndef UART_POLLING
static vmm_irq_return_t uart_irq_handler(u32 irq_no, arch_regs_t * regs, void *dev)
{
	u16 iir, lsr;
	struct uart_port *port = (struct uart_port *)dev;

        iir = vmm_in_8((u8 *)REG_UART_IIR(port->base, port->reg_align));
        if (iir & UART_IIR_NOINT)
                return VMM_IRQ_NONE;

        lsr = vmm_in_8((u8 *)REG_UART_LSR(port->base, port->reg_align));

	/* Handle RX FIFO not empty */
         if (iir & (UART_IIR_RLSI | UART_IIR_RTO | UART_IIR_RDI)) { 
		if (lsr & UART_LSR_DR) {
			/* Mask RX interrupts till RX FIFO is empty */
			port->ier &= ~(UART_IER_RDI | UART_IER_RLSI);
			/* Signal work completion to sleeping thread */
			vmm_completion_complete(&port->read_possible);
		} else if (lsr & (UART_LSR_OE | UART_LSR_PE | UART_LSR_BI | UART_LSR_FE) ) {
			while(1);
		}
        }

#if 0
	/* Handle TX FIFO not full */
	if ((lsr & UART_LSR_THRE) && (iir & UART_IIR_THRI)) {
		/* Mask TX interrupts till TX FIFO is full */
		port->ier &= ~UART_IER_THRI;
		/* Signal work completion to sleeping thread */
		vmm_completion_complete_all(&port->write_possible);
	}
#endif

	vmm_out_8((u8 *)REG_UART_IER(port->base, port->reg_align), port->ier);

	return VMM_IRQ_HANDLED;
}

static u8 uart_getc_sleepable(struct uart_port *port)
{
	/* Wait until there is data in the FIFO */
	if (!uart_lowlevel_can_getc(port->base, port->reg_align)) {
		/* Enable the RX interrupt */
		port->ier |= (UART_IER_RDI | UART_IER_RLSI);
		vmm_out_8((u8 *)REG_UART_IER(port->base, port->reg_align), 
				port->ier);

		/* Wait for completion */
		vmm_completion_wait(&port->read_possible);
	}

	/* Read data to destination */
	return (vmm_in_8((u8 *)REG_UART_RBR(port->base, port->reg_align)));
}
#endif

static u32 uart_read(struct vmm_chardev *cdev, 
		     u8 *dest, u32 len, bool sleep)
{
	u32 i;
	struct uart_port *port;

	if (!(cdev && dest && cdev->priv)) {
		return 0;
	}

	port = cdev->priv;

#ifndef UART_POLLING
	if (sleep) {
		for(i = 0; i < len; i++) {
			dest[i] = uart_getc_sleepable(port);
		}
	} else {
#endif
		for(i = 0; i < len; i++) {
			if (!uart_lowlevel_can_getc(port->base, port->reg_align)) {
				break;
			}
			dest[i] = uart_lowlevel_getc(port->base, port->reg_align);
		}
#ifndef UART_POLLING
	}
#endif

	return i;
}

static u32 uart_write(struct vmm_chardev *cdev, 
		      u8 *src, u32 len, bool sleep)
{
	u32 i;
	struct uart_port *port;

	if (!(cdev && src && cdev->priv)) {
		return 0;
	}

	port = cdev->priv;

	for(i = 0; i < len; i++) {
		if (!uart_lowlevel_can_putc(port->base, port->reg_align)) {
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
	struct uart_port *port;
	
	port = vmm_zalloc(sizeof(struct uart_port));
	if(!port) {
		rc = VMM_EFAIL;
		goto free_nothing;
	}

	strcpy(port->cd.name, dev->node->name);
	port->cd.dev = dev;
	port->cd.ioctl = NULL;
	port->cd.read = uart_read;
	port->cd.write = uart_write;
	port->cd.priv = port;

	INIT_COMPLETION(&port->read_possible);

	rc = vmm_devtree_regmap(dev->node, &port->base, 0);
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
		goto free_reg;
	}
	port->baudrate = *((u32 *)attr);
	port->input_clock = vmm_devdrv_clock_get_rate(dev);

	/* Call low-level init function 
	 * Note: low-level init will make sure that
	 * interrupts are disabled in IER register.
	 */
	uart_lowlevel_init(port->base, port->reg_align, 
			port->baudrate, port->input_clock);

#ifndef UART_POLLING
	attr = vmm_devtree_attrval(dev->node, "irq");
	if (!attr) {
		rc = VMM_EFAIL;
		goto free_port;
	}
	port->irq = *((u32 *) attr);
	if ((rc = vmm_host_irq_register(port->irq, dev->node->name,
					uart_irq_handler, port))) {
		goto free_port;
	}
#endif

	rc = vmm_chardev_register(&port->cd);
	if(rc) {
		goto free_reg;
	}

	dev->priv = port;

	return VMM_OK;

free_reg:
	vmm_devtree_regunmap(dev->node, port->base, 0);
free_port:
	vmm_free(port);
free_nothing:
	return rc;
}

static int uart_driver_remove(struct vmm_device *dev)
{
	struct uart_port *port = dev->priv;

	if (port) {
		vmm_chardev_unregister(&port->cd);
		vmm_devtree_regunmap(dev->node, port->base, 0);
		vmm_free(port);
		dev->priv = NULL;
	}

	return VMM_OK;
}

static struct vmm_devid uart_devid_table[] = {
	{ .type = "serial", .compatible = "ns8250"},
	{ .type = "serial", .compatible = "ns16450"},
	{ .type = "serial", .compatible = "ns16550a"},
	{ .type = "serial", .compatible = "ns16550"},
	{ .type = "serial", .compatible = "ns16750"},
	{ .type = "serial", .compatible = "ns16850"},
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

static void __exit uart_driver_exit(void)
{
	vmm_devdrv_unregister_driver(&uart_driver);
}

VMM_DECLARE_MODULE(MODULE_DESC, 
			MODULE_AUTHOR, 
			MODULE_LICENSE, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
