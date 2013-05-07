/**
 * Copyright (c) 2013 Jean-Christophe Dubois.
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
 * @file imx-uart.c
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @brief source file for imx serial port driver.
 *
 * based on linux/drivers/tty/serial/imx.c
 *
 *  Driver for Motorola IMX serial ports
 *
 *  Based on drivers/char/serial.c, by Linus Torvalds, Theodore Ts'o.
 *
 *  Author: Sascha Hauer <sascha@saschahauer.de>
 *  Copyright (C) 2004 Pengutronix
 *
 *  Copyright (C) 2009 emlix GmbH
 *  Author: Fabian Godehardt (added IrDA support for iMX)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * [29-Mar-2005] Mike Lee
 * Added hardware handshake
 */

#include <vmm_error.h>
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
#include <drv/imx-uart.h>

#define MODULE_DESC                     "IMX Serial Driver"
#define MODULE_AUTHOR                   "Jean-Christophe Dubois"
#define MODULE_LICENSE                  "GPL"
#define MODULE_IPRIORITY                0
#define MODULE_INIT                     imx_driver_init
#define MODULE_EXIT                     imx_driver_exit

//#define UART_IMX_USE_TXINTR

/* Register definitions */
#define URXD0 0x0		/* Receiver Register */
#define URTX0 0x40		/* Transmitter Register */
#define UCR1  0x80		/* Control Register 1 */
#define UCR2  0x84		/* Control Register 2 */
#define UCR3  0x88		/* Control Register 3 */
#define UCR4  0x8c		/* Control Register 4 */
#define UFCR  0x90		/* FIFO Control Register */
#define USR1  0x94		/* Status Register 1 */
#define USR2  0x98		/* Status Register 2 */
#define UESC  0x9c		/* Escape Character Register */
#define UTIM  0xa0		/* Escape Timer Register */
#define UBIR  0xa4		/* BRM Incremental Register */
#define UBMR  0xa8		/* BRM Modulator Register */
#define UBRC  0xac		/* Baud Rate Count Register */
#define IMX21_ONEMS 0xb0	/* One Millisecond register */
#define IMX1_UTS 0xd0		/* UART Test Register on i.mx1 */
#define IMX21_UTS 0xb4		/* UART Test Register on all other i.mx */

