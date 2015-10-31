/**
 * Copyright (c) 2012 Anup Patel.
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
 * @file cmd_module.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of module command
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_devtree.h>
#include <vmm_modules.h>
#include <vmm_host_aspace.h>
#include <vmm_cmdmgr.h>
#include <libs/stringlib.h>

#define MODULE_DESC			"Command module"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			cmd_module_init
#define	MODULE_EXIT			cmd_module_exit

static void cmd_module_usage(struct vmm_chardev *cdev)
{
	vmm_cprintf(cdev, "Usage:\n");
	vmm_cprintf(cdev, "   module help\n");
	vmm_cprintf(cdev, "   module list\n");
	vmm_cprintf(cdev, "   module info <index>\n");
	vmm_cprintf(cdev, "   module load <phys_addr> <phys_size> (EXPERIMENTAL)\n");
	vmm_cprintf(cdev, "   module unload <index>\n");
}

static void cmd_module_list(struct vmm_chardev *cdev)
{
	int num, count;
	struct vmm_module *mod;
	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");
	vmm_cprintf(cdev, " %-5s %-25s %-25s %-10s %-11s\n",
			  "Num", "Name", "Author", "License", "Type");
	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");
	count = vmm_modules_count();
	for (num = 0; num < count; num++) {
		mod = vmm_modules_getmodule(num);
		vmm_cprintf(cdev, " %-5d %-25s %-25s %-10s %-11s\n",
				  num, mod->name, mod->author, mod->license,
				  vmm_modules_isbuiltin(mod) ?
				  "built-in" : "loadable");
	}
	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");
	vmm_cprintf(cdev, "Total %d modules\n", count);
}

static int cmd_module_info(struct vmm_chardev *cdev, u32 index)
{
	struct vmm_module *mod;

	mod = vmm_modules_getmodule(index);
	if (!mod) {
		return VMM_EFAIL;
	}

	vmm_cprintf(cdev, "Name:        %s\n", mod->name);
	vmm_cprintf(cdev, "Description: %s\n", mod->desc);
	vmm_cprintf(cdev, "Author:      %s\n", mod->author);
	vmm_cprintf(cdev, "License:     %s\n", mod->license);
	vmm_cprintf(cdev, "iPriority:   %d\n", mod->ipriority);
	vmm_cprintf(cdev, "Type:        %s\n", vmm_modules_isbuiltin(mod) ?
					 "built-in" : "loadable");

	return VMM_OK;
}

static int cmd_module_load(struct vmm_chardev *cdev,
			   physical_addr_t phys_addr,
			   physical_size_t phys_size)
{
	int rc;
	virtual_addr_t mod_va;
	virtual_size_t mod_sz = phys_size;

	mod_va = vmm_host_iomap(phys_addr, mod_sz);

	if ((rc = vmm_modules_load(mod_va, mod_sz))) {
		vmm_host_iounmap(mod_va);
		return rc;
	} else {
		vmm_cprintf(cdev, "Loaded module succesfully\n");
	}

	rc = vmm_host_iounmap(mod_va);
	if (rc) {
		vmm_cprintf(cdev, "Error: Failed to unmap memory.\n");
		return rc;
	}

	return VMM_OK;
}

static int cmd_module_unload(struct vmm_chardev *cdev, u32 index)
{
	int rc = VMM_OK;
	struct vmm_module *mod;

	mod = vmm_modules_getmodule(index);
	if (!mod) {
		return VMM_EFAIL;
	}

	if (vmm_modules_isbuiltin(mod)) {
		vmm_cprintf(cdev, "Can't unload built-in module\n");
		return VMM_EFAIL;
	}

	if ((rc = vmm_modules_unload(mod))) {
		vmm_cprintf(cdev, "Failed to unload module (error %d)\n", rc);
	} else {
		vmm_cprintf(cdev, "Unloaded module succesfully\n");
	}

	return rc;
}

static int cmd_module_exec(struct vmm_chardev *cdev, int argc, char **argv)
{
	int index;
	physical_addr_t addr;
	physical_size_t size;

	if (argc == 2) {
		if (strcmp(argv[1], "help") == 0) {
			cmd_module_usage(cdev);
			return VMM_OK;
		} else if (strcmp(argv[1], "list") == 0) {
			cmd_module_list(cdev);
			return VMM_OK;
		}
	}
	if (argc < 3) {
		cmd_module_usage(cdev);
		return VMM_EFAIL;
	}
	if (strcmp(argv[1], "info") == 0) {
		index = atoi(argv[2]);
		return cmd_module_info(cdev, index);
	} else if (strcmp(argv[1], "load") == 0 && argc == 4) {
		addr = (physical_addr_t)strtoull(argv[2], NULL, 0);
		size = (physical_size_t)strtoull(argv[3], NULL, 0);
		return cmd_module_load(cdev, addr, size);
	} else if (strcmp(argv[1], "unload") == 0) {
		index = atoi(argv[2]);
		return cmd_module_unload(cdev, index);
	} else {
		cmd_module_usage(cdev);
		return VMM_EFAIL;
	}
	return VMM_OK;
}

static struct vmm_cmd cmd_module = {
	.name = "module",
	.desc = "module related commands",
	.usage = cmd_module_usage,
	.exec = cmd_module_exec,
};

static int __init cmd_module_init(void)
{
	return vmm_cmdmgr_register_cmd(&cmd_module);
}

static void __exit cmd_module_exit(void)
{
	vmm_cmdmgr_unregister_cmd(&cmd_module);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
