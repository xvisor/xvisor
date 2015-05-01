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
#include <vmm_main.h>
#include <vmm_smp.h>
#include <vmm_delay.h>
#include <vmm_stdio.h>
#include <vmm_spinlocks.h>
#include <vmm_devtree.h>
#include <vmm_devdrv.h>
#include <vmm_host_io.h>
#include <vmm_host_aspace.h>
#include <arch_board.h>
#include <arch_timer.h>

#include <exynos/plat/cpu.h>
#include <exynos/regs-watchdog.h>
#include <exynos/regs-clock.h>

/*
 * Global board context
 */
static virtual_addr_t pmu_base;

/*
 * Reset & Shutdown
 */

static int exynos4_reset(void)
{
#if 0
	void *wdt_ptr = (void *)vmm_host_iomap(EXYNOS4_PA_WATCHDOG, 0x100);

	if (wdt_ptr) {
		u32 perir_reg;
		void *cmu_ptr =
		    (void *)vmm_host_iomap(EXYNOS4_PA_CMU +
					   EXYNOS4_CLKGATE_IP_PERIR,
					   sizeof(perir_reg));

		if (cmu_ptr) {
			vmm_printf("%s: CMU reg is at 0x%08x + 0x%08x\n",
				   __func__, EXYNOS4_PA_CMU,
				   EXYNOS4_CLKGATE_IP_PERIR);

			vmm_writel(0, wdt_ptr + S3C2410_WTCON);

			/* enable the WDT clock if it is not already enabled */
			perir_reg = vmm_readl(cmu_ptr);

			vmm_printf("%s: CMU PERIR reg is 0x%08x\n", __func__,
				   perir_reg);
			if (!(perir_reg & (1 << 14))) {
				perir_reg |= (1 << 14);
				vmm_printf
				    ("%s: enabling WDT in PERIR: writing 0x%08x\n",
				     __func__, perir_reg);
				vmm_writel(perir_reg, cmu_ptr);
			}

			vmm_writel(0x80, wdt_ptr + S3C2410_WTDAT);
			vmm_writel(0x80, wdt_ptr + S3C2410_WTCNT);

			vmm_writel(0x2025, wdt_ptr + S3C2410_WTCON);

			vmm_host_iounmap((virtual_addr_t)cmu_ptr);
		}

		vmm_host_iounmap((virtual_addr_t)wdt_ptr);
	}
#else
	if (pmu_base) {
		/* Trigger a Software reset */
		vmm_writel(0x1, (void *)(pmu_base + EXYNOS_SWRESET));
	}
#endif

	vmm_mdelay(500);

	return VMM_EFAIL;
}

static int exynos4_shutdown(void)
{
	/* FIXME: For now we do a soft reset */
	if (pmu_base) {
		/* Trigger a Software reset */
		vmm_writel(0x1, (void *)(pmu_base + EXYNOS_SWRESET));
	}

	vmm_mdelay(500);

	return VMM_EFAIL;
}

/*
 * Print board information
 */

void arch_board_print_info(struct vmm_chardev *cdev)
{
	/* FIXME: To be implemented. */
}

/*
 * Initialization functions
 */

int __init arch_board_early_init(void)
{
	/* Initalize some code that will help determine the SOC type */
	exynos_init_cpu(EXYNOS_PA_CHIPID);

	/* Map PMU base */
	pmu_base = vmm_host_iomap(EXYNOS4_PA_PMU, 0x1000);

	/* Register reset & shutdown callbacks */
	vmm_register_system_reset(exynos4_reset);
	vmm_register_system_shutdown(exynos4_shutdown);

	/*
	 * TODO:
	 * Host virtual memory, device tree, heap is up.
	 * Do necessary early stuff like iomapping devices
	 * memory or boot time memory reservation here.
	 */
	return 0;
}

int __init arch_board_final_init(void)
{
	int rc;
	struct vmm_devtree_node *node;

	/* All VMM API's are available here */
	/* We can register a Board specific resource here */

	/* Do Probing using device driver framework */
	node = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPARATOR_STRING
				   "sfrregion");
	if (!node) {
		return VMM_ENOTAVAIL;
	}

	/* Initiate device driver probing */
	rc = vmm_devdrv_probe(node);

	/* Dereference the node */
	vmm_devtree_dref_node(node);

	return rc;
}
