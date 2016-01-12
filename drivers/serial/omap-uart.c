/**
 * Copyright (c) 2012 Sukanto Ghosh.
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
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief source file for OMAP UART serial port driver.
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
#include <drv/omap-uart.h>

#define MODULE_DESC			"OMAP UART Driver"
#define MODULE_AUTHOR			"Sukanto Ghosh"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(SERIAL_IPRIORITY+1)
#define	MODULE_INIT			omap_uart_driver_init
#define	MODULE_EXIT			omap_uart_driver_exit

struct omap_uart_port {
	struct serial *p;
	virtual_addr_t base;
	u32 baudrate;
	u32 input_clock;
	u32 reg_shift;
	u32 irq;
	u32 lcr;
	u32 mcr;
	u32 ier;
	u32 fcr;
	u32 efr;
};

#define omap_serial_in(reg)		\
		(vmm_in_8((u8 *)REG_##reg(base, 1 << reg_shift)))
#define omap_serial_out(reg, val)	\
		vmm_out_8((u8 *)REG_##reg(base, 1 << reg_shift), (val))

bool omap_uart_lowlevel_can_getc(virtual_addr_t base, u32 reg_shift)
{
	if (omap_serial_in(UART_LSR) & UART_LSR_DR) {
		return TRUE;
	}
	return FALSE;
}

u8 omap_uart_lowlevel_getc(virtual_addr_t base, u32 reg_shift)
{
	while (!omap_uart_lowlevel_can_getc(base, reg_shift));

	return (omap_serial_in(UART_RBR));
}

#define BOTH_EMPTY (UART_LSR_TEMT | UART_LSR_THRE)

bool omap_uart_lowlevel_can_putc(virtual_addr_t base, u32 reg_shift)
{
	if ((omap_serial_in(UART_LSR) & BOTH_EMPTY) == BOTH_EMPTY) {
		return TRUE;
	}
	return FALSE;
}

void omap_uart_lowlevel_putc(virtual_addr_t base, u32 reg_shift, u8 ch)
{
	while (!omap_uart_lowlevel_can_putc(base, reg_shift));

	omap_serial_out(UART_THR, ch);
}

void omap_uart_lowlevel_init(virtual_addr_t base, u32 reg_shift,
				u32 baudrate, u32 input_clock)
{
	u16 bdiv;
	bdiv = udiv32(input_clock, (16 * baudrate));

	/* clear interrupt enable reg */
	omap_serial_out(UART_IER, 0);

	/* set mode select to disabled before dll/dlh */
	omap_serial_out(UART_OMAP_MDR1, UART_OMAP_MDR1_DISABLE);
	omap_serial_out(UART_LCR, UART_LCR_CONF_MODE_A);
	omap_serial_out(UART_DLL, 0);
	omap_serial_out(UART_DLM, 0);
	omap_serial_out(UART_LCR, 0);

	/* no modem control DTR RTS */
	omap_serial_out(UART_MCR, 0);

	/* enable FIFO */
	omap_serial_out(UART_FCR, UART_FCR_R_TRIG_00 | UART_FCR_T_TRIG_00 | \
			UART_FCR_CLEAR_XMIT | UART_FCR_CLEAR_RCVR | UART_FCR_ENABLE_FIFO);

	/* set baudrate divisor */
	omap_serial_out(UART_LCR, UART_LCR_CONF_MODE_B);
	omap_serial_out(UART_DLL, bdiv & 0xFF);
	omap_serial_out(UART_DLM, (bdiv >> 8) & 0xFF);
	omap_serial_out(UART_LCR, UART_LCR_WLEN8);

	/* set mode select to 16x mode  */
	omap_serial_out(UART_OMAP_MDR1, UART_OMAP_MDR1_16X_MODE);
}

