/**
 * Copyright (c) 2016 Anup Patel.
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
 * @file bcm283x_mu.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief source file for BCM283x Miniuart serial driver.
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
#include <drv/serial/bcm283x_mu.h>

#define MODULE_DESC			"BCM283x Miniuart Serial Driver"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(SERIAL_IPRIORITY+1)
#define	MODULE_INIT			bcm283x_mu_driver_init
#define	MODULE_EXIT			bcm283x_mu_driver_exit

bool bcm283x_mu_lowlevel_can_getc(virtual_addr_t base)
{
	void *lsr = (void *)(base + BCM283X_MU_LSR);

	if (!(vmm_readl(lsr) & BCM283X_MU_LSR_RX_READY))
		return FALSE;

	return TRUE;
}

u8 bcm283x_mu_lowlevel_getc(virtual_addr_t base)
{
	/* Wait until there is no data in the FIFO */
	while (!bcm283x_mu_lowlevel_can_getc(base)) ;

	/* Read IO register */
	return (char)(vmm_readl((void *)(base + BCM283X_MU_IO)) & 0xFF);
}

bool bcm283x_mu_lowlevel_can_putc(virtual_addr_t base)
{
	void *lsr = (void *)(base + BCM283X_MU_LSR);

	if (!(vmm_readl(lsr) & BCM283X_MU_LSR_TX_EMPTY))
		return FALSE;

	return TRUE;
}

void bcm283x_mu_lowlevel_putc(virtual_addr_t base, u8 ch)
{
	/* Wait until there is data in the FIFO */
	while (!bcm283x_mu_lowlevel_can_putc(base)) ;

	/* Send the character */
	vmm_writel(ch, (void *)(base + BCM283X_MU_IO));
}

void bcm283x_mu_lowlevel_init(virtual_addr_t base,
			      u32 baudrate, u32 input_clock)
{
	u32 val;
	u32 divider = udiv32(input_clock, (baudrate * 8));
	void *iir = (void *)(base + BCM283X_MU_IIR);
	void *ier = (void *)(base + BCM283X_MU_IER);
	void *lcr = (void *)(base + BCM283X_MU_LCR);
	void *baud = (void *)(base + BCM283X_MU_BAUD);
	void *cntl = (void *)(base + BCM283X_MU_CNTL);

	/* Wait until there is data in the FIFO */
	while (!bcm283x_mu_lowlevel_can_putc(base)) ;

	/* Disable port */
	vmm_writel(0x0, cntl);

	/* Disable interrupts */
	vmm_writel(0x0, ier);

	/* Flush port */
	vmm_writel(BCM283X_MU_IIR_FLUSH, iir);

	/* Setup 8bit data width and baudrate */
	vmm_writel(BCM283X_MU_LCR_8BIT, lcr);
	vmm_writel(divider - 1, baud);

	/* Enable RX & TX port */
	val = BCM283X_MU_CNTL_RX_ENABLE | BCM283X_MU_CNTL_TX_ENABLE;
	vmm_writel(val, cntl);
}

struct bcm283x_mu_port {
	struct serial *p;
	virtual_addr_t base;
	u32 baudrate;
	u32 input_clock;
	u32 irq;
	u16 mask;
};

static vmm_irq_return_t bcm283x_mu_irq_handler(int irq_no, void *dev)
{
	u8 ch;
	u32 status;
	struct bcm283x_mu_port *port = (struct bcm283x_mu_port *)dev;

	/* Get interrupt status */
	status = vmm_readl((void *)(port->base + BCM283X_MU_IIR));

	/* Handle RX interrupt */
	if (status & BCM283X_MU_IIR_RX_INTERRUPT) {
		/* Pull-out bytes from RX FIFO */
		while (bcm283x_mu_lowlevel_can_getc(port->base)) {
			ch = bcm283x_mu_lowlevel_getc(port->base);
			serial_rx(port->p, &ch, 1);
		}
	}

	return VMM_IRQ_HANDLED;
}

static u32 bcm283x_mu_tx(struct serial *p, u8 *src, size_t len)
{
	u32 i;
	struct bcm283x_mu_port *port = serial_tx_priv(p);

	for (i = 0; i < len; i++) {
		if (!bcm283x_mu_lowlevel_can_putc(port->base)) {
			break;
		}
		bcm283x_mu_lowlevel_putc(port->base, src[i]);
	}

	return i;
}

static int bcm283x_mu_driver_probe(struct vmm_device *dev,
				   const struct vmm_devtree_nodeid *devid)
{
	int rc;
	struct bcm283x_mu_port *port;

	port = vmm_zalloc(sizeof(struct bcm283x_mu_port));
	if (!port) {
		rc = VMM_ENOMEM;
		goto free_nothing;
	}

	rc = vmm_devtree_request_regmap(dev->of_node, &port->base, 0,
					"BCM283x MINIUART");
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
					bcm283x_mu_irq_handler, port))) {
		goto free_reg;
	}

	/* Call low-level init function */
	bcm283x_mu_lowlevel_init(port->base,
				 port->baudrate, port->input_clock);

	/* Create Serial Port */
	port->p = serial_create(dev, 256, bcm283x_mu_tx, port);
	if (VMM_IS_ERR_OR_NULL(port->p)) {
		rc = VMM_PTR_ERR(port->p);
		goto free_irq;
	}

	/* Save port pointer */
	dev->priv = port;

	/* Unmask Rx Interrupt */
	port->mask |= BCM283X_MU_IER_RX_INTERRUPT;
	port->mask |= BCM283X_MU_IER_ENABLE_INTERRUPT;
	vmm_writel(port->mask, (void *)(port->base + BCM283X_MU_IER));

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

static int bcm283x_mu_driver_remove(struct vmm_device *dev)
{
	struct bcm283x_mu_port *port = dev->priv;

	if (!port) {
		return VMM_OK;
	}

	/* Mask RX interrupts */
	port->mask &= ~BCM283X_MU_IER_RX_INTERRUPT;
	port->mask &= BCM283X_MU_IER_ENABLE_INTERRUPT;
	vmm_writel(port->mask, (void *)(port->base + BCM283X_MU_IER));

	/* Free-up resources */
	serial_destroy(port->p);
	vmm_host_irq_unregister(port->irq, port);
	vmm_devtree_regunmap_release(dev->of_node, port->base, 0);
	vmm_free(port);
	dev->priv = NULL;

	return VMM_OK;
}

static struct vmm_devtree_nodeid bcm283x_mu_devid_table[] = {
	{ .compatible = "brcm,bcm283x-mu" },
	{ /* end of list */ },
};

static struct vmm_driver bcm283x_mu_driver = {
	.name = "bcm283x_mu_serial",
	.match_table = bcm283x_mu_devid_table,
	.probe = bcm283x_mu_driver_probe,
	.remove = bcm283x_mu_driver_remove,
};

static int __init bcm283x_mu_driver_init(void)
{
	return vmm_devdrv_register_driver(&bcm283x_mu_driver);
}

static void __exit bcm283x_mu_driver_exit(void)
{
	vmm_devdrv_unregister_driver(&bcm283x_mu_driver);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
