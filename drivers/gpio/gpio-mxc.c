/*
 * Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWide.
 * All rights reserved.
 *
 * MXC GPIO support. (c) 2008 Daniel Mack <daniel@caiaq.de>
 * Copyright 2008 Juergen Beisert, kernel@pengutronix.de
 *
 * Based on code from Freescale,
 * Copyright (C) 2004-2010 Freescale Semiconductor, Inc. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * @file gpio-mxc.c
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @brief MXC GPIO support
 */

#include <linux/err.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/irqchip/chained_irq.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/basic_mmio_gpio.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/module.h>
#include <asm-generic/bug.h>

enum mxc_gpio_hwtype {
	IMX1_GPIO,	/* runs on i.mx1 */
	IMX21_GPIO,	/* runs on i.mx21 and i.mx27 */
	IMX31_GPIO,	/* runs on i.mx31 */
	IMX35_GPIO,	/* runs on all other i.mx */
};

/* device type dependent stuff */
struct mxc_gpio_hwdata {
	unsigned dr_reg;
	unsigned gdir_reg;
	unsigned psr_reg;
	unsigned icr1_reg;
	unsigned icr2_reg;
	unsigned imr_reg;
	unsigned isr_reg;
	int edge_sel_reg;
	unsigned low_level;
	unsigned high_level;
	unsigned rise_edge;
	unsigned fall_edge;
};

struct mxc_gpio_port {
	struct list_head node;
	void __iomem* base;
	u32 irq;
	u32 irq_high;
	struct vmm_host_extirq_group *extirq_group;
	struct bgpio_chip bgc;
	u32 both_edges;
};

static struct mxc_gpio_hwdata imx1_imx21_gpio_hwdata = {
	.dr_reg		= 0x1c,
	.gdir_reg	= 0x00,
	.psr_reg	= 0x24,
	.icr1_reg	= 0x28,
	.icr2_reg	= 0x2c,
	.imr_reg	= 0x30,
	.isr_reg	= 0x34,
	.edge_sel_reg	= -EINVAL,
	.low_level	= 0x03,
	.high_level	= 0x02,
	.rise_edge	= 0x00,
	.fall_edge	= 0x01,
};

static struct mxc_gpio_hwdata imx31_gpio_hwdata = {
	.dr_reg		= 0x00,
	.gdir_reg	= 0x04,
	.psr_reg	= 0x08,
	.icr1_reg	= 0x0c,
	.icr2_reg	= 0x10,
	.imr_reg	= 0x14,
	.isr_reg	= 0x18,
	.edge_sel_reg	= -EINVAL,
	.low_level	= 0x00,
	.high_level	= 0x01,
	.rise_edge	= 0x02,
	.fall_edge	= 0x03,
};

static struct mxc_gpio_hwdata imx35_gpio_hwdata = {
	.dr_reg		= 0x00,
	.gdir_reg	= 0x04,
	.psr_reg	= 0x08,
	.icr1_reg	= 0x0c,
	.icr2_reg	= 0x10,
	.imr_reg	= 0x14,
	.isr_reg	= 0x18,
	.edge_sel_reg	= 0x1c,
	.low_level	= 0x00,
	.high_level	= 0x01,
	.rise_edge	= 0x02,
	.fall_edge	= 0x03,
};

static enum mxc_gpio_hwtype mxc_gpio_hwtype;
static struct mxc_gpio_hwdata *mxc_gpio_hwdata;

#define GPIO_DR			(mxc_gpio_hwdata->dr_reg)
#define GPIO_GDIR		(mxc_gpio_hwdata->gdir_reg)
#define GPIO_PSR		(mxc_gpio_hwdata->psr_reg)
#define GPIO_ICR1		(mxc_gpio_hwdata->icr1_reg)
#define GPIO_ICR2		(mxc_gpio_hwdata->icr2_reg)
#define GPIO_IMR		(mxc_gpio_hwdata->imr_reg)
#define GPIO_ISR		(mxc_gpio_hwdata->isr_reg)
#define GPIO_EDGE_SEL		(mxc_gpio_hwdata->edge_sel_reg)

