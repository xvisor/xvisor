/**
 * Copyright (c) 2012 Jean-Christophe Dubois.
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
 * @file smp_scu.c
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @brief SCU API code
 *
 * Adapted from linux/arch/arm/kernel/smp_scu.c
 *
 *  Copyright (C) 2002 ARM Ltd.
 *  All Rights Reserved
 *
 * The original source is licensed under GPL.
 */

#include <vmm_error.h>
#include <vmm_smp.h>
#include <vmm_cache.h>
#include <vmm_host_io.h>
#include <vmm_host_irq.h>
#include <vmm_host_aspace.h>

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

#ifdef CONFIG_SMP
/*
 * Get the number of CPU cores from the SCU configuration
 */
u32 scu_get_core_count(void *scu_base)
{
	return (vmm_readl(scu_base + SCU_CONFIG) & 0x03) + 1;
}

bool scu_cpu_core_is_smp(void *scu_base, u32 cpu)
{
	return (vmm_readl(scu_base + SCU_CONFIG) >> (4 + cpu)) & 0x01;
}

/*
 * Enable the SCU
 */
void scu_enable(void *scu_base)
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
#endif

/*
 * Set the executing CPUs power mode as defined.  This will be in
 * preparation for it executing a WFI instruction.
 *
 * This function must be called with preemption disabled, and as it
 * has the side effect of disabling coherency, caches must have been
 * flushed.  Interrupts must also have been disabled.
 */
int scu_power_mode(void *scu_base, u32 mode)
{
	u32 val, cpu;

	cpu = vmm_smp_processor_id();

	if (mode > SCU_PM_POWEROFF || mode == SCU_PM_EINVAL || cpu > 3) {
		return VMM_EFAIL;
	}

	val = vmm_readb(scu_base + SCU_CPU_STATUS + cpu) & ~0x03;
	val |= mode;
	vmm_writeb(val, scu_base + SCU_CPU_STATUS + cpu);

	return VMM_OK;
}

#if defined(CONFIG_ARM_SMP_OPS) && defined(CONFIG_ARM_GIC)

static virtual_addr_t scu_base;
static virtual_addr_t clear_addr[CONFIG_CPU_COUNT];
static virtual_addr_t release_addr[CONFIG_CPU_COUNT];

static struct vmm_devtree_nodeid scu_matches[] = {
	{.compatible = "arm,arm11mp-scu"},
	{.compatible = "arm,cortex-a9-scu"},
	{ /* end of list */ },
};

static int __init scu_cpu_init(struct vmm_devtree_node *node,
				unsigned int cpu)
{
	int rc;
	u32 ncores;
	physical_addr_t pa;
	struct vmm_devtree_node *scu_node;

	/* Map SCU base */
	if (!scu_base) {
		scu_node = vmm_devtree_find_matching(NULL, scu_matches);
		if (!scu_node) {
			return VMM_ENODEV;
		}
		rc = vmm_devtree_regmap(scu_node, &scu_base, 0);
		if (rc) {
			return rc;
		}
	}

	/* Map clear address */
	rc = vmm_devtree_read_physaddr(node,
			VMM_DEVTREE_CPU_CLEAR_ADDR_ATTR_NAME, &pa);
	if (rc) {
		clear_addr[cpu] = 0x0;
	} else {
		clear_addr[cpu] = vmm_host_iomap(pa, VMM_PAGE_SIZE);
	}

	/* Map release address */
	rc = vmm_devtree_read_physaddr(node,
			VMM_DEVTREE_CPU_RELEASE_ADDR_ATTR_NAME, &pa);
	if (rc) {
		release_addr[cpu] = 0x0;
	} else {
		release_addr[cpu] = vmm_host_iomap(pa, VMM_PAGE_SIZE);
	}

	/* Check core count from SCU */
	ncores = scu_get_core_count((void *)scu_base);
	if (ncores <= cpu) {
		return VMM_ENOSYS;
	}

	/* Check SCU status */
	if (!scu_cpu_core_is_smp((void *)scu_base, cpu)) {
		return VMM_ENOSYS;
	}

	return VMM_OK;
}

extern u8 _start_secondary_nopen;

static int __init scu_cpu_prepare(unsigned int cpu)
{
	int rc;
	physical_addr_t _start_secondary_pa;

	/* Get physical address secondary startup code */
	rc = vmm_host_va2pa((virtual_addr_t)&_start_secondary_nopen,
			    &_start_secondary_pa);
	if (rc) {
		return rc;
	}

	/* Enable snooping through SCU */
	if (scu_base) {
		scu_enable((void *)scu_base);
	}

	/* Write to clear address */
	if (clear_addr[cpu]) {
		vmm_writel(~0x0, (void *)clear_addr[cpu]);
	}

	/* Write to release address */
	if (release_addr[cpu]) {
		vmm_writel((u32)_start_secondary_pa,
					(void *)release_addr[cpu]);
	}

	return VMM_OK;
}

static int __init scu_cpu_boot(unsigned int cpu)
{
	const struct vmm_cpumask *mask = get_cpu_mask(cpu);

	/* Wakeup target cpu from wfe/wfi by sending an IPI */
	vmm_host_irq_raise(0, mask);

	return VMM_OK;
}

struct smp_operations smp_scu_ops = {
	.name = "smp-scu",
	.cpu_init = scu_cpu_init,
	.cpu_prepare = scu_cpu_prepare,
	.cpu_boot = scu_cpu_boot,
};

SMP_OPS_DECLARE(smp_scu, &smp_scu_ops);

#endif