/* UART Control Register Bit Fields.*/
#define URXD_CHARRDY	(1<<15)
#define URXD_ERR	(1<<14)
#define URXD_OVRRUN	(1<<13)
#define URXD_FRMERR	(1<<12)
#define URXD_BRK	(1<<11)
#define URXD_PRERR	(1<<10)
#define UCR1_ADEN	(1<<15)	/* Auto detect interrupt */
#define UCR1_ADBR	(1<<14)	/* Auto detect baud rate */
#define UCR1_TRDYEN	(1<<13)	/* Transmitter ready interrupt enable */
#define UCR1_IDEN	(1<<12)	/* Idle condition interrupt */
#define UCR1_RRDYEN	(1<<9)	/* Recv ready interrupt enable */
#define UCR1_RDMAEN	(1<<8)	/* Recv ready DMA enable */
#define UCR1_IREN	(1<<7)	/* Infrared interface enable */
#define UCR1_TXMPTYEN	(1<<6)	/* Transimitter empty interrupt enable */
#define UCR1_RTSDEN	(1<<5)	/* RTS delta interrupt enable */
#define UCR1_SNDBRK	(1<<4)	/* Send break */
#define UCR1_TDMAEN	(1<<3)	/* Transmitter ready DMA enable */
#define IMX1_UCR1_UARTCLKEN (1<<2)	/* UART clock enabled, i.mx1 only */
#define UCR1_DOZE	(1<<1)	/* Doze */
#define UCR1_UARTEN	(1<<0)	/* UART enabled */
#define UCR2_ESCI	(1<<15)	/* Escape seq interrupt enable */
#define UCR2_IRTS	(1<<14)	/* Ignore RTS pin */
#define UCR2_CTSC	(1<<13)	/* CTS pin control */
#define UCR2_CTS	(1<<12)	/* Clear to send */
#define UCR2_ESCEN	(1<<11)	/* Escape enable */
#define UCR2_PREN	(1<<8)	/* Parity enable */
#define UCR2_PROE	(1<<7)	/* Parity odd/even */
#define UCR2_STPB	(1<<6)	/* Stop */
#define UCR2_WS		(1<<5)	/* Word size */
#define UCR2_RTSEN	(1<<4)	/* Request to send interrupt enable */
#define UCR2_ATEN	(1<<3)	/* Aging Timer Enable */
#define UCR2_TXEN	(1<<2)	/* Transmitter enabled */
#define UCR2_RXEN	(1<<1)	/* Receiver enabled */
#define UCR2_SRST	(1<<0)	/* SW reset */
#define UCR3_DTREN	(1<<13)	/* DTR interrupt enable */
#define UCR3_PARERREN	(1<<12)	/* Parity enable */
#define UCR3_FRAERREN	(1<<11)	/* Frame error interrupt enable */
#define UCR3_DSR	(1<<10)	/* Data set ready */
#define UCR3_DCD	(1<<9)	/* Data carrier detect */
#define UCR3_RI		(1<<8)	/* Ring indicator */
#define UCR3_TIMEOUTEN	(1<<7)	/* Timeout interrupt enable */
#define UCR3_RXDSEN	(1<<6)	/* Receive status interrupt enable */
#define UCR3_AIRINTEN	(1<<5)	/* Async IR wake interrupt enable */
#define UCR3_AWAKEN	(1<<4)	/* Async wake interrupt enable */
#define IMX21_UCR3_RXDMUXSEL	(1<<2)	/* RXD Muxed Input Select */
#define UCR3_INVT	(1<<1)	/* Inverted Infrared transmission */
#define UCR3_BPEN	(1<<0)	/* Preset registers enable */
#define UCR4_CTSTL_SHF	10	/* CTS trigger level shift */
#define UCR4_CTSTL_MASK	0x3F	/* CTS trigger is 6 bits wide */
#define UCR4_INVR	(1<<9)	/* Inverted infrared reception */
#define UCR4_ENIRI	(1<<8)	/* Serial infrared interrupt enable */
#define UCR4_WKEN	(1<<7)	/* Wake interrupt enable */
#define UCR4_REF16	(1<<6)	/* Ref freq 16 MHz */
#define UCR4_IRSC	(1<<5)	/* IR special case */
#define UCR4_TCEN	(1<<3)	/* Transmit complete interrupt enable */
#define UCR4_BKEN	(1<<2)	/* Break condition interrupt enable */
#define UCR4_OREN	(1<<1)	/* Receiver overrun interrupt enable */
#define UCR4_DREN	(1<<0)	/* Recv data ready interrupt enable */
#define UFCR_RXTL_SHF	0	/* Receiver trigger level shift */
#define UFCR_DCEDTE	(1<<6)	/* DCE/DTE mode select */
#define UFCR_RFDIV	(7<<7)	/* Reference freq divider mask */
#define UFCR_RFDIV_REG(x)	(((x) < 7 ? 6 - (x) : 6) << 7)
#define UFCR_TXTL_SHF	10	/* Transmitter trigger level shift */
#define USR1_PARITYERR	(1<<15)	/* Parity error interrupt flag */
#define USR1_RTSS	(1<<14)	/* RTS pin status */
#define USR1_TRDY	(1<<13)	/* Transmitter ready interrupt/dma flag */
#define USR1_RTSD	(1<<12)	/* RTS delta */
#define USR1_ESCF	(1<<11)	/* Escape seq interrupt flag */
#define USR1_FRAMERR	(1<<10)	/* Frame error interrupt flag */
#define USR1_RRDY	(1<<9)	/* Receiver ready interrupt/dma flag */
#define USR1_TIMEOUT	(1<<7)	/* Receive timeout interrupt status */
#define USR1_RXDS	 (1<<6)	/* Receiver idle interrupt flag */
#define USR1_AIRINT	 (1<<5)	/* Async IR wake interrupt flag */
#define USR1_AWAKE	 (1<<4)	/* Aysnc wake interrupt flag */
#define USR2_ADET	 (1<<15)	/* Auto baud rate detect complete */
#define USR2_TXFE	 (1<<14)	/* Transmit buffer FIFO empty */
#define USR2_DTRF	 (1<<13)	/* DTR edge interrupt flag */
#define USR2_IDLE	 (1<<12)	/* Idle condition */
#define USR2_IRINT	 (1<<8)	/* Serial infrared interrupt flag */
#define USR2_WAKE	 (1<<7)	/* Wake */
#define USR2_RTSF	 (1<<4)	/* RTS edge interrupt flag */
#define USR2_TXDC	 (1<<3)	/* Transmitter complete */
#define USR2_BRCD	 (1<<2)	/* Break condition */
#define USR2_ORE	(1<<1)	/* Overrun error */
#define USR2_RDR	(1<<0)	/* Recv data ready */
#define UTS_FRCPERR	(1<<13)	/* Force parity error */
#define UTS_LOOP	(1<<12)	/* Loop tx and rx */
#define UTS_TXEMPTY	 (1<<6)	/* TxFIFO empty */
#define UTS_RXEMPTY	 (1<<5)	/* RxFIFO empty */
#define UTS_TXFULL	 (1<<4)	/* TxFIFO full */
#define UTS_RXFULL	 (1<<3)	/* RxFIFO full */
#define UTS_SOFTRST	 (1<<0)	/* Software reset */