#define GPIO_INT_LOW_LEV	(mxc_gpio_hwdata->low_level)
#define GPIO_INT_HIGH_LEV	(mxc_gpio_hwdata->high_level)
#define GPIO_INT_RISE_EDGE	(mxc_gpio_hwdata->rise_edge)
#define GPIO_INT_FALL_EDGE	(mxc_gpio_hwdata->fall_edge)
#define GPIO_INT_BOTH_EDGES	0x4

static struct platform_device_id mxc_gpio_devtype[] = {
	{
		.name = "imx1-gpio",
		.driver_data = IMX1_GPIO,
	}, {
		.name = "imx21-gpio",
		.driver_data = IMX21_GPIO,
	}, {
		.name = "imx31-gpio",
		.driver_data = IMX31_GPIO,
	}, {
		.name = "imx35-gpio",
		.driver_data = IMX35_GPIO,
	}, {
		/* sentinel */
	}
};

static const struct vmm_devtree_nodeid mxc_gpio_dt_ids[] = {
	{ .compatible = "fsl,imx1-gpio", .data = &mxc_gpio_devtype[IMX1_GPIO], },
	{ .compatible = "fsl,imx21-gpio", .data = &mxc_gpio_devtype[IMX21_GPIO], },
	{ .compatible = "fsl,imx31-gpio", .data = &mxc_gpio_devtype[IMX31_GPIO], },
	{ .compatible = "fsl,imx35-gpio", .data = &mxc_gpio_devtype[IMX35_GPIO], },
	{ /* sentinel */ }
};

/*
 * MX2 has one interrupt *for all* gpio ports. The list is used
 * to save the references to all ports, so that mx2_gpio_irq_handler
 * can walk through all interrupt status registers.
 */
static LIST_HEAD(mxc_gpio_ports);

/* Note: This driver assumes 32 GPIOs are handled in one register */

static int gpio_set_irq_type(struct vmm_host_irq *d, u32 type)
{
	struct mxc_gpio_port *port = vmm_host_irq_get_chip_data(d);
	u32 bit, val;
	u32 gpio_idx = vmm_host_extirq_to_hwirq(port->extirq_group, d->num);
	u32 gpio = port->bgc.gc.base + gpio_idx;
	int edge;
	void __iomem *reg = port->base;

	port->both_edges &= ~(1 << gpio_idx);
	switch (type) {
	case IRQ_TYPE_EDGE_RISING:
		edge = GPIO_INT_RISE_EDGE;
		break;
	case IRQ_TYPE_EDGE_FALLING:
		edge = GPIO_INT_FALL_EDGE;
		break;
	case IRQ_TYPE_EDGE_BOTH:
		if (GPIO_EDGE_SEL >= 0) {
			edge = GPIO_INT_BOTH_EDGES;
		} else {
			val = __gpio_get_value(gpio);
			if (val) {
				edge = GPIO_INT_LOW_LEV;
				pr_debug("mxc: set GPIO %d to low trigger\n", gpio);
			} else {
				edge = GPIO_INT_HIGH_LEV;
				pr_debug("mxc: set GPIO %d to high trigger\n", gpio);
			}
			port->both_edges |= 1 << gpio_idx;
		}
		break;
	case IRQ_TYPE_LEVEL_LOW:
		edge = GPIO_INT_LOW_LEV;
		break;
	case IRQ_TYPE_LEVEL_HIGH:
		edge = GPIO_INT_HIGH_LEV;
		break;
	default:
		return -EINVAL;
	}

	if (GPIO_EDGE_SEL >= 0) {
		val = readl(port->base + GPIO_EDGE_SEL);
		if (edge == GPIO_INT_BOTH_EDGES)
			writel(val | (1 << gpio_idx),
				port->base + GPIO_EDGE_SEL);
		else
			writel(val & ~(1 << gpio_idx),
				port->base + GPIO_EDGE_SEL);
	}

	if (edge != GPIO_INT_BOTH_EDGES) {
		reg += GPIO_ICR1 + ((gpio_idx & 0x10) >> 2); /* lower or upper register */
		bit = gpio_idx & 0xf;
		val = readl(reg) & ~(0x3 << (bit << 1));
		writel(val | (edge << (bit << 1)), reg);
	}

	writel(1 << gpio_idx, port->base + GPIO_ISR);

	return 0;
}

