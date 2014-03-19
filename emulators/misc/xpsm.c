/**
 * Copyright (c) 2014 Himanshu Chauhan
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
 * @file xpsm.c
 * @author Himanshu Chauhan <hschauhan@nulltrace.org>
 * @brief Xvisor PCI Shared Memory Driver.
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <emu/pci/pci_emu_core.h>

#define XPSM_EMU_IPRIORITY		(PCI_EMU_CORE_IPRIORITY + 1)

#define MODULE_DESC			"PCI Shared Memory Device"
#define MODULE_AUTHOR			"Himanshu"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		XPSM_EMU_IPRIORITY
#define	MODULE_INIT			xpsm_emulator_init
#define	MODULE_EXIT			xpsm_emulator_exit

static int xpsm_emulator_reset(struct pci_device *pdev)
{
	return VMM_OK;
}

static int xpsm_emulator_probe(struct pci_device *pdev,
			       struct vmm_guest *guest,
			       const struct vmm_devtree_nodeid *eid)
{
	pdev->priv = NULL;

	return VMM_OK;
}

static int xpsm_emulator_remove(struct pci_device *pdev)
{
	return VMM_OK;
}

static struct vmm_devtree_nodeid xpsm_emuid_table[] = {
	{ .type = "psm", 
	  .compatible = "xpsm", 
	},
	{ /* end of list */ },
};

static struct pci_dev_emulator xpsm_emulator = {
	.name = "xpsm",
	.match_table = xpsm_emuid_table,
	.probe = xpsm_emulator_probe,
	.reset = xpsm_emulator_reset,
	.remove = xpsm_emulator_remove,
};

static int __init xpsm_emulator_init(void)
{
	return pci_emu_register_device(&xpsm_emulator);
}

static void __exit xpsm_emulator_exit(void)
{
	pci_emu_unregister_device(&xpsm_emulator);
}

VMM_DECLARE_MODULE(MODULE_DESC,
		   MODULE_AUTHOR,
		   MODULE_LICENSE,
		   MODULE_IPRIORITY,
		   MODULE_INIT,
		   MODULE_EXIT);
