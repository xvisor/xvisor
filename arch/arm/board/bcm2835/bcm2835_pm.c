/**
 * Copyright (c) 2013 Anup Patel.
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
 * @file bcm2835_pm.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief BCM2835 PM and Watchdog implementation
 */

#include <vmm_error.h>
#include <vmm_main.h>
#include <vmm_compiler.h>
#include <vmm_stdio.h>
#include <vmm_host_io.h>
#include <vmm_devtree.h>

#include <bcm2835_pm.h>

static virtual_addr_t pm_base_va;

#define PM_RSTC 			((void *)pm_base_va+0x1c)
#define PM_RSTS 			((void *)pm_base_va+0x20)
#define PM_WDOG 			((void *)pm_base_va+0x24)

#define PM_WDOG_RESET 			0000000000
#define PM_PASSWORD 			0x5a000000
#define PM_WDOG_TIME_SET 		0x000fffff
#define PM_RSTC_WRCFG_CLR 		0xffffffcf
#define PM_RSTC_WRCFG_SET 		0x00000030
#define PM_RSTC_WRCFG_FULL_RESET 	0x00000020
#define PM_RSTC_RESET 			0x00000102

#define PM_RSTS_HADPOR_SET		0x00001000
#define PM_RSTS_HADSRH_SET		0x00000400
#define PM_RSTS_HADSRF_SET		0x00000200
#define PM_RSTS_HADSRQ_SET		0x00000100
#define PM_RSTS_HADWRH_SET		0x00000040
#define PM_RSTS_HADWRF_SET		0x00000020
#define PM_RSTS_HADWRQ_SET		0x00000010
#define PM_RSTS_HADDRH_SET		0x00000004
#define PM_RSTS_HADDRF_SET		0x00000002
#define PM_RSTS_HADDRQ_SET		0x00000001

static int bcm2835_pm_reset(void)
{
	u32 pm_rstc, pm_wdog;
	u32 timeout = 10;

	/* Setup watchdog for reset */
	pm_rstc = vmm_readl(PM_RSTC);

	/* watchdog timer = timer clock / 16; 
	 * need password (31:16) + value (11:0) 
	 */
	pm_wdog  = PM_PASSWORD;
	pm_wdog |= (timeout & PM_WDOG_TIME_SET);
	pm_rstc  = PM_PASSWORD;
	pm_rstc |= (pm_rstc & PM_RSTC_WRCFG_CLR);
	pm_rstc |= PM_RSTC_WRCFG_FULL_RESET;

	vmm_writel(pm_wdog, PM_WDOG);
	vmm_writel(pm_rstc, PM_RSTC);

	return VMM_OK;
}

static int bcm2835_pm_poweroff(void)
{
	/* we set the watchdog hard reset bit here to distinguish this reset 
	 * from the normal (full) reset. bootcode.bin will not reboot after 
	 * a hard reset 
	 */
	u32 pm_rsts = vmm_readl(PM_RSTS);

	pm_rsts  = PM_PASSWORD;
	pm_rsts |= (pm_rsts & PM_RSTC_WRCFG_CLR);
	pm_rsts |= PM_RSTS_HADWRH_SET;

	vmm_writel(pm_rsts, PM_RSTS);

	return bcm2835_pm_reset();
}

int __init bcm2835_pm_init(void)
{
	int rc;
	struct vmm_devtree_node *node;

	node = vmm_devtree_find_compatible(NULL, NULL, 
					   "brcm,bcm2835-pm-wdt");
	if (!node) {
		return VMM_ENODEV;
	}

	rc = vmm_devtree_regmap(node, &pm_base_va, 0);
	if (rc) {
		return rc;
	}

	/* Register reset & shutdown callbacks */
	vmm_register_system_reset(bcm2835_pm_reset);
	vmm_register_system_shutdown(bcm2835_pm_poweroff);

	return VMM_OK;
}