void __noinline mxc_flip_edge(struct mxc_gpio_port *port, u32 gpio)
{
	void __iomem *reg = port->base;
	u32 bit, val;
	int edge;

	reg += GPIO_ICR1 + ((gpio & 0x10) >> 2); /* lower or upper register */
	bit = gpio & 0xf;
	val = readl(reg);
	edge = (val >> (bit << 1)) & 3;
	val &= ~(0x3 << (bit << 1));
	if (edge == GPIO_INT_HIGH_LEV) {
		edge = GPIO_INT_LOW_LEV;
		pr_debug("mxc: switch GPIO %d to low trigger\n", gpio);
	} else if (edge == GPIO_INT_LOW_LEV) {
		edge = GPIO_INT_HIGH_LEV;
		pr_debug("mxc: switch GPIO %d to high trigger\n", gpio);
	} else {
		pr_err("mxc: invalid configuration for GPIO %d: %x\n",
		       gpio, edge);
		return;
	}
	writel(val | (edge << (bit << 1)), reg);
}

/* handle 32 interrupts in one status register */
static void mxc_gpio_irq_handler(struct mxc_gpio_port *port, u32 irq_stat)
{
	u32 irq_num = 0;
	u32 cpu = vmm_smp_processor_id();
	struct vmm_host_irq *irq;

	while (irq_stat != 0) {
		int irqoffset = fls(irq_stat) - 1;

		if (port->both_edges & (1 << irqoffset))
			mxc_flip_edge(port, irqoffset);

		irq_num = vmm_host_extirq_find_mapping(port->extirq_group,
						       irqoffset);
		irq = vmm_host_irq_get(irq_num);
		vmm_handle_level_irq(irq, cpu, port);
		irq_stat &= ~(1 << irqoffset);
	}
}

/* MX1 and MX3 has one interrupt *per* gpio port */
static vmm_irq_return_t mx3_gpio_irq_handler(int irq, void *data)
{
	u32 irq_stat;
	struct vmm_host_irq* desc = NULL;
	struct mxc_gpio_port *port = data;
	struct vmm_host_irq_chip *chip = NULL;

	desc = vmm_host_irq_get(irq);
	chip = vmm_host_irq_get_chip(desc);

	vmm_chained_irq_enter(chip, desc);

	irq_stat = readl(port->base + GPIO_ISR) & readl(port->base + GPIO_IMR);
	mxc_gpio_irq_handler(port, irq_stat);

	vmm_chained_irq_exit(chip, desc);
	return VMM_IRQ_HANDLED;
}

/* MX2 has one interrupt *for all* gpio ports */
static vmm_irq_return_t mx2_gpio_irq_handler(int irq, void *data)
{
	u32 irq_msk, irq_stat;
	struct vmm_host_irq* desc = NULL;
	struct mxc_gpio_port *port = NULL;
	struct vmm_host_irq_chip *chip = NULL;

	desc = vmm_host_irq_get(irq);
	chip = vmm_host_irq_get_chip(desc);
	port = vmm_host_irq_get_chip_data(desc);
	vmm_chained_irq_enter(chip, desc);

	/* walk through all interrupt status registers */
	list_for_each_entry(port, &mxc_gpio_ports, node) {
		irq_msk = readl(port->base + GPIO_IMR);
		if (!irq_msk)
			continue;

		irq_stat = readl(port->base + GPIO_ISR) & irq_msk;
		if (irq_stat)
			mxc_gpio_irq_handler(port, irq_stat);
	}
	vmm_chained_irq_exit(chip, desc);
	return VMM_IRQ_HANDLED;
}

