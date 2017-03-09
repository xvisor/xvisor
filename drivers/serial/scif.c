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
#include <drv/serial/scif.h>

#define MODULE_DESC			"SuperH SCIF Serial Driver"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(SERIAL_IPRIORITY+1)
#define	MODULE_INIT			scif_driver_init
#define	MODULE_EXIT			scif_driver_exit

#define SCI_OF_DATA(type)		((void *)(type))
#define SCI_OF_REGTYPE(data)		((unsigned long)(data))

struct plat_sci_reg {
	u8 offset, size;
};

/* Helper for invalidating specific entries of an inherited map. */
#define sci_reg_invalid	{ .offset = 0, .size = 0 }

static const struct plat_sci_reg sci_regmap[SCIx_NR_REGTYPES][SCIx_NR_REGS] = {
	[SCIx_PROBE_REGTYPE] = {
		[0 ... SCIx_NR_REGS - 1] = sci_reg_invalid,
	},

	/*
	 * Common SCI definitions, dependent on the port's regshift
	 * value.
	 */
	[SCIx_SCI_REGTYPE] = {
		[SCSMR]		= { 0x00,  8 },
		[SCBRR]		= { 0x01,  8 },
		[SCSCR]		= { 0x02,  8 },
		[SCxTDR]	= { 0x03,  8 },
		[SCxSR]		= { 0x04,  8 },
		[SCxRDR]	= { 0x05,  8 },
		[SCFCR]		= sci_reg_invalid,
		[SCFDR]		= sci_reg_invalid,
		[SCTFDR]	= sci_reg_invalid,
		[SCRFDR]	= sci_reg_invalid,
		[SCSPTR]	= sci_reg_invalid,
		[SCLSR]		= sci_reg_invalid,
		[HSSRR]		= sci_reg_invalid,
		[SCPCR]		= sci_reg_invalid,
		[SCPDR]		= sci_reg_invalid,
		[SCDL]		= sci_reg_invalid,
		[SCCKS]		= sci_reg_invalid,
	},

	/*
	 * Common definitions for legacy IrDA ports, dependent on
	 * regshift value.
	 */
	[SCIx_IRDA_REGTYPE] = {
		[SCSMR]		= { 0x00,  8 },
		[SCBRR]		= { 0x01,  8 },
		[SCSCR]		= { 0x02,  8 },
		[SCxTDR]	= { 0x03,  8 },
		[SCxSR]		= { 0x04,  8 },
		[SCxRDR]	= { 0x05,  8 },
		[SCFCR]		= { 0x06,  8 },
		[SCFDR]		= { 0x07, 16 },
		[SCTFDR]	= sci_reg_invalid,
		[SCRFDR]	= sci_reg_invalid,
		[SCSPTR]	= sci_reg_invalid,
		[SCLSR]		= sci_reg_invalid,
		[HSSRR]		= sci_reg_invalid,
		[SCPCR]		= sci_reg_invalid,
		[SCPDR]		= sci_reg_invalid,
		[SCDL]		= sci_reg_invalid,
		[SCCKS]		= sci_reg_invalid,
	},

	/*
	 * Common SCIFA definitions.
	 */
	[SCIx_SCIFA_REGTYPE] = {
		[SCSMR]		= { 0x00, 16 },
		[SCBRR]		= { 0x04,  8 },
		[SCSCR]		= { 0x08, 16 },
		[SCxTDR]	= { 0x20,  8 },
		[SCxSR]		= { 0x14, 16 },
		[SCxRDR]	= { 0x24,  8 },
		[SCFCR]		= { 0x18, 16 },
		[SCFDR]		= { 0x1c, 16 },
		[SCTFDR]	= sci_reg_invalid,
		[SCRFDR]	= sci_reg_invalid,
		[SCSPTR]	= sci_reg_invalid,
		[SCLSR]		= sci_reg_invalid,
		[HSSRR]		= sci_reg_invalid,
		[SCPCR]		= { 0x30, 16 },
		[SCPDR]		= { 0x34, 16 },
		[SCDL]		= sci_reg_invalid,
		[SCCKS]		= sci_reg_invalid,
	},

	/*
	 * Common SCIFB definitions.
	 */
	[SCIx_SCIFB_REGTYPE] = {
		[SCSMR]		= { 0x00, 16 },
		[SCBRR]		= { 0x04,  8 },
		[SCSCR]		= { 0x08, 16 },
		[SCxTDR]	= { 0x40,  8 },
		[SCxSR]		= { 0x14, 16 },
		[SCxRDR]	= { 0x60,  8 },
		[SCFCR]		= { 0x18, 16 },
		[SCFDR]		= sci_reg_invalid,
		[SCTFDR]	= { 0x38, 16 },
		[SCRFDR]	= { 0x3c, 16 },
		[SCSPTR]	= sci_reg_invalid,
		[SCLSR]		= sci_reg_invalid,
		[HSSRR]		= sci_reg_invalid,
		[SCPCR]		= { 0x30, 16 },
		[SCPDR]		= { 0x34, 16 },
		[SCDL]		= sci_reg_invalid,
		[SCCKS]		= sci_reg_invalid,
	},

	/*
	 * Common SH-2(A) SCIF definitions for ports with FIFO data
	 * count registers.
	 */
	[SCIx_SH2_SCIF_FIFODATA_REGTYPE] = {
		[SCSMR]		= { 0x00, 16 },
		[SCBRR]		= { 0x04,  8 },
		[SCSCR]		= { 0x08, 16 },
		[SCxTDR]	= { 0x0c,  8 },
		[SCxSR]		= { 0x10, 16 },
		[SCxRDR]	= { 0x14,  8 },
		[SCFCR]		= { 0x18, 16 },
		[SCFDR]		= { 0x1c, 16 },
		[SCTFDR]	= sci_reg_invalid,
		[SCRFDR]	= sci_reg_invalid,
		[SCSPTR]	= { 0x20, 16 },
		[SCLSR]		= { 0x24, 16 },
		[HSSRR]		= sci_reg_invalid,
		[SCPCR]		= sci_reg_invalid,
		[SCPDR]		= sci_reg_invalid,
		[SCDL]		= sci_reg_invalid,
		[SCCKS]		= sci_reg_invalid,
	},

	/*
	 * Common SH-3 SCIF definitions.
	 */
	[SCIx_SH3_SCIF_REGTYPE] = {
		[SCSMR]		= { 0x00,  8 },
		[SCBRR]		= { 0x02,  8 },
		[SCSCR]		= { 0x04,  8 },
		[SCxTDR]	= { 0x06,  8 },
		[SCxSR]		= { 0x08, 16 },
		[SCxRDR]	= { 0x0a,  8 },
		[SCFCR]		= { 0x0c,  8 },
		[SCFDR]		= { 0x0e, 16 },
		[SCTFDR]	= sci_reg_invalid,
		[SCRFDR]	= sci_reg_invalid,
		[SCSPTR]	= sci_reg_invalid,
		[SCLSR]		= sci_reg_invalid,
		[HSSRR]		= sci_reg_invalid,
		[SCPCR]		= sci_reg_invalid,
		[SCPDR]		= sci_reg_invalid,
		[SCDL]		= sci_reg_invalid,
		[SCCKS]		= sci_reg_invalid,
	},

	/*
	 * Common SH-4(A) SCIF(B) definitions.
	 */
	[SCIx_SH4_SCIF_REGTYPE] = {
		[SCSMR]		= { 0x00, 16 },
		[SCBRR]		= { 0x04,  8 },
		[SCSCR]		= { 0x08, 16 },
		[SCxTDR]	= { 0x0c,  8 },
		[SCxSR]		= { 0x10, 16 },
		[SCxRDR]	= { 0x14,  8 },
		[SCFCR]		= { 0x18, 16 },
		[SCFDR]		= { 0x1c, 16 },
		[SCTFDR]	= sci_reg_invalid,
		[SCRFDR]	= sci_reg_invalid,
		[SCSPTR]	= { 0x20, 16 },
		[SCLSR]		= { 0x24, 16 },
		[HSSRR]		= sci_reg_invalid,
		[SCPCR]		= sci_reg_invalid,
		[SCPDR]		= sci_reg_invalid,
		[SCDL]		= sci_reg_invalid,
		[SCCKS]		= sci_reg_invalid,
	},

	/*
	 * Common SCIF definitions for ports with a Baud Rate Generator for
	 * External Clock (BRG).
	 */
	[SCIx_SH4_SCIF_BRG_REGTYPE] = {
		[SCSMR]		= { 0x00, 16 },
		[SCBRR]		= { 0x04,  8 },
		[SCSCR]		= { 0x08, 16 },
		[SCxTDR]	= { 0x0c,  8 },
		[SCxSR]		= { 0x10, 16 },
		[SCxRDR]	= { 0x14,  8 },
		[SCFCR]		= { 0x18, 16 },
		[SCFDR]		= { 0x1c, 16 },
		[SCTFDR]	= sci_reg_invalid,
		[SCRFDR]	= sci_reg_invalid,
		[SCSPTR]	= { 0x20, 16 },
		[SCLSR]		= { 0x24, 16 },
		[HSSRR]		= sci_reg_invalid,
		[SCPCR]		= sci_reg_invalid,
		[SCPDR]		= sci_reg_invalid,
		[SCDL]		= { 0x30, 16 },
		[SCCKS]		= { 0x34, 16 },
	},

	/*
	 * Common HSCIF definitions.
	 */
	[SCIx_HSCIF_REGTYPE] = {
		[SCSMR]		= { 0x00, 16 },
		[SCBRR]		= { 0x04,  8 },
		[SCSCR]		= { 0x08, 16 },
		[SCxTDR]	= { 0x0c,  8 },
		[SCxSR]		= { 0x10, 16 },
		[SCxRDR]	= { 0x14,  8 },
		[SCFCR]		= { 0x18, 16 },
		[SCFDR]		= { 0x1c, 16 },
		[SCTFDR]	= sci_reg_invalid,
		[SCRFDR]	= sci_reg_invalid,
		[SCSPTR]	= { 0x20, 16 },
		[SCLSR]		= { 0x24, 16 },
		[HSSRR]		= { 0x40, 16 },
		[SCPCR]		= sci_reg_invalid,
		[SCPDR]		= sci_reg_invalid,
		[SCDL]		= { 0x30, 16 },
		[SCCKS]		= { 0x34, 16 },
	},

	/*
	 * Common SH-4(A) SCIF(B) definitions for ports without an SCSPTR
	 * register.
	 */
	[SCIx_SH4_SCIF_NO_SCSPTR_REGTYPE] = {
		[SCSMR]		= { 0x00, 16 },
		[SCBRR]		= { 0x04,  8 },
		[SCSCR]		= { 0x08, 16 },
		[SCxTDR]	= { 0x0c,  8 },
		[SCxSR]		= { 0x10, 16 },
		[SCxRDR]	= { 0x14,  8 },
		[SCFCR]		= { 0x18, 16 },
		[SCFDR]		= { 0x1c, 16 },
		[SCTFDR]	= sci_reg_invalid,
		[SCRFDR]	= sci_reg_invalid,
		[SCSPTR]	= sci_reg_invalid,
		[SCLSR]		= { 0x24, 16 },
		[HSSRR]		= sci_reg_invalid,
		[SCPCR]		= sci_reg_invalid,
		[SCPDR]		= sci_reg_invalid,
		[SCDL]		= sci_reg_invalid,
		[SCCKS]		= sci_reg_invalid,
	},

	/*
	 * Common SH-4(A) SCIF(B) definitions for ports with FIFO data
	 * count registers.
	 */
	[SCIx_SH4_SCIF_FIFODATA_REGTYPE] = {
		[SCSMR]		= { 0x00, 16 },
		[SCBRR]		= { 0x04,  8 },
		[SCSCR]		= { 0x08, 16 },
		[SCxTDR]	= { 0x0c,  8 },
		[SCxSR]		= { 0x10, 16 },
		[SCxRDR]	= { 0x14,  8 },
		[SCFCR]		= { 0x18, 16 },
		[SCFDR]		= { 0x1c, 16 },
		[SCTFDR]	= { 0x1c, 16 },	/* aliased to SCFDR */
		[SCRFDR]	= { 0x20, 16 },
		[SCSPTR]	= { 0x24, 16 },
		[SCLSR]		= { 0x28, 16 },
		[HSSRR]		= sci_reg_invalid,
		[SCPCR]		= sci_reg_invalid,
		[SCPDR]		= sci_reg_invalid,
		[SCDL]		= sci_reg_invalid,
		[SCCKS]		= sci_reg_invalid,
	},

	/*
	 * SH7705-style SCIF(B) ports, lacking both SCSPTR and SCLSR
	 * registers.
	 */
	[SCIx_SH7705_SCIF_REGTYPE] = {
		[SCSMR]		= { 0x00, 16 },
		[SCBRR]		= { 0x04,  8 },
		[SCSCR]		= { 0x08, 16 },
		[SCxTDR]	= { 0x20,  8 },
		[SCxSR]		= { 0x14, 16 },
		[SCxRDR]	= { 0x24,  8 },
		[SCFCR]		= { 0x18, 16 },
		[SCFDR]		= { 0x1c, 16 },
		[SCTFDR]	= sci_reg_invalid,
		[SCRFDR]	= sci_reg_invalid,
		[SCSPTR]	= sci_reg_invalid,
		[SCLSR]		= sci_reg_invalid,
		[HSSRR]		= sci_reg_invalid,
		[SCPCR]		= sci_reg_invalid,
		[SCPDR]		= sci_reg_invalid,
		[SCDL]		= sci_reg_invalid,
		[SCCKS]		= sci_reg_invalid,
	},
};

