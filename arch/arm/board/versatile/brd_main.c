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
#include <vmm_stdio.h>
#include <vmm_devtree.h>
#include <vmm_devdrv.h>
#include <vmm_host_io.h>
#include <vmm_host_aspace.h>
#include <arch_board.h>
#include <arch_timer.h>
#include <versatile_plat.h>
#include <versatile_board.h>
#include <sp804_timer.h>

static virtual_addr_t versatile_sys_base;
static virtual_addr_t versatile_sctl_base;
static virtual_addr_t versatile_sp804_base;
static u32 versatile_sp804_irq;

/*
 * Reset & Shutdown
 */

int arch_board_reset(void)
{
	vmm_writel(0x101, (void *)(versatile_sys_base +
			   VERSATILE_SYS_RESETCTL_OFFSET));

	return VMM_OK;
}

int arch_board_shutdown(void)
{
	/* FIXME: TBD */
	return VMM_OK;
}

/*
 * Initialization functions
 */

int __init arch_board_early_init(void)
{
	int rc;
	u32 val;
	struct vmm_devtree_node *node;

	/* Host aspace, Heap, Device tree, and Host IRQ available.
	 *
	 * Do necessary early stuff like:
	 * iomapping devices, 
	 * SOC clocking init, 
	 * Setting-up system data in device tree nodes,
	 * ....
	 */

	/* Map sysreg */
	node = vmm_devtree_find_compatible(NULL, NULL, "arm,versatile-sysreg");
	if (!node) {
		return VMM_ENODEV;
	}
	rc = vmm_devtree_regmap(node, &versatile_sys_base, 0);
	if (rc) {
		return rc;
	}

	/* Map sysctl */
	node = vmm_devtree_find_compatible(NULL, NULL, "arm,versatile-sctl");
	if (!node) {
		return VMM_ENODEV;
	}
	rc = vmm_devtree_regmap(node, &versatile_sctl_base, 0);
	if (rc) {
		return rc;
	}

	/* Select reference clock for sp804 timers: 
	 *      REFCLK is 32KHz
	 *      TIMCLK is 1MHz
	 */
	val = vmm_readl((void *)versatile_sctl_base) | 
			(VERSATILE_TIMCLK << VERSATILE_TIMER1_EnSel) |
			(VERSATILE_TIMCLK << VERSATILE_TIMER2_EnSel) |
			(VERSATILE_TIMCLK << VERSATILE_TIMER3_EnSel) |
			(VERSATILE_TIMCLK << VERSATILE_TIMER4_EnSel);
	vmm_writel(val, (void *)versatile_sctl_base);

	/* Map sp804 registers */
	node = vmm_devtree_find_compatible(NULL, NULL, "arm,sp804");
	if (!node) {
		return VMM_ENODEV;
	}
	rc = vmm_devtree_regmap(node, &versatile_sp804_base, 0);
	if (rc) {
		return rc;
	}

	/* Get sp804 irq */
	rc = vmm_devtree_irq_get(node, &versatile_sp804_irq, 0);
	if (rc) {
		return rc;
	}

	return 0;
}

int __init arch_clocksource_init(void)
{
	int rc;

	/* Initialize sp804 timer0 as clocksource */
	rc = sp804_clocksource_init(versatile_sp804_base, 
				    "sp804_timer0", 1000000);
	if (rc) {
		vmm_printf("%s: sp804 clocksource init failed (error %d)\n", 
			   __func__, rc);
	}

	return VMM_OK;
}

int __cpuinit arch_clockchip_init(void)
{
	int rc;

	/* Initialize sp804 timer1 as clockchip */
	rc = sp804_clockchip_init(versatile_sp804_base + 0x20, 
				  "sp804_timer1", versatile_sp804_irq, 
				  1000000, 0);
	if (rc) {
		vmm_printf("%s: sp804 clockchip init failed (error %d)\n", 
			   __func__, rc);
	}

	return VMM_OK;
}

int __init arch_board_final_init(void)
{
	int rc;
	struct vmm_devtree_node *node;

	/* All VMM API's are available here */
	/* We can register a Board specific resource here */

	/* Find simple-bus node */
	node = vmm_devtree_find_compatible(NULL, NULL, "simple-bus");
	if (!node) {
		return VMM_ENODEV;
	}

	/* Do probing using device driver framework */
	rc = vmm_devdrv_probe(node);
	if (rc) {
		return rc;
	}

	return VMM_OK;
}
