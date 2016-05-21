/*
 * Copyright (C) 2014,2016 Institut de Recherche Technologique SystemX and OpenWide.
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
 * @author Jean Guyomarc'h (jean.guyomarch@openwide.fr)
 * @brief Freescale i.MX GPC function helpers
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/printk.h>
#include <imx6qdl-clock.h>
#include <imx-common.h>

#define GPC_CNTR		0x000
#define GPC_IMR1		0x008
#define GPC_PGC_GPU_PDN		0x260
#define GPC_PGC_GPU_PUPSCR	0x264
#define GPC_PGC_GPU_PDNSCR	0x268
#define GPC_PGC_CPU_PDN		0x2a0
#define GPU_VPU_PUP_REQ		BIT(1)
#define GPU_VPU_PDN_REQ		BIT(0)

#define IMR_NUM			4

#define GPC_CLK_MAX		6

#define DT_COMPATIBLE		"fsl,imx6q-gpc"

static void __iomem *gpc_base;
//static u32 gpc_wake_irqs[IMR_NUM];
static u32 gpc_saved_imrs[IMR_NUM];
static struct clk *clocks[GPC_CLK_MAX];
static unsigned int clocks_count = 0;

#if 0
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
#endif

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

void __init imx_gpc_clocks_init(void)
{
	struct device_node *np;
	struct clk *clk;
	int i, chk;

	np = vmm_devtree_find_compatible(NULL, NULL, DT_COMPATIBLE);
	if (NULL == np) {
		vmm_lerror("Failed to find compatible GPC node \"%s\"\n",
				DT_COMPATIBLE);
		return;
	}

	for (i = 0; ; i++) {
		clk = of_clk_get(np, i);
		if (IS_ERR(clk)) {
			break; /* No more clocks */
		}
		if (i >= GPC_CLK_MAX) {
			vmm_lerror("imx-gpc", "Too many clocks for GPC node\n");
			while (i--) {
				clk_put(clocks[i]);
			}
			return;
		}
		clocks[i] = clk;
	}
	clocks_count = i;

	/* Start clocks */
	for (i = 0; i < clocks_count; i++) {
		chk = clk_prepare_enable(clocks[i]);
		if (chk != 0) {
			vmm_lerror("imx-gpc", "error %i, failed to enable clock %s\n",
					chk, __clk_get_name(clocks[i]));
		}
	}
}

void __init imx_gpc_init(void)
{
	struct device_node *np;
	virtual_addr_t vbase = 0;
	int i, rc;

	np = vmm_devtree_find_compatible(NULL, NULL, DT_COMPATIBLE);
	if (!np) {
		printk("Failed to find compatible GPC node\n");
		return;
	}
	rc = vmm_devtree_regmap(np, &vbase, 0);
	vmm_devtree_dref_node(np);
	if (VMM_OK != rc) {
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