#define sci_getreg(regtype, offset)		(sci_regmap[regtype] + offset)

/*
 * The "offset" here is rather misleading, in that it refers to an enum
 * value relative to the port mapping rather than the fixed offset
 * itself, which needs to be manually retrieved from the platform's
 * register map for the given port.
 */
static unsigned int sci_serial_in(virtual_addr_t base, unsigned long regtype,
					int offset)
{
	const struct plat_sci_reg *reg = sci_getreg(regtype, offset);

	if (reg->size == 8)
		return vmm_readb((void *) (base + reg->offset));
	else if (reg->size == 16)
		return vmm_readw((void *) (base + reg->offset));

	return 0;
}

static void sci_serial_out(virtual_addr_t base, unsigned long regtype,
				int offset, int value)
{
	const struct plat_sci_reg *reg = sci_getreg(regtype, offset);

	if (reg->size == 8)
		vmm_writeb(value, (void *) (base + reg->offset));
	else if (reg->size == 16)
		vmm_writew(value, (void *) (base + reg->offset));
}

bool scif_lowlevel_can_getc(virtual_addr_t base, unsigned long regtype)
{
	u16 mask = SCFSR_RDF | SCFSR_DR;

	if (sci_serial_in(base, regtype, SCxSR) & mask) {
		return TRUE;
	}

	return FALSE;
}