/* FIXME: Temporary */
static void irq_gc_lock(struct vmm_host_irq_chip *gc)
{
	gc = gc;
}

/* FIXME: Temporary */
static void irq_gc_unlock(struct vmm_host_irq_chip *gc)
{
	gc = gc;
}

static void irq_gc_init_lock(struct vmm_host_irq_chip *gc)
{
	gc = gc;
}

/**
 * irq_gc_ack_set_bit - Ack pending interrupt via setting bit
 * @d: irq_data
 */
void irq_gc_ack_set_bit(struct vmm_host_irq *d)
{
	struct vmm_host_irq_chip *gc = vmm_host_irq_get_chip(d);
	struct mxc_gpio_port *port = vmm_host_irq_get_chip_data(d);
	int irqoffset = vmm_host_extirq_to_hwirq(port->extirq_group, d->num);

	irq_gc_lock(gc);
	writel(1 << irqoffset, port->base + GPIO_ISR);
	irq_gc_unlock(gc);
}

/**
 * irq_gc_mask_clr_bit - Mask chip via clearing bit in mask register
 * @d: irq_data
 *
 * Chip has a single mask register. Values of this register are cached
 * and protected by gc->lock
 */
void irq_gc_mask_clr_bit(struct vmm_host_irq *d)
{
	struct vmm_host_irq_chip *gc = vmm_host_irq_get_chip(d);
	struct mxc_gpio_port *port = vmm_host_irq_get_chip_data(d);
	int irqoffset = vmm_host_extirq_to_hwirq(port->extirq_group, d->num);
	u32 mask = 0;

	irq_gc_lock(gc);
	mask = readl(port->base + GPIO_IMR) & ~(1 << irqoffset);
	writel(mask, port->base + GPIO_IMR);
	irq_gc_unlock(gc);
}

/**
 * irq_gc_mask_set_bit - Mask chip via setting bit in mask register
 * @d: irq_data
 *
 * Chip has a single mask register. Values of this register are cached
 * and protected by gc->lock
 */
void irq_gc_mask_set_bit(struct irq_data *d)
{
	struct vmm_host_irq_chip *gc = vmm_host_irq_get_chip(d);
	struct mxc_gpio_port *port = vmm_host_irq_get_chip_data(d);
	int irqoffset = vmm_host_extirq_to_hwirq(port->extirq_group, d->num);
	u32 mask = 0;

	irq_gc_lock(gc);
	mask = readl(port->base + GPIO_IMR) | (1 << irqoffset);
	writel(mask, port->base + GPIO_IMR);
	irq_gc_unlock(gc);
}

static int __init mxc_gpio_init_gc(struct mxc_gpio_port *port,
				   const char *name, int sz,
				   struct vmm_device *dev)
{
	struct vmm_host_irq_chip *gc;
	int irq = 0;
	int i = 0;

	if (NULL == (gc = vmm_zalloc(sizeof (struct vmm_host_irq_chip))))
	{
		pr_err("mxc: Failed to allocate IRQ chip\n");
		return -ENOMEM;
	}
	irq_gc_init_lock(gc);

	gc->irq_ack = irq_gc_ack_set_bit;
	gc->irq_mask = irq_gc_mask_clr_bit;
	gc->irq_unmask = irq_gc_mask_set_bit;
	gc->irq_set_type = gpio_set_irq_type;

	port->extirq_group = vmm_host_extirq_add(dev->node, sz,
						 &extirq_simple_ops, port);
	if (!port->extirq_group)
		return VMM_ENOTAVAIL;

	for (i = 0; i < sz; ++i) {
		irq = vmm_host_extirq_create_mapping(port->extirq_group, i);
		if (irq < 0) {
			pr_err("mxc: Failed to map extended IRQs\n");
			vmm_free(gc);
			return -ENODEV;
		}
		vmm_host_irq_set_chip(irq, gc);
		vmm_host_irq_set_chip_data(irq, port);
	}

