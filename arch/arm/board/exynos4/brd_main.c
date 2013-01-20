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
 * @file brd_main.c
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @brief main source file for board specific code
 */

#include <vmm_error.h>
#include <vmm_smp.h>
#include <vmm_delay.h>
#include <vmm_stdio.h>
#include <vmm_spinlocks.h>
#include <vmm_devtree.h>
#include <vmm_devdrv.h>
#include <vmm_host_io.h>
#include <vmm_host_aspace.h>
#include <libs/libfdt.h>

#include <exynos/plat/cpu.h>

#include <exynos/mct_timer.h>

/*
 * Global board context
 */

/*
 * Device Tree support
 */

extern u32 dt_blob_start;

int arch_board_ram_start(physical_addr_t * addr)
{
	int rc = VMM_OK;
	struct fdt_fileinfo fdt;
	struct fdt_node_header *fdt_node;

	rc = libfdt_parse_fileinfo((virtual_addr_t) & dt_blob_start, &fdt);
	if (rc) {
		return rc;
	}

	fdt_node = libfdt_find_node(&fdt,
				    VMM_DEVTREE_PATH_SEPARATOR_STRING
				    VMM_DEVTREE_HOSTINFO_NODE_NAME
				    VMM_DEVTREE_PATH_SEPARATOR_STRING
				    VMM_DEVTREE_MEMORY_NODE_NAME);
	if (!fdt_node) {
		return VMM_EFAIL;
	}

	rc = libfdt_get_property(&fdt, fdt_node,
				 VMM_DEVTREE_MEMORY_PHYS_ADDR_ATTR_NAME, addr);
	if (rc) {
		return rc;
	}

	return VMM_OK;
}

int arch_board_ram_size(physical_size_t * size)
{
	int rc = VMM_OK;
	struct fdt_fileinfo fdt;
	struct fdt_node_header *fdt_node;

	rc = libfdt_parse_fileinfo((virtual_addr_t) & dt_blob_start, &fdt);
	if (rc) {
		return rc;
	}

	fdt_node = libfdt_find_node(&fdt,
				    VMM_DEVTREE_PATH_SEPARATOR_STRING
				    VMM_DEVTREE_HOSTINFO_NODE_NAME
				    VMM_DEVTREE_PATH_SEPARATOR_STRING
				    VMM_DEVTREE_MEMORY_NODE_NAME);
	if (!fdt_node) {
		return VMM_EFAIL;
	}

	rc = libfdt_get_property(&fdt, fdt_node,
				 VMM_DEVTREE_MEMORY_PHYS_SIZE_ATTR_NAME, size);
	if (rc) {
		return rc;
	}

	return VMM_OK;
}

int arch_board_devtree_populate(struct vmm_devtree_node **root)
{
	int rc = VMM_OK;
	struct fdt_fileinfo fdt;

	rc = libfdt_parse_fileinfo((virtual_addr_t) & dt_blob_start, &fdt);
	if (rc) {
		return rc;
	}

	return libfdt_parse_devtree(&fdt, root);
}

/*
 * Reset & Shutdown
 */

int arch_board_reset(void)
{
#if 0
	int tmp = 0;

	vmm_host_physical_write(EXYNOS4_PA_WATCHDOG + 0x00, &tmp, sizeof(tmp));

	tmp = 0x80;

	vmm_host_physical_write(EXYNOS4_PA_WATCHDOG + 0x04, &tmp, sizeof(tmp));
	vmm_host_physical_write(EXYNOS4_PA_WATCHDOG + 0x08, &tmp, sizeof(tmp));

	tmp = 0x2025;

	vmm_host_physical_write(EXYNOS4_PA_WATCHDOG + 0x00, &tmp, sizeof(tmp));
#else
	void *ptr = (void *)vmm_host_iomap(0x10020000, 0x1000);

	vmm_writel(0x1, ptr + 0x400);
#endif

	vmm_mdelay(500);

	vmm_printf("%s: failed\n", __func__);

	return VMM_OK;
}

int arch_board_shutdown(void)
{
	vmm_writel(0x1, (void *)(EXYNOS4_PA_PMU + 0x400));

	return VMM_OK;
}

#if 0
/*
 * Clocking support
 */

static long ct_round(struct exynos_clk *clk, unsigned long rate)
{
	return rate;
}

