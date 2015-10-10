/**
 * Copyright (c) 2014 Himanshu Chauhan.
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
 * @file x86-legacy-controller.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief Wrapper for doing x86 legacy init for enumeration.
 */

#include <vmm_error.h>
#include <vmm_modules.h>
#include <linux/pci.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <linux/sched.h>

#define MODULE_DESC			"x86 Legacy PCI Controller"
#define MODULE_AUTHOR			"Himanshu Chauhan"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(PCI_HOST_CONTROLLER_IPRIORITY)
#define	MODULE_INIT			x86_legacy_init
#define	MODULE_EXIT			x86_legacy_exit

extern void pci_arch_init(void);
extern void pci_subsys_init(void);

static int x86_legacy_probe(struct vmm_device *dev,
			    const struct vmm_devtree_nodeid *devid)
{
	pci_subsys_init();

	return VMM_OK;
}

static struct vmm_devtree_nodeid x86_legacy_pci_controller_devid_table[] = {
        { .type = "pci", .compatible = "x86-legacy" },
	{ /* end of list */ },
};

static struct vmm_driver x86_pci_legacy_driver = {
        .name = "x86_pci_legacy_host",
        .match_table = x86_legacy_pci_controller_devid_table,
        .probe = x86_legacy_probe,
};

static int __init x86_legacy_init(void)
{
	pci_arch_init();

        return vmm_devdrv_register_driver(&x86_pci_legacy_driver);
}

static void __exit x86_legacy_exit(void)
{
	vmm_devdrv_unregister_driver(&x86_pci_legacy_driver);
}

VMM_DECLARE_MODULE(MODULE_DESC,
		   MODULE_AUTHOR,
		   MODULE_LICENSE,
		   MODULE_IPRIORITY,
		   MODULE_INIT,
		   MODULE_EXIT);
