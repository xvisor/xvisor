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
 * @file 8250-uart.c
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
#include <drv/8250-uart.h>

#define MODULE_DESC			"8250 UART Driver"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			uart_8250_driver_init
#define	MODULE_EXIT			uart_8250_driver_exit

#undef UART_POLLING
#define UART_RXBUF_SIZE 1024

static u8 uart_8250_in8(struct uart_8250_port *port, u32 offset)
{
	return vmm_inb((port->base+(offset*port->reg_align)));
}

static void uart_8250_out8(struct uart_8250_port *port, u32 offset, u8 val)
{
	vmm_outb(val, (port->base+(offset*port->reg_align)));
	if (offset == UART_LCR_OFFSET) {
		port->lcr_last = val;
	}
}

static void uart_8250_clear_errors(struct uart_8250_port *port)
{
	/* If there was a RX FIFO error (because of framing, parity, 
	 * break error) keep removing entries from RX FIFO until
	 * LSR does not show this bit set
	 */
        while(uart_8250_in8(port, UART_LSR_OFFSET) & UART_LSR_BRK_ERROR_BITS) {
		uart_8250_in8(port, UART_RBR_OFFSET);
	};
}

bool uart_8250_lowlevel_can_getc(struct uart_8250_port *port)
{
	if (uart_8250_in8(port, UART_LSR_OFFSET) & UART_LSR_DR) {
		return TRUE;
	}
	return FALSE;
}

u8 uart_8250_lowlevel_getc(struct uart_8250_port *port)
{
	if (uart_8250_in8(port, UART_LSR_OFFSET) & UART_LSR_DR) {
		return (uart_8250_in8(port, UART_RBR_OFFSET));
	}
	return 0;
}

bool uart_8250_lowlevel_can_putc(struct uart_8250_port *port)
{
	if (uart_8250_in8(port, UART_LSR_OFFSET) & UART_LSR_THRE) {
		return TRUE;
	}
	return FALSE;
}

void uart_8250_lowlevel_putc(struct uart_8250_port *port, u8 ch)
{
	if (uart_8250_in8(port, UART_LSR_OFFSET) & UART_LSR_THRE) {
		uart_8250_out8(port, UART_THR_OFFSET, ch);
	}
}

void uart_8250_lowlevel_init(struct uart_8250_port *port) 
{
	u16 bdiv;
	bdiv = udiv32(port->input_clock, (16 * port->baudrate));

	/* set DLAB bit */
	uart_8250_out8(port, UART_LCR_OFFSET, 0x80);
	/* set baudrate divisor */
	uart_8250_out8(port, UART_DLL_OFFSET, bdiv & 0xFF);
	/* set baudrate divisor */
	uart_8250_out8(port, UART_DLM_OFFSET, (bdiv >> 8) & 0xFF); 
	/* clear DLAB; set 8 bits, no parity */
	uart_8250_out8(port, UART_LCR_OFFSET, 0x03);
	/* enable FIFO */
	uart_8250_out8(port, UART_FCR_OFFSET, 0x01);
	/* no modem control DTR RTS */
	uart_8250_out8(port, UART_MCR_OFFSET, 0x00);
	/* clear line status */
	uart_8250_in8(port, UART_LSR_OFFSET);
	/* read receive buffer */
	uart_8250_in8(port, UART_RBR_OFFSET);
	/* set scratchpad */
	uart_8250_out8(port, UART_SCR_OFFSET, 0x00);
	/* set interrupt enable reg */
	port->ier = 0x00;
	uart_8250_out8(port, UART_IER_OFFSET, 0x00);
}

