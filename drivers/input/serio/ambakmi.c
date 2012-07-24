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
 */

#include <vmm_heap.h>
#include <vmm_string.h>
#include <vmm_modules.h>
#include <vmm_devtree.h>
#include <vmm_devdrv.h>

#include <linux/error.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/printk.h>
#include <linux/interrupt.h>
#include <linux/amba/kmi.h>
#include <linux/serio.h>

#define MODULE_VARID			amba_kmi_driver_module
#define MODULE_NAME			"AMBA KMI Driver"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_IPRIORITY		(SERIO_IPRIORITY+1)
#define	MODULE_INIT			amba_kmi_driver_init
#define	MODULE_EXIT			amba_kmi_driver_exit

#define KMI_BASE	(kmi->base)

struct amba_kmi_port {
	struct serio 		*io;
	struct vmm_device	*dev;
	void			*base;
	unsigned int		irq;
	unsigned int		divisor;
	unsigned int		open;
};

static irqreturn_t amba_kmi_int(u32 irq_no, arch_regs_t *regs, void *dev)
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

	ret = vmm_devdrv_clock_enable(kmi->dev);
	if (ret)
		goto out;

	divisor = vmm_devdrv_clock_rate(kmi->dev) / 8000000 - 1;
	writeb(divisor, KMICLKDIV);
	writeb(KMICR_EN, KMICR);

	ret = vmm_host_irq_register(kmi->irq, 
				    kmi->dev->node->name, amba_kmi_int, kmi);
	if (ret) {
		printk(KERN_ERR "kmi: failed to claim IRQ%d\n", kmi->irq);
		writeb(0, KMICR);
		goto clk_disable;
	}

	writeb(KMICR_EN | KMICR_RXINTREN, KMICR);

	return 0;

 clk_disable:
	vmm_devdrv_clock_disable(kmi->dev);
 out:
	return ret;
}

static void amba_kmi_close(struct serio *io)
{
	struct amba_kmi_port *kmi = io->port_data;

	writeb(0, KMICR);

	vmm_host_irq_unregister(kmi->irq, kmi);
	vmm_devdrv_clock_disable(kmi->dev);
}

static int amba_kmi_driver_probe(struct vmm_device *dev,
			      const struct vmm_devid *devid)
{
	const char *attr;
	virtual_addr_t base;
	struct amba_kmi_port *kmi;
	struct serio *io;
	int ret;

	kmi = vmm_malloc(sizeof(struct amba_kmi_port));
	io = vmm_malloc(sizeof(struct serio));
	if (!kmi || !io) {
		ret = -ENOMEM;
		goto out;
	}

	vmm_memset(kmi, 0, sizeof(struct amba_kmi_port));
	vmm_memset(io, 0, sizeof(struct serio));

	io->id.type	= SERIO_8042;
	io->write	= amba_kmi_write;
	io->open	= amba_kmi_open;
	io->close	= amba_kmi_close;
	vmm_strncpy(io->name, dev->node->name, sizeof(io->name));
	vmm_strncpy(io->phys, dev->node->name, sizeof(io->phys));
	io->port_data	= kmi;

	kmi->io		= io;
	ret = vmm_devdrv_regmap(dev, &base, 0);
	if (ret) {
		ret = -ENOMEM;
		goto out;
	}
	kmi->base = (void *)base;

	kmi->dev = dev;

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
	vmm_devdrv_regunmap(dev, (virtual_addr_t)kmi->base, 0);
 out:
	vmm_free(kmi);
	vmm_free(io);
	return ret;
}

static int amba_kmi_driver_remove(struct vmm_device *dev)
{
	struct amba_kmi_port *kmi = (struct amba_kmi_port *)dev->priv;

	if (kmi) {
		dev->priv = NULL;
		serio_unregister_port(kmi->io);
		vmm_devdrv_regunmap(dev, (virtual_addr_t)kmi->base, 0);
		vmm_free(kmi);
	}

	return VMM_OK;
}

static struct vmm_devid amba_kmi_devid_table[] = {
	{.type = "serio",.compatible = "pl050"},
	{.type = "serio",.compatible = "ambakmi"},
	{ /* end of list */ },
};

static struct vmm_driver amba_kmi_driver = {
	.name = "amba_kmi",
	.match_table = amba_kmi_devid_table,
	.probe = amba_kmi_driver_probe,
	.remove = amba_kmi_driver_remove,
};

static int __init amba_kmi_driver_init(void)
{
	return vmm_devdrv_register_driver(&amba_kmi_driver);
}

static void amba_kmi_driver_exit(void)
{
	vmm_devdrv_unregister_driver(&amba_kmi_driver);
}

VMM_DECLARE_MODULE(MODULE_VARID,
		   MODULE_NAME,
		   MODULE_AUTHOR,
		   MODULE_IPRIORITY,
		   MODULE_INIT,
		   MODULE_EXIT);