/*
 * This determines how often we check the modem status signals
 * for any change.  They generally aren't connected to an IRQ
 * so we have to poll them.  We also check immediately before
 * filling the TX fifo incase CTS has been dropped.
 */
#define MCTRL_TIMEOUT	(250*HZ/1000)

struct imx_port {
	struct vmm_chardev cd;
	struct vmm_completion read_possible;
#if defined(UART_SAMSUNG_USE_TXINTR)
	struct vmm_completion write_possible;
#endif
	virtual_addr_t base;
	u32 baudrate;
	u32 input_clock;
	u32 irq;
	u16 mask;
};

bool imx_lowlevel_can_getc(virtual_addr_t base)
{
	u32 status = vmm_readl((void *)(base + USR2));

	if (~status & USR2_RDR) {
		return FALSE;
	} else {
		return TRUE;
	}
}

u8 imx_lowlevel_getc(virtual_addr_t base)
{
	u8 data;

	/* Wait until there is data in the FIFO */
	while (!imx_lowlevel_can_getc(base)) ;

	data = vmm_readl((void *)(base + URXD0));

	return data;
}

bool imx_lowlevel_can_putc(virtual_addr_t base)
{
	u32 status = vmm_readl((void *)(base + USR1));

	if (~status & USR1_TRDY) {
		return FALSE;
	} else {
		return TRUE;
	}
}

void imx_lowlevel_putc(virtual_addr_t base, u8 ch)
{
	/* Wait until there is space in the FIFO */
	while (!imx_lowlevel_can_putc(base)) ;

	/* Send the character */
	vmm_writel(ch, (void *)(base + URTX0));
}

void imx_lowlevel_init(virtual_addr_t base, u32 baudrate, u32 input_clock)
{
	unsigned int temp = vmm_readl((void *)(base + UCR1));
	unsigned int divider;
	unsigned int remainder;

	/* First, disable everything */
	temp &= ~UCR1_UARTEN;
	vmm_writel(temp, (void *)base + UCR1);

#if 0
	/*
	 * Set baud rate
	 *
	 * UBRDIV  = (UART_CLK / (16 * BAUD_RATE)) - 1
	 * DIVSLOT = MOD(UART_CLK / BAUD_RATE, 16)
	 */
	temp = udiv32(input_clock, baudrate);
	divider = udiv32(temp, 16) - 1;
	remainder = umod32(temp, 16);

	vmm_out_le16((void *)(base + S3C2410_UBRDIV), (u16)
		     divider);
	vmm_out_8((void *)(base + S3C2443_DIVSLOT), (u8)
		  remainder);

	/* Set the UART to be 8 bits, 1 stop bit, no parity */
	vmm_out_le32((void *)(base + S3C2410_ULCON),
		     S3C2410_LCON_CS8 | S3C2410_LCON_PNONE);

	/* enable FIFO, set RX and TX trigger */
	vmm_out_le32((void *)(base + S3C2410_UFCON), S3C2410_UFCON_DEFAULT);
#else
	/* enable the UART */
	temp |= UCR1_RRDYEN | UCR1_RTSDEN | UCR1_UARTEN;

	vmm_writel(temp, (void *)(base + UCR1));
#endif
}

