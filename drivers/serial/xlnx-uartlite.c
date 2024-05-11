/**
 * Copyright (c) 2024 Xu, Zefan.
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
 * @file xlnx-uartlite.c
 * @author Xu, Zefan (ceba_robot@outlook.com)
 * @brief source file for xlinx uartlite driver.
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
#include <drv/serial/xlnx-uartlite.h>

#define MODULE_DESC			"Xlinx uartlite driver"
#define MODULE_AUTHOR			"Xu, Zefan"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(SERIAL_IPRIORITY+1)
#define MODULE_INIT			xlnx_uartlite_driver_init
#define MODULE_EXIT			xlnx_uartlite_driver_exit

bool xlnx_uartlite_lowlevel_can_getc(struct xlnx_uartlite *regs)
{
	if (vmm_readl((void*)&regs->stat_reg) & UARTLITE_STAT_RX_FIFO_VALID_DATA)
		return TRUE;

	return FALSE;
}

u8 xlnx_uartlite_lowlevel_getc(struct xlnx_uartlite *regs)
{
	/* Wait until there is data in the FIFO */
	while (!xlnx_uartlite_lowlevel_can_getc(regs)) ;

	/* Read IO register */
	return (char)vmm_readl((void *)(&regs->rx_fifo));
}

bool xlnx_uartlite_lowlevel_can_putc(struct xlnx_uartlite *reg)
{
	if (vmm_readl(&reg->stat_reg) & UARTLITE_STAT_TX_FIFO_FULL)
		return FALSE;

	return TRUE;
}

void xlnx_uartlite_lowlevel_putc(struct xlnx_uartlite *reg, u8 ch)
{
	/* Wait until tx FIFO is not full */
	while (!xlnx_uartlite_lowlevel_can_putc(reg)) ;

	/* Send the character */
	vmm_writel(ch, (void *)&reg->tx_fifo);
}

static u32 xlnx_uartlite_tx(struct serial *p, u8 *src, size_t len)
{
	u32 i;
	struct xlnx_uartlite_priv *port = serial_tx_priv(p);

	for (i = 0; i < len; i++) {
		if (!xlnx_uartlite_lowlevel_can_putc(port->regs)) {
			break;
		}
		xlnx_uartlite_lowlevel_putc(port->regs, src[i]);
	}

	return i;
}

void xlnx_uartlite_lowlevel_init(struct xlnx_uartlite_priv *port)
{
	struct xlnx_uartlite *regs = port->regs;

	/* RX/TX reset, disable interrupt */
	vmm_writel(
		UARTLITE_CTRL_RST_RX_FIFO | \
		UARTLITE_CTRL_RST_TX_FIFO,
		&regs->ctrl_reg
	);
}

static vmm_irq_return_t xlnx_uartlite_irq_handler(int irq_no, void *pdev)
{
	u8 ch;
	struct xlnx_uartlite_priv *port = (struct xlnx_uartlite_priv *)pdev;
	struct xlnx_uartlite *regs = port->regs;

	/* Handle RX interrupt */
	while (xlnx_uartlite_lowlevel_can_getc(regs)) {
		ch = xlnx_uartlite_lowlevel_getc(regs);
		serial_rx(port->p, &ch, 1);
	}

	return VMM_IRQ_HANDLED;
}

static int xlnx_uartlite_driver_probe(struct vmm_device *dev)
{
	int rc;
	struct xlnx_uartlite_priv *port;

	port = vmm_zalloc(sizeof(struct xlnx_uartlite_priv));
	if (!port) {
		rc = VMM_ENOMEM;
		goto free_nothing;
	}

	rc = vmm_devtree_request_regmap(dev->of_node,
					(virtual_addr_t*)&port->regs,
					0, "Xilinx uartlite");
	if (rc) {
		goto free_port;
	}

	port->irq = vmm_devtree_irq_parse_map(dev->of_node, 0);
	if (!port->irq) {
		rc = VMM_ENODEV;
		goto free_reg;
	}
	if ((rc = vmm_host_irq_register(port->irq, dev->name,
					xlnx_uartlite_irq_handler, port))) {
		goto free_reg;
	}

	/* Call low-level init function */
	xlnx_uartlite_lowlevel_init(port);

	/* Create Serial Port */
	port->p = serial_create(dev, 256, xlnx_uartlite_tx, port);
	if (VMM_IS_ERR_OR_NULL(port->p)) {
		rc = VMM_PTR_ERR(port->p);
		goto free_irq;
	}

	/* Save port pointer */
	dev->priv = port;

	/* Enable Interrupt */
	// vmm_writel(UARTLITE_CTRL_ENABLE_INTR, &port->regs->ctrl_reg);

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

static int xlnx_uartlite_driver_remove(struct vmm_device *dev)
{
	struct xlnx_uartlite_priv *port = dev->priv;

	if (!port) {
		return VMM_OK;
	}

	/* Disable interrupts */
	// vmm_writel(0, &port->regs->ctrl_reg);

	/* Free-up resources */
	serial_destroy(port->p);
	vmm_host_irq_unregister(port->irq, port);
	vmm_devtree_regunmap_release(dev->of_node,
				     (virtual_addr_t)port->regs, 0);
	vmm_free(port);
	dev->priv = NULL;

	return VMM_OK;
}

static struct vmm_devtree_nodeid xlnx_uartlite_devid_table[] = {
	{ .compatible = "xilinx,uartlite" },
	{ .compatible = "xlnx,opb-uartlite-1.00.b" },
	{ .compatible = "xlnx,xps-uartlite-1.00.a" },
	{ /* end of list */ },
};

static struct vmm_driver xlnx_uartlite_driver = {
	.name = "xlnx_uartlite",
	.match_table = xlnx_uartlite_devid_table,
	.probe = xlnx_uartlite_driver_probe,
	.remove = xlnx_uartlite_driver_remove,
};

static int __init xlnx_uartlite_driver_init(void)
{
	return vmm_devdrv_register_driver(&xlnx_uartlite_driver);
}

static void __exit xlnx_uartlite_driver_exit(void)
{
	vmm_devdrv_unregister_driver(&xlnx_uartlite_driver);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
