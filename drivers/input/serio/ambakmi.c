/**
 * Copyright (c) 2012 Anup Patel.
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
 * @file ambakmi.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief AMBA Keyboard/Mouse Interface Controller
 *
 * The source has been largely adapted from Linux 3.x or higher:
 * drivers/input/serio/ambakmi.c
 *
 *  Copyright (C) 2000-2003 Deep Blue Solutions Ltd.
 *  Copyright (C) 2002 Russell King.
 *
 * The original code is licensed under the GPL.
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/serio.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/amba/bus.h>
#include <linux/amba/kmi.h>
#include <linux/clk.h>

#include <asm/io.h>
#include <asm/irq.h>

#define MODULE_DESC			"AMBA KMI Driver"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(SERIO_IPRIORITY+1)
#define	MODULE_INIT			amba_kmi_driver_init
#define	MODULE_EXIT			amba_kmi_driver_exit

#define KMI_BASE	(kmi->base)

struct amba_kmi_port {
	struct serio 		*io;
	struct clk		*clk;
	void			*base;
	unsigned int		irq;
	unsigned int		divisor;
	unsigned int		open;
};

static irqreturn_t amba_kmi_int(u32 irq_no, void *dev)
{
	struct amba_kmi_port *kmi = dev;
	unsigned int status = readb(KMIIR);
	int handled = IRQ_NONE;

	while (status & KMIIR_RXINTR) {
		serio_interrupt(kmi->io, readb(KMIDATA), 0);
		status = readb(KMIIR);
		handled = IRQ_HANDLED;
	}

	return handled;
}

static int amba_kmi_write(struct serio *io, unsigned char val)
{
	struct amba_kmi_port *kmi = io->port_data;
	unsigned int timeleft = 10000; /* timeout in 100ms */

	while ((readb(KMISTAT) & KMISTAT_TXEMPTY) == 0 && --timeleft)
		udelay(10);

	if (timeleft)
		writeb(val, KMIDATA);

	return timeleft ? 0 : SERIO_TIMEOUT;
}

static int amba_kmi_open(struct serio *io)
{
	struct amba_kmi_port *kmi = io->port_data;
	unsigned int divisor;
	int ret;

	ret = clk_enable(kmi->clk);
	if (ret)
		goto out;

	divisor = clk_get_rate(kmi->clk) / 8000000 - 1;
	writeb(divisor, KMICLKDIV);
	writeb(KMICR_EN, KMICR);

	ret = request_irq(kmi->irq, amba_kmi_int, 0, io->name, kmi);
	if (ret) {
		printk(KERN_ERR "kmi: failed to claim IRQ%d\n", kmi->irq);
		writeb(0, KMICR);
		goto clk_disable;
	}

	writeb(KMICR_EN | KMICR_RXINTREN, KMICR);

	return 0;

 clk_disable:
	clk_disable(kmi->clk);
 out:
	return ret;
}

static void amba_kmi_close(struct serio *io)
{
	struct amba_kmi_port *kmi = io->port_data;

	writeb(0, KMICR);

	free_irq(kmi->irq, kmi);
	clk_disable(kmi->clk);
}

static int amba_kmi_driver_probe(struct vmm_device *dev,
			      const struct vmm_devtree_nodeid *devid)
{
	const char *attr;
	struct amba_kmi_port *kmi;
	struct serio *io;
	int ret;

	kmi = kzalloc(sizeof(struct amba_kmi_port), GFP_KERNEL);
	io = kzalloc(sizeof(struct serio), GFP_KERNEL);
	if (!kmi || !io) {
		ret = -ENOMEM;
		goto out;
	}

	io->id.type	= SERIO_8042;
	io->write	= amba_kmi_write;
	io->open	= amba_kmi_open;
	io->close	= amba_kmi_close;
	strncpy(io->name, dev->node->name, sizeof(io->name));
	strncpy(io->phys, dev->node->name, sizeof(io->phys));
	io->port_data	= kmi;
	io->dev		= dev;

	kmi->io		= io;
	ret = vmm_devtree_regmap(dev->node, (virtual_addr_t *)&kmi->base, 0);
	if (ret) {
		ret = -ENOMEM;
		goto out;
	}

	kmi->clk = clk_get(dev, "KMIREFCLK");
	if (IS_ERR(kmi->clk)) {
		ret = -EFAIL;
		goto unmap;
	}

	attr = vmm_devtree_attrval(dev->node, "irq");
	if (!attr) {
		ret = -EFAIL;
		goto unmap;
	}
	kmi->irq = *((u32 *) attr);

	dev->priv = kmi;

	serio_register_port(kmi->io);

	return VMM_OK;

 unmap:
	vmm_devtree_regunmap(dev->node, (virtual_addr_t)kmi->base, 0);
 out:
	kfree(kmi);
	kfree(io);
	return ret;
}

static int amba_kmi_driver_remove(struct vmm_device *dev)
{
	struct amba_kmi_port *kmi = (struct amba_kmi_port *)dev->priv;

	dev->priv = NULL;

	serio_unregister_port(kmi->io);
	vmm_devtree_regunmap(dev->node, (virtual_addr_t)kmi->base, 0);
	kfree(kmi);

	return VMM_OK;
}

static struct vmm_devtree_nodeid amba_kmi_devid_table[] = {
	{.type = "serio",.compatible = "pl050"},
	{.type = "serio",.compatible = "ambakmi"},
	{ /* end of list */ },
};

static struct vmm_driver amba_kmi_driver = {
	.name = "kmi-pl050",
	.match_table = amba_kmi_devid_table,
	.probe = amba_kmi_driver_probe,
	.remove = amba_kmi_driver_remove,
};

static int __init amba_kmi_driver_init(void)
{
	return vmm_devdrv_register_driver(&amba_kmi_driver);
}

static void __exit amba_kmi_driver_exit(void)
{
	vmm_devdrv_unregister_driver(&amba_kmi_driver);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
