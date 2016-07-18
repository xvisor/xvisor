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
 * @file scif.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief source file for SuperH SCIF serial port driver.
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
#include <drv/scif.h>

#define MODULE_DESC			"SuperH SCIF Serial Driver"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(SERIAL_IPRIORITY+1)
#define	MODULE_INIT			scif_driver_init
#define	MODULE_EXIT			scif_driver_exit

bool scif_lowlevel_can_getc(virtual_addr_t base)
{
	u16 mask = SCFSR_RDF | SCFSR_DR;

	if (vmm_readw((void *)(base + SCIF_SCFSR)) & mask) {
		return TRUE;
	}

	return FALSE;
}

u8 scif_lowlevel_getc(virtual_addr_t base)
{
	u8 data;
	u16 scfsr;

	/* Wait until there is data in the FIFO */
	while (!scif_lowlevel_can_getc(base)) ;

	/* Read RX data */
	data = vmm_readb((void *)(base + SCIF_SCFRDR));

	/* Clear required RX flags */
	scfsr = vmm_readw((void *)(base + SCIF_SCFSR));
	scfsr &= ~(SCFSR_RDF | SCFSR_DR);
	vmm_writew(scfsr, (void *)(base + SCIF_SCFSR));

	return data;
}

bool scif_lowlevel_can_putc(virtual_addr_t base)
{
	if (vmm_readw((void *)(base + SCIF_SCFSR)) & SCFSR_TEND) {
		return TRUE;
	}

	return FALSE;
}

void scif_lowlevel_putc(virtual_addr_t base, u8 ch)
{
	u16 scfsr;

	/* Wait until there is space in the FIFO */
	while (!scif_lowlevel_can_putc(base)) ;

	/* Send the character */
	vmm_writeb(ch, (void *)(base + SCIF_SCFTDR));

	/* Clear required TX flags */
	scfsr = vmm_readw((void *)(base + SCIF_SCFSR));
	scfsr &= ~(SCFSR_TEND | SCFSR_TDFE);
	vmm_writew(scfsr, (void *)(base + SCIF_SCFSR));
}

void scif_lowlevel_init(virtual_addr_t base, u32 baudrate,
			u32 input_clock, bool use_internal_clock)
{
	u16 tmp;

	/*
	 * Wait until last bit has been transmitted. This is needed for
	 * a smooth transition when we come from early prints
	 */
	while (!(vmm_readw((void *)(base + SCIF_SCFSR)) & SCFSR_TEND));

	/* Disable TX/RX parts and all interrupts */
	vmm_writew(0, (void *)(base + SCIF_SCSCR));

	/* Reset TX/RX FIFOs */
	vmm_writew(SCFCR_RFRST | SCFCR_TFRST, (void *)(base + SCIF_SCFCR));

	/* Clear all errors and flags */
	vmm_readw((void *)(base + SCIF_SCFSR));
	vmm_writew(0, (void *)(base + SCIF_SCFSR));
	vmm_readw((void *)(base + SCIF_SCLSR));
	vmm_writew(0, (void *)(base + SCIF_SCLSR));

	/* Setup trigger level for TX/RX FIFOs */
	vmm_writew(SCFCR_RTRG11 | SCFCR_TTRG11, (void *)(base + SCIF_SCFCR));

	/* Enable TX/RX parts */
	tmp = vmm_readw((void *)(base + SCIF_SCSCR));
	tmp |= (SCSCR_TE | SCSCR_RE);
	vmm_writew(tmp, (void *)(base + SCIF_SCSCR));

	/* Clear all errors */
	if (vmm_readw((void *)(base + SCIF_SCFSR)) & SCIF_ERRORS)
		vmm_writew(~SCIF_ERRORS, (void *)(base + SCIF_SCFSR));
	if (vmm_readw((void *)(base + SCIF_SCLSR)) & SCLSR_ORER)
		vmm_writew(0, (void *)(base + SCIF_SCLSR));
}

struct scif_port {
	struct serial *p;
	virtual_addr_t base;
	u32 baudrate;
	u32 input_clock;
	bool use_internal_clock;
	u32 irq;
	u16 mask;
};

