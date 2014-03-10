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
 * @file realview-reboot.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief ARM Realview reboot driver
 */

#include <vmm_error.h>
#include <vmm_main.h>
#include <vmm_compiler.h>
#include <vmm_stdio.h>
#include <vmm_devtree.h>
#include <vmm_devdrv.h>
#include <vmm_modules.h>
#include <drv/realview.h>

#define MODULE_DESC			"ARM Realview Reboot Driver"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			realview_reboot_init
#define	MODULE_EXIT			realview_reboot_exit

static int realview_reset(void)
{
	return realview_system_reset();
}

static int __init realview_reboot_driver_probe(struct vmm_device *dev,
					const struct vmm_devtree_nodeid *devid)
{
	/* Register reset & shutdown callbacks */
	vmm_register_system_reset(realview_reset);

	return VMM_OK;
}

static int __exit realview_reboot_driver_remove(struct vmm_device *dev)
{
	/* Nothing to do here. */
	return VMM_OK;
}

static struct vmm_devtree_nodeid realview_reboot_devid_table[] = {
	{ .compatible = "arm,realview-reboot" },
	{ /* end of list */ },
};

static struct vmm_driver realview_reboot_driver = {
	.name = "realview-reboot",
	.match_table = realview_reboot_devid_table,
	.probe = realview_reboot_driver_probe,
	.remove = realview_reboot_driver_remove,
};

static int __init realview_reboot_init(void)
{
	return vmm_devdrv_register_driver(&realview_reboot_driver);
}

static void __exit realview_reboot_exit(void)
{
	vmm_devdrv_unregister_driver(&realview_reboot_driver);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