#ifndef UART_POLLING
static vmm_irq_return_t uart_8250_irq_handler(int irq_no, void *dev)
{
	u16 iir, lsr;
	struct uart_8250_port *port = (struct uart_8250_port *)dev;

        iir = uart_8250_in8(port, UART_IIR_OFFSET);
        lsr = uart_8250_in8(port, UART_LSR_OFFSET);

	switch (iir & 0xf) {
        case UART_IIR_NOINT:
		return VMM_IRQ_NONE;

	case UART_IIR_RLSI:
	case UART_IIR_RTO:
	case UART_IIR_RDI: 
		if (lsr & UART_LSR_BRK_ERROR_BITS) {
			uart_8250_clear_errors(port);
		}
		if (lsr & UART_LSR_DR) {
			vmm_spin_lock(&port->rxlock);
			do {
				u8 ch = uart_8250_in8(port, UART_RBR_OFFSET);
				if (((port->rxtail + 1) % UART_RXBUF_SIZE) 
							!= port->rxhead) {
					port->rxbuf[port->rxtail] = ch;
					port->rxtail = 
					((port->rxtail + 1) % UART_RXBUF_SIZE);
				} else {
					break;
				}
			} while (uart_8250_in8(port, UART_LSR_OFFSET) & 
						(UART_LSR_DR | UART_LSR_OE));
			vmm_spin_unlock(&port->rxlock);
			/* Signal work completion to sleeping thread */
			vmm_completion_complete(&port->read_possible);
		} else {
			while (1);
		}			
		break;

	case UART_IIR_BUSY:
		/* This is unallocated IIR value as per generic UART but is 
		 * used by Designware UARTs, we do not expect other UART IPs 
		 * to hit this case 
		 */ 
		uart_8250_in8(port, 0x1f);
		uart_8250_out8(port, UART_LCR_OFFSET, port->lcr_last);
		break;
	default:
		break;
	};

	return VMM_IRQ_HANDLED;
}

static u8 uart_8250_getc_sleepable(struct uart_8250_port *port)
{
	u8 ch;
	bool avail = FALSE;
	irq_flags_t flags;
	while (!avail) {
		/* Check the rxbuf for data */
		vmm_spin_lock_irqsave(&port->rxlock, flags);
		if (port->rxhead != port->rxtail) {
			avail = TRUE;
			ch = port->rxbuf[port->rxhead];
			port->rxhead = ((port->rxhead + 1) % UART_RXBUF_SIZE);
		}
		vmm_spin_unlock_irqrestore(&port->rxlock, flags);

		/* Wait for completion */
		if (!avail) {
			vmm_completion_wait(&port->read_possible);
		}
	}
	return ch;
}
#endif

static u32 uart_8250_read(struct vmm_chardev *cdev, 
		     u8 *dest, u32 len, bool sleep)
{
	u32 i = 0;
	irq_flags_t flags;
	struct uart_8250_port *port;

	if (!(cdev && dest && cdev->priv)) {
		return 0;
	}

	port = cdev->priv;

#ifndef UART_POLLING
	if (sleep) {
		/* Ensure RX interrupts are enabled */
		port->ier |= (UART_IER_RLSI | UART_IER_RDI);
		uart_8250_out8(port, UART_IER_OFFSET, port->ier);

		for(i = 0; i < len; i++) {
			dest[i] = uart_8250_getc_sleepable(port);
		}
	} else {
#endif
		/* Disable the RX interrupts as we do not want irq-handler to
		 * start pulling the RX characters */
		uart_8250_out8(port, UART_IER_OFFSET, 
				port->ier & ~(UART_IER_RLSI | UART_IER_RDI));
		/* Check the RX FIFO buffer if interrupts were enabled */
		if (port->ier & (UART_IER_RLSI | UART_IER_RDI)) {
			vmm_spin_lock_irqsave(&port->rxlock, flags);
			for(; (i < len) && (port->rxhead != port->rxtail); i++) {
				dest[i] = port->rxbuf[port->rxhead];
				port->rxhead = 
					((port->rxhead + 1) % UART_RXBUF_SIZE);
			}
			vmm_spin_unlock_irqrestore(&port->rxlock, flags);
		}
		/* If more characters needed directly poll the UART */
		for(; i < len; i++) {
			if (!uart_8250_lowlevel_can_getc(port)) {
				break;
			}
			dest[i] = uart_8250_lowlevel_getc(port);
		}
		/* Restore IER value */ 
		uart_8250_out8(port, UART_IER_OFFSET, port->ier);
#ifndef UART_POLLING
	}
#endif

	return i;
}

static u32 uart_8250_write(struct vmm_chardev *cdev, 
		      u8 *src, u32 len, bool sleep)
{
	u32 i;
	struct uart_8250_port *port;

	if (!(cdev && src && cdev->priv)) {
		return 0;
	}

	port = cdev->priv;

	for(i = 0; i < len; i++) {
		if (!uart_8250_lowlevel_can_putc(port)) {
			break;
		}
		uart_8250_lowlevel_putc(port, src[i]);
	}

	return i;
}