static vmm_irq_return_t scif_irq_handler(int irq_no, void *dev)
{
	u8 ch;
	u16 ctrl, status;
	struct scif_port *port = (struct scif_port *)dev;
	virtual_addr_t base = port->base;

	ctrl = vmm_readw((void *)(base + SCIF_SCSCR));
	status = vmm_readw((void *)(base + SCIF_SCFSR)) & ~SCFSR_TEND;
	/* Ignore next flag if TX Interrupt is disabled */
	if (!(ctrl & SCSCR_TIE))
		status &= ~SCFSR_TDFE;

	while (status != 0) {
		/* Pull-out bytes from RX FIFO */
		while (scif_lowlevel_can_getc(base)) {
			ch = scif_lowlevel_getc(base);
			serial_rx(port->p, &ch, 1);
		}

		/* Error Interrupt */
		if (vmm_readw((void *)(base + SCIF_SCFSR)) & SCIF_ERRORS)
			vmm_writew(~SCIF_ERRORS, (void *)(base + SCIF_SCFSR));
		if (vmm_readw((void *)(base + SCIF_SCLSR)) & SCLSR_ORER)
			vmm_writew(0, (void *)(base + SCIF_SCLSR));

		ctrl = vmm_readw((void *)(base + SCIF_SCSCR));
		status = vmm_readw((void *)(base + SCIF_SCFSR)) & ~SCFSR_TEND;
		/* Ignore next flag if TX Interrupt is disabled */
		if ( !(ctrl & SCSCR_TIE) )
			status &= ~SCFSR_TDFE;
	}

	return VMM_IRQ_HANDLED;
}

static u32 scif_tx(struct serial *p, u8 *src, size_t len)
{
	u32 i;
	struct scif_port *port = serial_tx_priv(p);

	for (i = 0; i < len; i++) {
		if (!scif_lowlevel_can_putc(port->base)) {
			break;
		}
		scif_lowlevel_putc(port->base, src[i]);
	}

	return i;
}

static int scif_driver_probe(struct vmm_device *dev,
			      const struct vmm_devtree_nodeid *devid)
{
	int rc;
	struct scif_port *port;

	port = vmm_zalloc(sizeof(struct scif_port));
	if (!port) {
		rc = VMM_ENOMEM;
		goto free_nothing;
	}

	rc = vmm_devtree_request_regmap(dev->of_node, &port->base, 0,
					"SCIF UART");
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

	if (vmm_devtree_getattr(dev->of_node, "clock-internal")) {
		port->use_internal_clock = TRUE;
	} else {
		port->use_internal_clock = FALSE;
	}

	port->irq = vmm_devtree_irq_parse_map(dev->of_node, 0);
	if (!port->irq) {
		rc = VMM_ENODEV;
		goto free_reg;
	}
	if ((rc = vmm_host_irq_register(port->irq, dev->name,
					scif_irq_handler, port))) {
		goto free_reg;
	}

	/* Call low-level init function */
	scif_lowlevel_init(port->base, port->baudrate,
			   port->input_clock, port->use_internal_clock);

	/* Create Serial Port */
	port->p = serial_create(dev, 256, scif_tx, port);
	if (VMM_IS_ERR_OR_NULL(port->p)) {
		rc = VMM_PTR_ERR(port->p);
		goto free_irq;
	}

	/* Save port pointer */
	dev->priv = port;

	/* Enable RX and Error interrupts */
	port->mask = vmm_readw((void *)(port->base + SCIF_SCSCR));
	port->mask |= (SCSCR_RIE | SCSCR_REIE);
	vmm_writew(port->mask, (void *)(port->base + SCIF_SCSCR));

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

static int scif_driver_remove(struct vmm_device *dev)
{
	struct scif_port *port = dev->priv;

	if (!port) {
		return VMM_OK;
	}

	/* Mask RX and Error interrupts */
	port->mask &= ~(SCSCR_RIE | SCSCR_REIE);
	vmm_writew(port->mask, (void *)(port->base + SCIF_SCSCR));

	/* Free-up resources */
	serial_destroy(port->p);
	vmm_host_irq_unregister(port->irq, port);
	vmm_devtree_regunmap_release(dev->of_node, port->base, 0);
	vmm_free(port);
	dev->priv = NULL;

	return VMM_OK;
}

static struct vmm_devtree_nodeid scif_devid_table[] = {
	{ .compatible = "renesas,scif" },
	{ /* end of list */ },
};

static struct vmm_driver scif_driver = {
	.name = "scif_serial",
	.match_table = scif_devid_table,
	.probe = scif_driver_probe,
	.remove = scif_driver_remove,
};

static int __init scif_driver_init(void)
{
	return vmm_devdrv_register_driver(&scif_driver);
}

static void __exit scif_driver_exit(void)
{
	vmm_devdrv_unregister_driver(&scif_driver);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
