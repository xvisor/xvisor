/*
 * Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWide.
 * All rights reserved.
 *
 * Adapted from Linux Kernel 3.13.6 arch/arm/mach-imx/gpc.c
 *
 * Copyright 2011-2013 Freescale Semiconductor, Inc.
 * Copyright 2011 Linaro Ltd.
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 *
 * @file gpc.c
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @brief Freescale i.MX GPC funcion helpers
 */

#include <linux/io.h>
#include <linux/irq.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/printk.h>
#include <imx-common.h>

#define GPC_IMR1		0x008
#define GPC_PGC_CPU_PDN		0x2a0

#define IMR_NUM			4

static void __iomem *gpc_base;
static u32 gpc_wake_irqs[IMR_NUM];
static u32 gpc_saved_imrs[IMR_NUM];

void imx_gpc_pre_suspend(void)
{
	void __iomem *reg_imr1 = gpc_base + GPC_IMR1;
	int i;

	/* Tell GPC to power off ARM core when suspend */
	writel_relaxed(0x1, gpc_base + GPC_PGC_CPU_PDN);

	for (i = 0; i < IMR_NUM; i++) {
		gpc_saved_imrs[i] = readl_relaxed(reg_imr1 + i * 4);
		writel_relaxed(~gpc_wake_irqs[i], reg_imr1 + i * 4);
	}
}

void imx_gpc_post_resume(void)
{
	void __iomem *reg_imr1 = gpc_base + GPC_IMR1;
	int i;

	/* Keep ARM core powered on for other low-power modes */
	writel_relaxed(0x0, gpc_base + GPC_PGC_CPU_PDN);

	for (i = 0; i < IMR_NUM; i++)
		writel_relaxed(gpc_saved_imrs[i], reg_imr1 + i * 4);
}

/* static int imx_gpc_irq_set_wake(struct irq_data *d, unsigned int on) */
/* { */
/* 	unsigned int idx = d->num / 32 - 1; */
/* 	u32 mask; */

/* 	/\* Sanity check for SPI irq *\/ */
/* 	if (d->num < 32) */
/* 		return -EINVAL; */

/* 	mask = 1 << d->num % 32; */
/* 	gpc_wake_irqs[idx] = on ? gpc_wake_irqs[idx] | mask : */
/* 				  gpc_wake_irqs[idx] & ~mask; */

/* 	return 0; */
/* } */

void imx_gpc_mask_all(void)
{
	void __iomem *reg_imr1 = gpc_base + GPC_IMR1;
	int i;

	for (i = 0; i < IMR_NUM; i++) {
		gpc_saved_imrs[i] = readl_relaxed(reg_imr1 + i * 4);
		writel_relaxed(~0, reg_imr1 + i * 4);
	}

}

void imx_gpc_restore_all(void)
{
	void __iomem *reg_imr1 = gpc_base + GPC_IMR1;
	int i;

	for (i = 0; i < IMR_NUM; i++)
		writel_relaxed(gpc_saved_imrs[i], reg_imr1 + i * 4);
}

void imx_gpc_irq_unmask(struct irq_data *d)
{
	void __iomem *reg;
	u32 val;

	/* Sanity check for SPI irq */
	if (d->num < 32)
		return;

	reg = gpc_base + GPC_IMR1 + (d->num / 32 - 1) * 4;
	val = readl_relaxed(reg);
	val &= ~(1 << d->num % 32);
	writel_relaxed(val, reg);
}

void imx_gpc_irq_mask(struct irq_data *d)
{
	void __iomem *reg;
	u32 val;

	/* Sanity check for SPI irq */
	if (d->num < 32)
		return;

	reg = gpc_base + GPC_IMR1 + (d->num / 32 - 1) * 4;
	val = readl_relaxed(reg);
	val |= 1 << (d->num % 32);
	writel_relaxed(val, reg);
}

void __init imx_gpc_init(void)
{
	struct device_node *np;
	virtual_addr_t vbase = 0;
	int i;

	if (NULL == (np = vmm_devtree_find_compatible(NULL, NULL, "fsl,imx6q-gpc")))
	{
		printk("Failed to find compatible GPC node\n");
		return;
	}
	if (VMM_OK != vmm_devtree_regmap(np, &vbase, 0))
	{
		printk("Failed to map GPC registers\n");
		return;
	}
	gpc_base = (void __iomem *)vbase;

	/* Initially mask all interrupts */
	for (i = 0; i < IMR_NUM; i++)
		writel_relaxed(~0, gpc_base + GPC_IMR1 + i * 4);

	/* Register GPC as the secondary interrupt controller behind GIC */
	printk("FIXME: GPC is the secondary interrupt controller behind "
		   "GIC\n");
	/* gic_arch_extn.irq_mask = imx_gpc_irq_mask; */
	/* gic_arch_extn.irq_unmask = imx_gpc_irq_unmask; */
	/* gic_arch_extn.irq_set_wake = imx_gpc_irq_set_wake; */
}