#undef omap_serial_in
#undef omap_serial_out
#define omap_serial_in(port, reg)	\
	(vmm_in_8((u8 *)REG_##reg(port->base, 1 << port->reg_shift)))
#define omap_serial_out(port, reg, val)	\
	vmm_out_8((u8 *)REG_##reg(port->base, 1 << port->reg_shift), (val))

void uart_configure_xonxoff(struct omap_uart_port *port)
{
	u16 efr;

	port->lcr = omap_serial_in(port, UART_LCR);
	omap_serial_out(port, UART_LCR, UART_LCR_CONF_MODE_B);
	port->efr = omap_serial_in(port, UART_EFR);
	omap_serial_out(port, UART_EFR, port->efr & ~UART_EFR_ECB);

	omap_serial_out(port, UART_XON1, 0x11);
	omap_serial_out(port, UART_XOFF1, 0x13);

	/* clear SW control mode bits */
	efr = port->efr;
	efr &= OMAP_UART_SW_CLR;

#if 0  /* Enable if required */
	/*
	 * IXON Flag:
	 * Enable XON/XOFF flow control on output.
	 * Transmit XON1, XOFF1
	 */
	efr |= OMAP_UART_SW_TX;

	/*
	 * IXOFF Flag:
	 * Enable XON/XOFF flow control on input.
	 * Receiver compares XON1, XOFF1.
	 */
	efr |= OMAP_UART_SW_RX;
#endif

	omap_serial_out(port, UART_EFR, port->efr | UART_EFR_ECB);
	omap_serial_out(port, UART_LCR, UART_LCR_CONF_MODE_A);

	port->mcr = omap_serial_in(port, UART_MCR);
	omap_serial_out(port, UART_MCR, port->mcr | UART_MCR_TCRTLR);

	omap_serial_out(port, UART_LCR, UART_LCR_CONF_MODE_B);
	omap_serial_out(port, UART_TI752_TCR, OMAP_UART_TCR_TRIG);
	/* Enable special char function UARTi.EFR_REG[5] and
	 * load the new software flow control mode IXON or IXOFF
	 * and restore the UARTi.EFR_REG[4] ENHANCED_EN value.
	 */
	omap_serial_out(port, UART_EFR, efr | UART_EFR_SCD);
	omap_serial_out(port, UART_LCR, UART_LCR_CONF_MODE_A);

	omap_serial_out(port, UART_MCR, port->mcr & ~UART_MCR_TCRTLR);
	omap_serial_out(port, UART_LCR, port->lcr);
}

static int omap_uart_startup_configure(struct omap_uart_port *port)
{
	u16 bdiv, cval;
	bdiv = udiv32(port->input_clock, (16 * port->baudrate));

        /*
         * Clear the FIFO buffers and disable them.
         */
        omap_serial_out(port, UART_FCR, UART_FCR_ENABLE_FIFO);
        omap_serial_out(port, UART_FCR, UART_FCR_ENABLE_FIFO |
                       UART_FCR_CLEAR_RCVR | UART_FCR_CLEAR_XMIT);
        omap_serial_out(port, UART_FCR, 0);

        /*
         * Clear the interrupt registers.
         */
        (void) omap_serial_in(port, UART_LSR);
        if (omap_serial_in(port, UART_LSR) & UART_LSR_DR)
                (void) omap_serial_in(port, UART_RBR);
        (void) omap_serial_in(port, UART_IIR);
        (void) omap_serial_in(port, UART_MSR);

        cval = UART_LCR_WLEN8;

        /*
         * Now, initialize the UART
         */
        omap_serial_out(port, UART_LCR, cval);

        /*
         * Finally, enable interrupts
         */
        port->ier = UART_IER_RLSI | UART_IER_RDI;
        omap_serial_out(port, UART_IER, port->ier);

        /* Enable module level wake port */
        omap_serial_out(port, UART_OMAP_WER, OMAP_UART_WER_MOD_WKUP);


        port->fcr = UART_FCR_R_TRIG_01 | UART_FCR_T_TRIG_01 |
                        UART_FCR_ENABLE_FIFO;

        port->ier &= ~UART_IER_MSI;

        omap_serial_out(port, UART_IER, port->ier);
        omap_serial_out(port, UART_LCR, cval);         /* reset DLAB */

        /* FCR can be changed only when the
         * baud clock is not running
         * DLL_REG and DLH_REG set to 0.
         */
        omap_serial_out(port, UART_LCR, UART_LCR_CONF_MODE_A);
        omap_serial_out(port, UART_DLL, 0);
        omap_serial_out(port, UART_DLM, 0);
        omap_serial_out(port, UART_LCR, 0);

        omap_serial_out(port, UART_LCR, UART_LCR_CONF_MODE_B);

        port->efr = omap_serial_in(port, UART_EFR);
        omap_serial_out(port, UART_EFR, port->efr | UART_EFR_ECB);

        omap_serial_out(port, UART_LCR, UART_LCR_CONF_MODE_A);
        port->mcr = omap_serial_in(port, UART_MCR);
        omap_serial_out(port, UART_MCR, port->mcr | UART_MCR_TCRTLR);

        /* FIFO ENABLE, DMA MODE */
        omap_serial_out(port, UART_FCR, port->fcr);
        omap_serial_out(port, UART_LCR, UART_LCR_CONF_MODE_B);

        omap_serial_out(port, UART_EFR, port->efr);
        omap_serial_out(port, UART_LCR, UART_LCR_CONF_MODE_A);
        omap_serial_out(port, UART_MCR, port->mcr);

        /* Protocol, Baud Rate, and Interrupt Settings */

        omap_serial_out(port, UART_OMAP_MDR1, UART_OMAP_MDR1_DISABLE);
        omap_serial_out(port, UART_LCR, UART_LCR_CONF_MODE_B);

        port->efr = omap_serial_in(port, UART_EFR);
        omap_serial_out(port, UART_EFR, port->efr | UART_EFR_ECB);

        omap_serial_out(port, UART_LCR, 0);
        omap_serial_out(port, UART_IER, 0);
        omap_serial_out(port, UART_LCR, UART_LCR_CONF_MODE_B);

        omap_serial_out(port, UART_DLL, bdiv & 0xff);          /* LS of divisor */
        omap_serial_out(port, UART_DLM, bdiv >> 8);            /* MS of divisor */

        omap_serial_out(port, UART_LCR, 0);
        omap_serial_out(port, UART_IER, port->ier);
        omap_serial_out(port, UART_LCR, UART_LCR_CONF_MODE_B);

        omap_serial_out(port, UART_EFR, port->efr);
        omap_serial_out(port, UART_LCR, cval);

        omap_serial_out(port, UART_OMAP_MDR1, UART_OMAP_MDR1_16X_MODE);

        omap_serial_out(port, UART_MCR, port->mcr);

	uart_configure_xonxoff(port);

	return VMM_OK;
}

static vmm_irq_return_t omap_uart_irq_handler(int irq_no, void *dev)
{
	u16 iir, lsr;
	struct omap_uart_port *port = (struct omap_uart_port *)dev;

        iir = omap_serial_in(port, UART_IIR);
        if (iir & UART_IIR_NO_INT)
                return VMM_IRQ_NONE;

        lsr = omap_serial_in(port, UART_LSR);

	/* Handle RX FIFO not empty */
         if (iir & (UART_IIR_RLSI | UART_IIR_RTO | UART_IIR_RDI)) {
		if (lsr & UART_LSR_DR) {
			do {
				u8 ch = omap_serial_in(port, UART_RBR);
				serial_rx(port->p, &ch, 1);
			} while (omap_serial_in(port, UART_LSR) &
					      (UART_LSR_DR | UART_LSR_OE));
		} else if (lsr & (UART_LSR_OE | \
				  UART_LSR_PE | \
				  UART_LSR_BI | UART_LSR_FE) ) {
			while (1);
		}
        }

	omap_serial_out(port, UART_IER, port->ier);

	return VMM_IRQ_HANDLED;
}

static u32 omap_uart_tx(struct serial *p, u8 *src, size_t len)
{
	u32 i;
	struct omap_uart_port *port = serial_tx_priv(p);

	for (i = 0; i < len; i++) {
		if (!omap_uart_lowlevel_can_putc(port->base, port->reg_shift)) {
			break;
		}
		omap_uart_lowlevel_putc(port->base, port->reg_shift, src[i]);
	}

	return i;
}

static int omap_uart_driver_probe(struct vmm_device *dev,
				  const struct vmm_devtree_nodeid *devid)
{
	int rc;
	struct omap_uart_port *port;
	
	port = vmm_zalloc(sizeof(struct omap_uart_port));
	if (!port) {
		rc = VMM_ENOMEM;
		goto free_nothing;
	}

	rc = vmm_devtree_request_regmap(dev->of_node, &port->base, 0,
					"omap-uart");
	if (rc) {
		goto free_port;
	}

	if (vmm_devtree_read_u32(dev->of_node, "reg-shift",
				 &port->reg_shift)) {
		port->reg_shift = 0;
	}

	if (vmm_devtree_read_u32(dev->of_node, "baudrate",
				 &port->baudrate)) {
		port->baudrate = 115200;
	}

	rc = vmm_devtree_clock_frequency(dev->of_node,
					 &port->input_clock);
	if (rc) {
		goto free_reg;
	}

	omap_uart_startup_configure(port);

	port->irq = vmm_devtree_irq_parse_map(dev->of_node, 0);
	if (!port->irq) {
		rc = VMM_ENODEV;
		goto free_reg;
	}
	if ((rc = vmm_host_irq_register(port->irq, dev->name,
					omap_uart_irq_handler, port))) {
		goto free_reg;
	}

	/* Create Serial Port */
	port->p = serial_create(dev, 256, omap_uart_tx, port);
	if (VMM_IS_ERR_OR_NULL(port->p)) {
		rc = VMM_PTR_ERR(port->p);
		goto free_irq;
	}

	/* Save port pointer */
	dev->priv = port;

	/* Unmask Rx interrupt */
	port->ier |= (UART_IER_RDI | UART_IER_RLSI);
	omap_serial_out(port, UART_IER, port->ier);

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

static int omap_uart_driver_remove(struct vmm_device *dev)
{
	struct omap_uart_port *port = dev->priv;

	if (!port) {
		return VMM_OK;
	}

	/* Mask Rx interrupt */
	port->ier &= ~(UART_IER_RDI | UART_IER_RLSI);
	omap_serial_out(port, UART_IER, port->ier);

	/* Free-up resources */
	serial_destroy(port->p);
	vmm_host_irq_unregister(port->irq, port);
	vmm_devtree_regunmap_release(dev->of_node, port->base, 0);
	vmm_free(port);
	dev->priv = NULL;

	return VMM_OK;
}

static struct vmm_devtree_nodeid omap_uart_devid_table[] = {
	{ .compatible = "st16654" },
	{ /* end of list */ },
};

static struct vmm_driver omap_uart_driver = {
	.name = "omap_uart_serial",
	.match_table = omap_uart_devid_table,
	.probe = omap_uart_driver_probe,
	.remove = omap_uart_driver_remove,
};

static int __init omap_uart_driver_init(void)
{
	return vmm_devdrv_register_driver(&omap_uart_driver);
}

static void __exit omap_uart_driver_exit(void)
{
	vmm_devdrv_unregister_driver(&omap_uart_driver);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
