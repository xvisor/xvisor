/**
 * Copyright (c) 2016 Jean-Christophe Dubois.
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
 * @file smp_imx.c
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @brief i.MX SMP boot support
 *
 * Adapted from arch/arm/mach-imx/platsmp.c
 *
 *  Copyright 2011 Freescale Semiconductor, Inc.
 *  All Rights Reserved
 *
 * The original source is licensed under GPL.
 */

#include <vmm_error.h>
#include <vmm_smp.h>
#include <vmm_cache.h>
#include <vmm_stdio.h>
#include <vmm_host_io.h>
#include <vmm_host_irq.h>
#include <vmm_host_aspace.h>
#include <arch_barrier.h>

#include <smp_ops.h>

#define SCU_PM_NORMAL	0
#define SCU_PM_EINVAL	1
#define SCU_PM_DORMANT	2
#define SCU_PM_POWEROFF	3

#define SCU_CTRL		0x00
#define SCU_CONFIG		0x04
#define SCU_CPU_STATUS		0x08
#define SCU_INVALIDATE		0x0c
#define SCU_FPGA_REVISION	0x10

#define SRC_SCR			0x000
#define SRC_GPR1		0x020
#define BP_SRC_SCR_CORE1_RST	14
#define BP_SRC_SCR_CORE1_ENABLE	22

/*
 * Get the number of CPU cores from the SCU configuration
 */
static u32 scu_get_core_count(void *scu_base)
{
	return (vmm_readl(scu_base + SCU_CONFIG) & 0x03) + 1;
}

static bool smp_imx_core_is_smp(void *scu_base, u32 cpu)
{
	return (vmm_readl(scu_base + SCU_CONFIG) >> (4 + cpu)) & 0x01;
}

/*
 * Enable the SCU
 */
static void scu_enable(void *scu_base)
{
	u32 scu_ctrl;

#ifdef CONFIG_ARM_ERRATA_764369
	/*
	 * This code is mostly for TEGRA 2 and 3 processors. 
	 * This in not enabled or tested on Xvisor for now.
	 * We keep it as we might have to enable it someday.
	 */
	/* Cortex-A9 only */
	if ((read_cpuid(CPUID_ID) & 0xff0ffff0) == 0x410fc090) {

		scu_ctrl = vmm_readl(scu_base + 0x30);
		if (!(scu_ctrl & 1)) {
			vmm_writel(scu_ctrl | 0x1, scu_base + 0x30);
		}
	}
#endif

	scu_ctrl = vmm_readl(scu_base + SCU_CTRL);
	/* already enabled? */
	if (scu_ctrl & 1) {
		return;
	}

	scu_ctrl |= 1;
	vmm_writel(scu_ctrl, scu_base + SCU_CTRL);

	/*
	 * Ensure that the data accessed by CPU0 before the SCU was
	 * initialised is visible to the other CPUs.
	 */
	vmm_flush_cache_all();
}

static virtual_addr_t scu_base;

static const struct vmm_devtree_nodeid scu_matches[] = {
	{.compatible = "arm,arm11mp-scu"},
	{.compatible = "arm,cortex-a9-scu"},
	{ /* end of list */ },
};

static virtual_addr_t src_base;

static const struct vmm_devtree_nodeid src_matches[] = {
	{ .compatible = "fsl,imx51-src"},
	{ .compatible = "fsl,imx6-src"},
	{ /* sentinel */ }
};

static void __init smp_imx_ops_init(void)
{
	int rc;
	struct vmm_devtree_node *scu_node;
	struct vmm_devtree_node *src_node;

	scu_node = vmm_devtree_find_matching(NULL, scu_matches);
	if (!scu_node) {
		return;
	}

	rc = vmm_devtree_regmap(scu_node, &scu_base, 0);
	vmm_devtree_dref_node(scu_node);
	if (rc) {
		vmm_printf("%s: failed to map SCU registers\n", __func__);
		return;
	}

	src_node = vmm_devtree_find_matching(NULL, src_matches);
	if (!src_node) {
		return;
	}

	rc = vmm_devtree_regmap(src_node, &src_base, 0);
	vmm_devtree_dref_node(src_node);
	if (rc) {
		vmm_printf("%s: failed to map SRC registers\n", __func__);
		return;
	}
}

static int __init smp_imx_cpu_init(struct vmm_devtree_node *node,
				   unsigned int cpu)
{
	u32 ncores;

	/* Check SCU base and SRC base */
	if (!scu_base || !src_base) {
		return VMM_ENODEV;
	}

	/* Check core count from SCU */
	ncores = scu_get_core_count((void *)scu_base);
	if (ncores <= cpu) {
		return VMM_ENOSYS;
	}

	/* Check SCU status */
	if (!smp_imx_core_is_smp((void *)scu_base, cpu)) {
		return VMM_ENOSYS;
	}

	return VMM_OK;
}

static void smp_imx_set_cpu_jump(int cpu, void *jump_addr)
{
	physical_addr_t paddr;

	if (VMM_OK != vmm_host_va2pa((virtual_addr_t)jump_addr,
				     &paddr)) {
		vmm_printf("Failed to get cpu jump physical address "
			   "(0x%p)\n", jump_addr);
	}
	vmm_writel(paddr, (void *)src_base + SRC_GPR1 + cpu * 8);
}

static void smp_imx_set_cpu_arg(int cpu, u32 arg)
{
	vmm_writel(arg, (void *)src_base + SRC_GPR1 + cpu * 8 + 4);
}

extern u8 _start_secondary_nopen;

static int __init smp_imx_cpu_prepare(unsigned int cpu)
{
	/* Enable snooping through SCU */
	if (scu_base) {
		scu_enable((void *)scu_base);
	}

	smp_imx_set_cpu_jump(cpu, (void *)&_start_secondary_nopen);

	smp_imx_set_cpu_arg(cpu, 0);

	return VMM_OK;
}

static void smp_imx_enable_cpu(int cpu, bool enable)
{
	u32 mask, val;

	mask = 1 << (BP_SRC_SCR_CORE1_ENABLE + cpu - 1);
	val = vmm_readl((void *)src_base + SRC_SCR);
	val = enable ? val | mask : val & ~mask;
	val |= 1 << (BP_SRC_SCR_CORE1_RST + cpu - 1);
	vmm_writel(val, (void *)src_base + SRC_SCR);
}

static int __init smp_imx_cpu_boot(unsigned int cpu)
{
	/* Wake up the core through the SRC device */
	smp_imx_enable_cpu(cpu, true);

	return VMM_OK;
}

static struct smp_operations smp_imx_ops = {
	.name = "smp-imx",
	.ops_init = smp_imx_ops_init,
	.cpu_init = smp_imx_cpu_init,
	.cpu_prepare = smp_imx_cpu_prepare,
	.cpu_boot = smp_imx_cpu_boot,
};

SMP_OPS_DECLARE(smp_imx, &smp_imx_ops);