static int ct_set(struct exynos_clk *clk, unsigned long rate)
{
	return v2m_cfg_write(SYS_CFG_OSC | SYS_CFG_SITE_DB1 | 1, rate);
}

static const struct exynos_clk_ops osc1_clk_ops = {
	.round = ct_round,
	.set = ct_set,
};

static struct exynos_clk osc1_clk = {
	.ops = &osc1_clk_ops,
	.rate = 24000000,
};

static struct vmm_devclk clcd_clk = {
	.enable = exynos_clk_enable,
	.disable = exynos_clk_disable,
	.get_rate = exynos_clk_get_rate,
	.round_rate = exynos_clk_round_rate,
	.set_rate = exynos_clk_set_rate,
	.priv = &osc1_clk,
};

static struct vmm_devclk *exynos_getclk(struct vmm_devtree_node *node)
{
	if (strcmp(node->name, "clcd") == 0) {
		return &clcd_clk;
	}

	return NULL;
}
#endif

/*
 * Initialization functions
 */

int __init arch_board_early_init(void)
{
	/* Initalize some code that will help determine the SOC type */
	exynos_init_cpu(EXYNOS_PA_CHIPID);

	/*
	 * TODO:
	 * Host virtual memory, device tree, heap is up.
	 * Do necessary early stuff like iomapping devices
	 * memory or boot time memory reservation here.
	 */
	return 0;
}

static virtual_addr_t mct_timer_base;

int __init arch_clocksource_init(void)
{
	int rc;
	struct vmm_devtree_node *node;

	/* Map timer0 registers */
	node = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPARATOR_STRING
				   VMM_DEVTREE_HOSTINFO_NODE_NAME
				   VMM_DEVTREE_PATH_SEPARATOR_STRING "mct");
	if (!node) {
		rc = VMM_EFAIL;
		goto skip_mct_timer_init;
	}

	if (!mct_timer_base) {
		rc = vmm_devtree_regmap(node, &mct_timer_base, 0);
		if (rc) {
			return rc;
		}
	}

	/* Initialize mct as clocksource */
	rc = exynos4_clocksource_init(mct_timer_base, node->name, 300, 1000000,
				      20);
	if (rc) {
		return rc;
	}
 skip_mct_timer_init:

	return rc;
}

int __cpuinit arch_clockchip_init(void)
{
	int rc;
	struct vmm_devtree_node *node;
	u32 val, *valp, cpu = vmm_smp_processor_id();

	if (!cpu) {
		/* Map timer0 registers */
		node = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPARATOR_STRING
					   VMM_DEVTREE_HOSTINFO_NODE_NAME
					   VMM_DEVTREE_PATH_SEPARATOR_STRING
					   "mct");
		if (!node) {
			goto skip_mct_timer_init;
		}

		if (!mct_timer_base) {
			rc = vmm_devtree_regmap(node, &mct_timer_base, 0);
			if (rc) {
				return rc;
			}
		}

		/* Get MCT irq */
		valp = vmm_devtree_attrval(node, "irq");
		if (!valp) {
			return VMM_EFAIL;
		}

		val = *valp;

		/* Initialize MCT as clockchip */
		rc = exynos4_clockchip_init(mct_timer_base, val, node->name,
					    300, 1000000, 0);
		if (rc) {
			return rc;
		}

	}
 skip_mct_timer_init:

#if CONFIG_SAMSUNG_MCT_LOCAL_TIMERS
	if (mct_timer_base) {
		exynos4_local_timer_init(mct_timer_base, 0, "mct_tick", 450,
					 1000000);
	}
#endif

	return VMM_OK;
}

int __init arch_board_final_init(void)
{
	int rc;
	struct vmm_devtree_node *node;
#if defined(CONFIG_VTEMU)
	struct vmm_fb_info *info;
#endif

	/* All VMM API's are available here */
	/* We can register a Board specific resource here */

	/* Do Probing using device driver framework */
	node = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPARATOR_STRING
				   VMM_DEVTREE_HOSTINFO_NODE_NAME
				   VMM_DEVTREE_PATH_SEPARATOR_STRING
				   "sfrregion");

	if (!node) {
		return VMM_ENOTAVAIL;
	}

	//rc = vmm_devdrv_probe(node, exynos_getclk, NULL);
	rc = vmm_devdrv_probe(node, NULL, NULL);
	if (rc) {
		return rc;
	}

	return VMM_OK;
}