#if defined(UART_IMX_USE_TXINTR)
static void imx_txint(struct imx_port *port)
{
	port->mask &= ~UCR1_TRDYEN;
	vmm_writel(port->mask, (void *)port->base + UCR1);
	vmm_completion_complete(&port->write_possible);
}
#endif

static void imx_rxint(struct imx_port *port)
{
	port->mask &= ~UCR1_RRDYEN;
	vmm_writel(port->mask, (void *)port->base + UCR1);
	vmm_completion_complete(&port->read_possible);
}

static void imx_rtsint(struct imx_port *port)
{
}

static vmm_irq_return_t imx_irq_handler(u32 irq, void *dev_id)
{
	struct imx_port *port = dev_id;
	unsigned int sts;

	sts = vmm_readl((void *)port->base + USR1);

	if (sts & USR1_RRDY) {
		imx_rxint(port);
	}
#if defined(UART_IMX_USE_TXINTR)
	if ((sts & USR1_TRDY) && (port->mask & UCR1_TXMPTYEN)) {
		imx_txint(port);
	}
#endif

	if (sts & USR1_RTSD) {
		imx_rtsint(port);
	}

	if (sts & USR1_AWAKE) {
		vmm_writel(USR1_AWAKE, (void *)port->base + USR1);
	}

	return VMM_IRQ_HANDLED;
}

static u8 imx_getc_sleepable(struct imx_port *port)
{
	/* Wait until there is data in the FIFO */
	while (!imx_lowlevel_can_getc(port->base)) {
		/* Enable the RX interrupt */
		port->mask |= UCR1_RRDYEN;
		vmm_writel(port->mask, (void *)port->base + UCR1);
		/* Wait for completion */
		vmm_completion_wait(&port->read_possible);
	}

	/* Read data to destination */
	return imx_lowlevel_getc(port->base);
}

static u32 imx_read(struct vmm_chardev *cdev, u8 * dest, u32 len, bool sleep)
{
	u32 i;
	struct imx_port *port;

	if (!(cdev && dest && cdev->priv)) {
		return 0;
	}

	port = cdev->priv;

	if (sleep) {
		for (i = 0; i < len; i++) {
			dest[i] = imx_getc_sleepable(port);
		}
	} else {
		for (i = 0; i < len; i++) {
			if (!imx_lowlevel_can_getc(port->base)) {
				break;
			}
			dest[i] = imx_lowlevel_getc(port->base);
		}
	}

	return i;
}

#if defined(UART_IMX_USE_TXINTR)
static void imx_putc_sleepable(struct imx_port *port, u8 ch)
{
	/* Wait until there is space in the FIFO */
	if (!imx_lowlevel_can_putc(port->base)) {
		/* Enable the RX interrupt */
		port->mask |= UCR1_TRDYEN;
		vmm_writel(port->mask, (void *)port->base + UCR1);
		/* Wait for completion */
		vmm_completion_wait(&port->write_possible);
	}

	/* Write data to FIFO */
	imx_lowlevel_putc(port->base, ch);
}
#endif

static u32 imx_write(struct vmm_chardev *cdev, u8 * src, u32 len, bool sleep)
{
	u32 i;
	struct imx_port *port;

	if (!(cdev && src && cdev->priv)) {
		return 0;
	}

	port = cdev->priv;
#if defined(UART_IMX_USE_TXINTR)
	if (sleep) {
		for (i = 0; i < len; i++) {
			imx_putc_sleepable(port, src[i]);
		}
	} else {
		for (i = 0; i < len; i++) {
			if (!imx_lowlevel_can_putc(port->base)) {
				break;
			}
			imx_lowlevel_putc(port->base, src[i]);
		}
	}
#else
	for (i = 0; i < len; i++) {
		if (!imx_lowlevel_can_putc(port->base)) {
			break;
		}
		imx_lowlevel_putc(port->base, src[i]);
	}
#endif

	return i;
}