static int uart_8250_driver_probe(struct vmm_device *dev, 
				  const struct vmm_devtree_nodeid *devid)
{
	int rc;
	u32 reg_offset;
	struct uart_8250_port *port;
	
	port = vmm_zalloc(sizeof(struct uart_8250_port));
	if(!port) {
		rc = VMM_ENOMEM;
		goto free_nothing;
	}

	if (strlcpy(port->cd.name, dev->name, sizeof(port->cd.name)) >=
	    sizeof(port->cd.name)) {
		rc = VMM_EOVERFLOW;
		goto free_port;
	}

	port->cd.dev.parent = dev;
	port->cd.ioctl = NULL;
	port->cd.read = uart_8250_read;
	port->cd.write = uart_8250_write;
	port->cd.priv = port;

	INIT_COMPLETION(&port->read_possible);

	rc = vmm_devtree_regmap(dev->node, &port->base, 0);
	if(rc) {
		goto free_port;
	}

	if (vmm_devtree_read_u32(dev->node, "reg_align",
				 &port->reg_align)) {
		port->reg_align = 1;
	}

	if (vmm_devtree_read_u32(dev->node, "reg_offset",
				 &reg_offset) == VMM_OK) {
		port->base += reg_offset;
	}

	rc = vmm_devtree_read_u32(dev->node, "baudrate",
				  &port->baudrate);
	if (rc) {
		goto free_reg;
	}

	rc = vmm_devtree_clock_frequency(dev->node, &port->input_clock);
	if (rc) {
		goto free_reg;
	}

	/* Call low-level init function 
	 * Note: low-level init will make sure that
	 * interrupts are disabled in IER register.
	 */
	uart_8250_lowlevel_init(port); 

#ifndef UART_POLLING
	port->rxbuf = vmm_malloc(UART_RXBUF_SIZE);
	if (!port->rxbuf) {
		rc = VMM_EFAIL;
		goto free_reg;
	}
	port->rxhead = port->rxtail = 0;
	INIT_SPIN_LOCK(&port->rxlock);

	rc = vmm_devtree_irq_get(dev->node, &port->irq, 0);
	if (rc) {
		goto free_all;
	}
	if ((rc = vmm_host_irq_register(port->irq, dev->name,
					uart_8250_irq_handler, port))) {
		goto free_all;
	}
#endif

	rc = vmm_chardev_register(&port->cd);
	if(rc) {
		goto free_all;
	}

	dev->priv = port;

	return VMM_OK;

free_all:
#ifndef UART_POLLING
	vmm_free(port->rxbuf);
#endif
free_reg:
	vmm_devtree_regunmap(dev->node, port->base, 0);
free_port:
	vmm_free(port);
free_nothing:
	return rc;
}

static int uart_8250_driver_remove(struct vmm_device *dev)
{
	struct uart_8250_port *port = dev->priv;

	if (port) {
		vmm_chardev_unregister(&port->cd);
		vmm_devtree_regunmap(dev->node, port->base, 0);
		vmm_free(port);
		dev->priv = NULL;
	}

	return VMM_OK;
}

static struct vmm_devtree_nodeid uart_8250_devid_table[] = {
	{ .type = "serial", .compatible = "ns8250"},
	{ .type = "serial", .compatible = "ns16450"},
	{ .type = "serial", .compatible = "ns16550a"},
	{ .type = "serial", .compatible = "ns16550"},
	{ .type = "serial", .compatible = "ns16750"},
	{ .type = "serial", .compatible = "ns16850"},
	{ /* end of list */ },
};

static struct vmm_driver uart_8250_driver = {
	.name = "uart_8250_serial",
	.match_table = uart_8250_devid_table,
	.probe = uart_8250_driver_probe,
	.remove = uart_8250_driver_remove,
};

static int __init uart_8250_driver_init(void)
{
	return vmm_devdrv_register_driver(&uart_8250_driver);
}

static void __exit uart_8250_driver_exit(void)
{
	vmm_devdrv_unregister_driver(&uart_8250_driver);
}

VMM_DECLARE_MODULE(MODULE_DESC, 
			MODULE_AUTHOR, 
			MODULE_LICENSE, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