u8 scif_lowlevel_getc(virtual_addr_t base, unsigned long regtype)
{
	u8 data;
	u16 scfsr;

	/* Wait until there is data in the FIFO */
	while (!scif_lowlevel_can_getc(base, regtype)) ;

	/* Read RX data */
	data = sci_serial_in(base, regtype, SCxRDR);

	/* Clear required RX flags */
	scfsr = sci_serial_in(base, regtype, SCxSR);
	scfsr &= ~(SCFSR_RDF | SCFSR_DR);
	sci_serial_out(base, regtype, SCxSR, scfsr);

	return data;
}

bool scif_lowlevel_can_putc(virtual_addr_t base,
				unsigned long regtype)
{
	if (sci_serial_in(base, regtype, SCxSR) & SCFSR_TEND) {
		return TRUE;
	}

	return FALSE;
}

void scif_lowlevel_putc(virtual_addr_t base, unsigned long regtype,
			u8 ch)
{
	u16 scfsr;

	/* Wait until there is space in the FIFO */
	while (!scif_lowlevel_can_putc(base, regtype)) ;

	/* Send the character */
	sci_serial_out(base, regtype, SCxTDR, ch);

	/* Clear required TX flags */
	scfsr = sci_serial_in(base, regtype, SCxSR);
	scfsr &= ~(SCFSR_TEND | SCFSR_TDFE);
	sci_serial_out(base, regtype, SCxSR, scfsr);
}

