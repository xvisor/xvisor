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
 * @file cmd_rbd.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of rbd command
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_devtree.h>
#include <vmm_modules.h>
#include <vmm_cmdmgr.h>
#include <libs/stringlib.h>
#include <drv/rbd.h>

#define MODULE_DESC			"Command rbd"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			cmd_rbd_init
#define	MODULE_EXIT			cmd_rbd_exit

static void cmd_rbd_usage(struct vmm_chardev *cdev)
{
	vmm_cprintf(cdev, "Usage:\n");
	vmm_cprintf(cdev, "   rbd help\n");
	vmm_cprintf(cdev, "   rbd list\n");
	vmm_cprintf(cdev, "   rbd create <name> <phys_addr> <phys_size>\n");
	vmm_cprintf(cdev, "   rbd destroy <name>\n");
}

static int cmd_rbd_list(struct vmm_chardev *cdev)
{
	int num, count;
	char addr[32], size[32];
	struct rbd *d;

	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");
	vmm_cprintf(cdev, " %-32s %-22s %-22s\n", 
			  "Name", "Physical Address", "Physical Size");
	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");
	count = rbd_count();
	for (num = 0; num < count; num++) {
		d = rbd_get(num);
		vmm_snprintf(addr, sizeof(addr), "0x%"PRIPADDR, d->addr);
		vmm_snprintf(size, sizeof(size), "0x%"PRIPADDR, d->size);
		vmm_cprintf(cdev, " %-32s %-22s %-22s\n",
			    d->bdev->name, addr, size);
	}
	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");

	return VMM_OK;
}

static int cmd_rbd_create(struct vmm_chardev *cdev, const char *name,
			  physical_addr_t addr, physical_size_t size)
{
	struct rbd *d;

	d = rbd_create(name, addr, size, false);
	if (!d) {
		vmm_cprintf(cdev, "Failed to create %s RBD instance\n", name);
		return VMM_EFAIL;
	}

	vmm_cprintf(cdev, "Created %s RBD instance\n", name);

	return VMM_OK;
}

static int cmd_rbd_destroy(struct vmm_chardev *cdev, const char *name)
{
	struct rbd *d = rbd_find(name);

	if (!d) {
		vmm_cprintf(cdev, "Failed to find %s RBD instance\n", name);
		return VMM_ENOTAVAIL;
	}

	rbd_destroy(d);

	vmm_cprintf(cdev, "Destroyed %s RBD instance\n", name);

	return VMM_OK;
}

static int cmd_rbd_exec(struct vmm_chardev *cdev, int argc, char **argv)
{
	physical_addr_t addr;
	physical_size_t size;

	if (argc <= 1) {
		goto fail;
	}

	if (strcmp(argv[1], "help") == 0) {
		cmd_rbd_usage(cdev);
		return VMM_OK;
	} else if ((strcmp(argv[1], "list") == 0) && (argc == 2)) {
		return cmd_rbd_list(cdev);
	} else if ((strcmp(argv[1], "create") == 0) && (argc == 5)) {
		addr = (physical_addr_t)strtoull(argv[3], NULL, 0);
		size = (physical_size_t)strtoull(argv[4], NULL, 0);
		return cmd_rbd_create(cdev, argv[2], addr, size);
	} else if ((strcmp(argv[1], "destroy") == 0) && (argc == 3)) {
		return cmd_rbd_destroy(cdev, argv[2]);
	}

fail:
	cmd_rbd_usage(cdev);
	return VMM_EFAIL;
}

static struct vmm_cmd cmd_rbd = {
	.name = "rbd",
	.desc = "ram backed block device commands",
	.usage = cmd_rbd_usage,
	.exec = cmd_rbd_exec,
};

static int __init cmd_rbd_init(void)
{
	return vmm_cmdmgr_register_cmd(&cmd_rbd);
}

static void __exit cmd_rbd_exit(void)
{
	vmm_cmdmgr_unregister_cmd(&cmd_rbd);
}

VMM_DECLARE_MODULE(MODULE_DESC, 
			MODULE_AUTHOR, 
			MODULE_LICENSE, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
