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
 * @file exynos4.c
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @brief exynos4 board specific code
 */

#include <vmm_error.h>
#include <vmm_main.h>
#include <vmm_stdio.h>
#include <vmm_devtree.h>
#include <vmm_host_io.h>
#include <vmm_host_aspace.h>
#include <vmm_delay.h>

#include <generic_board.h>

#include <exynos/regs-pmu.h>

/*
 * Global board context
 */

static virtual_addr_t exynos4_sys_base;

/*
 * Samsung processor revision variables
 */
static u32 __samsung_cpu_id = 0;
static u32 __samsung_cpu_rev = 0;

static void __init exynos_init_cpu(physical_addr_t cpuid_addr)
{
        virtual_addr_t virt_addr = vmm_host_iomap(cpuid_addr, sizeof(__samsung_cpu_id));

        if (virt_addr) {
                __samsung_cpu_id = vmm_readl((void *)virt_addr);
                __samsung_cpu_rev = __samsung_cpu_id & 0xFF;

                vmm_host_iounmap(virt_addr);
        }
}

/*
 * Samsung processor revision API
 */
u32 samsung_rev(void)
{
        return __samsung_cpu_rev;
}

u32 samsung_cpu_id(void)
{
        return __samsung_cpu_id;
}

/*
 * Reset & Shutdown
 */

static int exynos4_reset(void)
{
	if (exynos4_sys_base) {
		/* Trigger a Software reset */
		vmm_writel(0x1, (void *)(exynos4_sys_base + EXYNOS_SWRESET));
	}

	vmm_mdelay(500);

	return VMM_EFAIL;
}

static int exynos4_shutdown(void)
{
	if (exynos4_sys_base) {
		/* Trigger a Software reset */
		vmm_writel(0x1, (void *)(exynos4_sys_base + EXYNOS_SWRESET));
	}

	vmm_mdelay(500);

	return VMM_EFAIL;
}

/*
 * Initialization functions
 */

static int __init exynos4_early_init(struct vmm_devtree_node *node)
{
	int rc;

	/* Host aspace, Heap, Device tree, and Host IRQ available.
	 *
	 * Do necessary early stuff like:
	 * iomapping devices, 
	 * SOC clocking init, 
	 * Setting-up system data in device tree nodes,
	 * ....
	 */

	/* Initalize some code that will help determine the SOC type */
	exynos_init_cpu(EXYNOS_PA_CHIPID);

	/* Map sysreg */
	node = vmm_devtree_find_compatible(NULL, NULL, "arm,a9mpcore-priv");
	if (!node) {
		return VMM_ENODEV;
	}
	rc = vmm_devtree_regmap(node, &exynos4_sys_base, 0);
	vmm_devtree_dref_node(node);
	if (rc) {
		return rc;
	}

	/* Register reset & shutdown callbacks */
	vmm_register_system_reset(exynos4_reset);
	vmm_register_system_shutdown(exynos4_shutdown);

	return 0;
}

static int __init exynos4_final_init(struct vmm_devtree_node *node)
{
	/* Nothing to do here. */
	return VMM_OK;
}

static struct generic_board exynos4_info = {
	.name		= "Exynos4",
	.early_init	= exynos4_early_init,
	.final_init	= exynos4_final_init,
};

GENERIC_BOARD_DECLARE(exynos4, "samsung,exynos4", &exynos4_info);
