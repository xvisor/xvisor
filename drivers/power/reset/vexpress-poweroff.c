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
 * @file vexpress-poweroff.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief ARM Versatile Express power-off/reboot driver
 */

#include <vmm_error.h>
#include <vmm_main.h>
#include <vmm_delay.h>
#include <vmm_compiler.h>
#include <vmm_stdio.h>
#include <vmm_devtree.h>
#include <vmm_devdrv.h>
#include <vmm_modules.h>

#include <linux/vexpress.h>

#define MODULE_DESC		"ARM Versatile Express Power-off/Reboot Driver"
#define MODULE_AUTHOR		"Anup Patel"
#define MODULE_LICENSE		"GPL"
#define MODULE_IPRIORITY	0
#define	MODULE_INIT		vexpress_poweroff_init
#define	MODULE_EXIT		vexpress_poweroff_exit

enum vexpress_reset_func { FUNC_RESET, FUNC_SHUTDOWN, FUNC_REBOOT };

static struct vexpress_config_func *reboot_func;
static struct vexpress_config_func *shutdown_func;

static int vexpress_reset(void)
{
	int err = VMM_EFAIL;

	if (reboot_func) {
		err = vexpress_config_write(reboot_func, 0, 0);
		vmm_mdelay(1000);
	}

	return err;
}

static int vexpress_shutdown(void)
{
	int err = VMM_EFAIL;

	if (shutdown_func) {
		err = vexpress_config_write(shutdown_func, 0, 0);
		vmm_mdelay(1000);
	}

	return err;
}

static int __init vexpress_poweroff_driver_probe(struct vmm_device *dev,
					const struct vmm_devtree_nodeid *devid)
{
	enum vexpress_reset_func func;

	func = (enum vexpress_reset_func)devid->data;
	switch (func) {
	case FUNC_SHUTDOWN:
		shutdown_func = vexpress_config_func_get_by_node(dev->node);
		if (!shutdown_func) {
			return VMM_ENODEV;
		}
		vmm_register_system_shutdown(vexpress_shutdown);
		break;
	case FUNC_RESET:
	case FUNC_REBOOT:
		reboot_func = vexpress_config_func_get_by_node(dev->node);
		if (!reboot_func) {
			return VMM_ENODEV;
		}
		vmm_register_system_reset(vexpress_reset);
		break;
	};

	return VMM_OK;
}

static int __exit vexpress_poweroff_driver_remove(struct vmm_device *dev)
{
	/* Nothing to do here. */
	return VMM_OK;
}

static struct vmm_devtree_nodeid vexpress_poweroff_devid_table[] = {
	{ .compatible = "arm,vexpress-reset", .data = (void *)FUNC_RESET },
	{ .compatible = "arm,vexpress-reboot", .data = (void *)FUNC_REBOOT },
	{ .compatible = "arm,vexpress-shutdown", .data = (void *)FUNC_SHUTDOWN },
	{ /* end of list */ },
};

static struct vmm_driver vexpress_poweroff_driver = {
	.name = "vexpress-poweroff",
	.match_table = vexpress_poweroff_devid_table,
	.probe = vexpress_poweroff_driver_probe,
	.remove = vexpress_poweroff_driver_remove,
};

static int __init vexpress_poweroff_init(void)
{
	return vmm_devdrv_register_driver(&vexpress_poweroff_driver);
}

static void __exit vexpress_poweroff_exit(void)
{
	vmm_devdrv_unregister_driver(&vexpress_poweroff_driver);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