void scif_lowlevel_init(virtual_addr_t base, unsigned long regtype,
			u32 baudrate, u32 input_clock,
			bool use_internal_clock)
{
	u16 tmp;

	/*
	 * Wait until last bit has been transmitted. This is needed for
	 * a smooth transition when we come from early prints
	 */
	while (!(sci_serial_in(base, regtype, SCxSR) & SCFSR_TEND));

	/* Disable TX/RX parts and all interrupts */
	sci_serial_out(base, regtype, SCSCR, 0);

	/* Reset TX/RX FIFOs */
	sci_serial_out(base, regtype, SCFCR, SCFCR_RFRST | SCFCR_TFRST);

	/* Clear all errors and flags */
	sci_serial_in(base, regtype, SCxSR);
	sci_serial_out(base, regtype, SCxSR, 0);
	sci_serial_in(base, regtype, SCLSR);
	sci_serial_out(base, regtype, SCLSR, 0);

	/* Setup trigger level for TX/RX FIFOs */
	sci_serial_out(base, regtype, SCFCR, SCFCR_RTRG11 | SCFCR_TTRG11);

	/* Enable TX/RX parts */
	tmp = sci_serial_in(base, regtype, SCSCR);
	tmp |= (SCSCR_TE | SCSCR_RE);
	sci_serial_out(base, regtype, SCSCR, tmp);

	/* Clear all errors */
	if (sci_serial_in(base, regtype, SCxSR) & SCIF_ERRORS)
		sci_serial_out(base, regtype, SCxSR, ~SCIF_ERRORS);
	if (sci_serial_in(base, regtype, SCLSR) & SCLSR_ORER)
		sci_serial_out(base, regtype, SCLSR, 0);
}

