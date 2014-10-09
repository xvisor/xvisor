/**
 * Copyright (c) 2014 Anup Patel.
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
 * @file sun4i-reboot.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Allwinner Sun4i reboot driver
 */

#include <vmm_error.h>
#include <vmm_main.h>
#include <vmm_compiler.h>
#include <vmm_stdio.h>
#include <vmm_host_io.h>
#include <vmm_devtree.h>
#include <vmm_devdrv.h>
#include <vmm_modules.h>

#define MODULE_DESC			"Allwinner Sun4i Reboot Driver"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			sun4i_reboot_init
#define	MODULE_EXIT			sun4i_reboot_exit

/* AW watchdog registers offsets */
#define AW_WDT_REG_CTRL			(0x0000)
#define AW_WDT_REG_MODE			(0x0004)

/* AW watchdog modes */
#define WDT_MODE_ENABLE			(1 << 0)
#define WDT_MODE_RESET			(1 << 1)

static virtual_addr_t aw_base = 0;

static int aw_timer_force_reset(void)
{
	u32 mode;

	if (!aw_base) {
		return VMM_EFAIL;
	}

	/* Clear & disable watchdog */
	vmm_writel(0, (void *)(aw_base + AW_WDT_REG_MODE));

	/* Force reset by configuring watchdog with minimum interval */
	mode = WDT_MODE_RESET | WDT_MODE_ENABLE;
	vmm_writel(mode, (void *)(aw_base + AW_WDT_REG_MODE));

	/* FIXME: Wait for watchdog to expire ??? */

	return VMM_OK;
}

static int __init sun4i_reboot_driver_probe(struct vmm_device *dev,
					const struct vmm_devtree_nodeid *devid)
{
	int rc;

	/* Map timer registers */
	rc = vmm_devtree_regmap(dev->node, &aw_base, 0);
	if (rc) {
		return rc;
	}

	/* Register reset callbacks */
	vmm_register_system_reset(aw_timer_force_reset);

	return VMM_OK;
}

static int __exit sun4i_reboot_driver_remove(struct vmm_device *dev)
{
	int rc;

	/* Unmap registers */
	rc = vmm_devtree_regunmap(dev->node, aw_base, 0);
	if (rc) {
		return rc;
	}

	/* Clear the base va */
	aw_base = 0;

	return VMM_OK;
}

static struct vmm_devtree_nodeid sun4i_reboot_devid_table[] = {
	{ .compatible = "allwinner,sun4i-reboot" },
	{ /* end of list */ },
};

static struct vmm_driver sun4i_reboot_driver = {
	.name = "sun4i-reboot",
	.match_table = sun4i_reboot_devid_table,
	.probe = sun4i_reboot_driver_probe,
	.remove = sun4i_reboot_driver_remove,
};

static int __init sun4i_reboot_init(void)
{
	return vmm_devdrv_register_driver(&sun4i_reboot_driver);
}

static void __exit sun4i_reboot_exit(void)
{
	vmm_devdrv_unregister_driver(&sun4i_reboot_driver);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