static int imx_driver_probe(struct vmm_device *dev,
			    const struct vmm_devtree_nodeid *devid)
{
	int rc = VMM_EFAIL;
	const char *attr = NULL;
	struct imx_port *port = NULL;

	port = vmm_zalloc(sizeof(struct imx_port));
	if (!port) {
		rc = VMM_EFAIL;
		goto free_nothing;
	}

	strcpy(port->cd.name, dev->node->name);
	port->cd.dev = dev;
	port->cd.ioctl = NULL;
	port->cd.read = imx_read;
	port->cd.write = imx_write;
	port->cd.priv = port;

	INIT_COMPLETION(&port->read_possible);
#if defined(UART_IMX_USE_TXINTR)
	INIT_COMPLETION(&port->write_possible);
#endif

	rc = vmm_devtree_regmap(dev->node, &port->base, 0);
	if (rc) {
		goto free_port;
	}

	port->mask = UCR1_RRDYEN | UCR1_RTSDEN;

#if defined(UART_IMX_USE_TXINTR)
	port->mask |= UCR1_TRDYEN;
#endif

	vmm_writel(port->mask, (void *)port->base + UCR1);

	attr = vmm_devtree_attrval(dev->node, "baudrate");
	if (!attr) {
		rc = VMM_EFAIL;
		goto free_reg;
	}
	port->baudrate = *((u32 *) attr);

	rc = vmm_devtree_clock_frequency(dev->node, &port->input_clock);
	if (!attr) {
		rc = VMM_EFAIL;
		goto free_reg;
	}

	rc = vmm_devtree_irq_get(dev->node, &port->irq, 0);
	if (rc) {
		rc = VMM_EFAIL;
		goto free_reg;
	}

	if ((rc =
	     vmm_host_irq_register(port->irq, dev->node->name,
				   imx_irq_handler, port))) {
		goto free_reg;
	}

	/* Call low-level init function */
	imx_lowlevel_init(port->base, port->baudrate, port->input_clock);

	port->mask = vmm_readl((void *)port->base + UCR1);

	rc = vmm_chardev_register(&port->cd);
	if (rc) {
		goto free_irq;
	}

	dev->priv = port;

	return rc;

 free_irq:
	vmm_host_irq_unregister(port->irq, port);
 free_reg:
	vmm_devtree_regunmap(dev->node, port->base, 0);
 free_port:
	vmm_free(port);
 free_nothing:
	return rc;
}

static int imx_driver_remove(struct vmm_device *dev)
{
	int rc = VMM_OK;
	struct imx_port *port = dev->priv;

	if (port) {
		rc = vmm_chardev_unregister(&port->cd);
		vmm_devtree_regunmap(dev->node, port->base, 0);
		vmm_free(port);
		dev->priv = NULL;
	}

	return rc;
}

static struct vmm_devtree_nodeid imx_devid_table[] = {
	{
	 .type = "serial",
	 .compatible = "freescale"},
	{
	 .type = "serial",
	 .compatible = "imx-uart"},
	{
	 .type = "serial",
	 .compatible = "freescale,imx-uart"},
	{
	 /* end of list */
	 },
};

static struct vmm_driver imx_driver = {
	.name = "imx_serial",
	.match_table = imx_devid_table,
	.probe = imx_driver_probe,
	.remove = imx_driver_remove,
};

static int __init imx_driver_init(void)
{
	return vmm_devdrv_register_driver(&imx_driver);
}

static void __exit imx_driver_exit(void)
{
	vmm_devdrv_unregister_driver(&imx_driver);
}

VMM_DECLARE_MODULE(MODULE_DESC,
		   MODULE_AUTHOR,
		   MODULE_LICENSE,
		   MODULE_IPRIORITY,
		   MODULE_INIT,
		   MODULE_EXIT);