struct scif_port {
	struct serial *p;
	virtual_addr_t base;
	unsigned long regtype;
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
	unsigned long regtype = port->regtype;

	ctrl = sci_serial_in(base, regtype, SCSCR);
	status = sci_serial_in(base, regtype, SCxSR) & ~SCFSR_TEND;

	/* Ignore next flag if TX Interrupt is disabled */
	if (!(ctrl & SCSCR_TIE))
		status &= ~SCFSR_TDFE;

	while (status != 0) {
		/* Pull-out bytes from RX FIFO */
		while (scif_lowlevel_can_getc(base, regtype)) {
			ch = scif_lowlevel_getc(base, regtype);
			serial_rx(port->p, &ch, 1);
		}

		/* Error Interrupt */
		if (sci_serial_in(base, regtype, SCxSR) & SCIF_ERRORS)
			sci_serial_out(base, regtype, SCxSR, ~SCIF_ERRORS);
		if (sci_serial_in(base, regtype, SCLSR) & SCLSR_ORER)
			sci_serial_out(base, regtype, SCLSR, 0);

		ctrl = sci_serial_in(base, regtype, SCSCR);
		status = sci_serial_in(base, regtype, SCxSR) & ~SCFSR_TEND;
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
		if (!scif_lowlevel_can_putc(port->base, port->regtype)) {
			break;
		}
		scif_lowlevel_putc(port->base, port->regtype, src[i]);
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

	port->regtype = SCI_OF_REGTYPE(devid->data);

	/* Call low-level init function */
	scif_lowlevel_init(port->base, port->regtype, port->baudrate,
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
	port->mask = sci_serial_in(port->base, port->regtype, SCSCR);
	port->mask |= (SCSCR_RIE | SCSCR_REIE);
	sci_serial_out(port->base, port->regtype, SCSCR, port->mask);

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
	sci_serial_out(port->base, port->regtype, SCSCR, port->mask);

	/* Free-up resources */
	serial_destroy(port->p);
	vmm_host_irq_unregister(port->irq, port);
	vmm_devtree_regunmap_release(dev->of_node, port->base, 0);
	vmm_free(port);
	dev->priv = NULL;

	return VMM_OK;
}

static struct vmm_devtree_nodeid scif_devid_table[] = {
        /* Generic types */
        {
                .compatible = "renesas,scif",
                .data = SCI_OF_DATA(SCIx_SH4_SCIF_BRG_REGTYPE),
        }, {
                .compatible = "renesas,scifa",
                .data = SCI_OF_DATA(SCIx_SCIFA_REGTYPE),
	},
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