	return VMM_OK;
}

static void mxc_gpio_get_hw(const struct vmm_devtree_nodeid *dev)
{
#if 0
	const struct vmm_devtree_nodeid *nodeid = 
		of_match_device(mxc_gpio_dt_ids, &pdev->dev);
#endif
	const struct platform_device_id *pdev = dev->data;
	enum mxc_gpio_hwtype hwtype;

	hwtype = pdev->driver_data;

	if (mxc_gpio_hwtype) {
		/*
		 * The driver works with a reasonable presupposition,
		 * that is all gpio ports must be the same type when
		 * running on one soc.
		 */
		BUG_ON(mxc_gpio_hwtype != hwtype);
		return;
	}

	if (hwtype == IMX35_GPIO)
		mxc_gpio_hwdata = &imx35_gpio_hwdata;
	else if (hwtype == IMX31_GPIO)
		mxc_gpio_hwdata = &imx31_gpio_hwdata;
	else
		mxc_gpio_hwdata = &imx1_imx21_gpio_hwdata;

	mxc_gpio_hwtype = hwtype;
}

static int mxc_gpio_to_irq(struct gpio_chip *gc, unsigned offset)
{
	struct bgpio_chip *bgc = to_bgpio_chip(gc);
	struct mxc_gpio_port *port =
		container_of(bgc, struct mxc_gpio_port, bgc);

	return vmm_host_extirq_find_mapping(port->extirq_group, offset);
}

#define PORT_NAME_LEN	12

static int mxc_gpio_probe(struct vmm_device *dev,
			  const struct vmm_devtree_nodeid *devid)
{
	struct device_node *np = dev->node;
	struct mxc_gpio_port *port;
	physical_addr_t paddr = 0;
	int err = VMM_OK;
	int port_num = 0;
	char *name = NULL;
	char *irq_name = NULL;

	mxc_gpio_get_hw(devid);

	port = devm_kzalloc(dev, sizeof(*port), GFP_KERNEL);
	if (!port)
		return -ENOMEM;

	err = vmm_devtree_request_regmap(np, (virtual_addr_t *)&port->base, 0,
					 "MXC GPIO");
	if (VMM_OK != err) {
		dev_err(dev, "fail to map registers from the device tree\n");
		goto out_regmap;
	}

	err = vmm_devtree_irq_get(np, &port->irq_high, 1);
	err = vmm_devtree_irq_get(np, &port->irq, 0);
	if (VMM_OK != err)
		goto out_irq_get;

	/*
	 * FIXME: As alias does not exist in Xvisor, the node name "gpiox"
	 * cannot be retrieved, and thus, the gpio id.
	 */
	/*
	 * Get the id from the base address:
	 * GPIO 1 (idx 0): (0x0209C000 & 0x3C000) >> 14 = 7
	 * GPIO 2 (idx 1): (0x020A0000 & 0x3C000) >> 14 = 8
	 * GPIO 3 (idx 2): (0x020A4000 & 0x3C000) >> 14 = 9
	 * GPIO 4 (idx 3): (0x020A8000 & 0x3C000) >> 14 = 10
	 * GPIO 5 (idx 4): (0x020AC000 & 0x3C000) >> 14 = 11
	 * GPIO 6 (idx 5): (0x020B0000 & 0x3C000) >> 14 = 12
	 * GPIO 7 (idx 6): (0x020B4000 & 0x3C000) >> 14 = 13
	 */
	if (VMM_OK != (err = vmm_host_va2pa((virtual_addr_t)port->base,
					    &paddr))) {
		goto out_bgio;
	}
	port_num = ((paddr & 0x3C000) >> 14) - 6;
	name = vmm_malloc(PORT_NAME_LEN);
	snprintf(name, PORT_NAME_LEN, "gpio_mxc%d", port_num);

	/* disable the interrupt and clear the status */
	writel(0, port->base + GPIO_IMR);
	writel(~0, port->base + GPIO_ISR);

	if (mxc_gpio_hwtype == IMX21_GPIO) {
		irq_name = vmm_malloc(PORT_NAME_LEN);
		strncpy(irq_name, name, PORT_NAME_LEN);
		/*
		 * Setup one handler for all GPIO interrupts. Actually setting
		 * the handler is needed only once, but doing it for every port
		 * is more robust and easier.
		 */
		err = vmm_host_irq_register(port->irq, irq_name,
					    mx2_gpio_irq_handler, port);
		if (VMM_OK != err)
			goto out_irq_reg;
	} else {
		/* setup one handler for each entry */
		irq_name = vmm_malloc(PORT_NAME_LEN + 5);
		snprintf(irq_name, PORT_NAME_LEN + 5, "gpio_mxc%d 0-15",
			 port_num);
		err = vmm_host_irq_register(port->irq, irq_name,
					    mx3_gpio_irq_handler, port);
		if (VMM_OK != err)
			goto out_irq_reg;
		if (port->irq_high > 0) {
			/* setup handler for GPIO 16 to 31 */
			irq_name = vmm_malloc(PORT_NAME_LEN + 6);
			snprintf(irq_name, PORT_NAME_LEN + 6,
				 "gpio_mxc%d 16-31", port_num);
			err = vmm_host_irq_register(port->irq_high, irq_name,
						    mx3_gpio_irq_handler,
						    port);
			if (VMM_OK != err)
				goto out_irq_reg_high;
		}
	}

	err = bgpio_init(&port->bgc, dev, 4,
			 port->base + GPIO_PSR,
			 port->base + GPIO_DR, NULL,
			 port->base + GPIO_GDIR, NULL, 0);
	if (err)
		goto out_bgio;

	port->bgc.gc.to_irq = mxc_gpio_to_irq;
	port->bgc.gc.base = (port_num - 1) * 32;

	err = gpiochip_add(&port->bgc.gc);
	if (err)
		goto out_bgpio_remove;

	/* gpio-mxc can be a generic irq chip */
	err = mxc_gpio_init_gc(port, name, 32, dev);
	if (err)
		goto out_gpiochip_remove;

	list_add_tail(&port->node, &mxc_gpio_ports);
	dev_info(dev, "%s registered\n", name);
	vmm_free(name);

	return err;

#if 0
out_irqdesc_free:
	irq_free_descs(irq_base, 32);
#endif
out_gpiochip_remove:
	WARN_ON(gpiochip_remove(&port->bgc.gc) < 0);
out_bgpio_remove:
	bgpio_remove(&port->bgc);
out_bgio:
	if (port->irq_high > 0)
		vmm_host_irq_unregister(port->irq_high, dev);
out_irq_reg_high:
	vmm_host_irq_unregister(port->irq, dev);
out_irq_reg:
out_irq_get:
	vmm_devtree_regunmap_release(np, (virtual_addr_t)port->base, 0);
out_regmap:
	devm_kfree(dev, port);
	dev_info(dev, "%s failed with errno %d\n", __func__, err);
	return err;
}

static struct vmm_driver mxc_gpio_driver = {
	.name		= "gpio-mxc",
	.match_table	= mxc_gpio_dt_ids,
	.probe		= mxc_gpio_probe,
};

static int __init gpio_mxc_init(void)
{
	return vmm_devdrv_register_driver(&mxc_gpio_driver);
}
#if 0
postcore_initcall(gpio_mxc_init);
#endif

VMM_DECLARE_MODULE("i.MX GPIO driver",
		   "Jimmy Durand Wesolowski",
		   "GPL",
		   1,
		   gpio_mxc_init,
		   NULL);

#if 0
MODULE_AUTHOR("Freescale Semiconductor, "
	      "Daniel Mack <danielncaiaq.de>, "
	      "Juergen Beisert <kernel@pengutronix.de>");
MODULE_DESCRIPTION("Freescale MXC GPIO");
MODULE_LICENSE("GPL");
#endif
