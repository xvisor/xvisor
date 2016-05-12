/**
 * Copyright (c) 2015 Anup Patel.
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
 * @file cmd_vdisk.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of vdisk command
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_devtree.h>
#include <vmm_modules.h>
#include <vmm_cmdmgr.h>
#include <vio/vmm_vdisk.h>
#include <libs/stringlib.h>

#define MODULE_DESC			"Command vdisk"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			cmd_vdisk_init
#define	MODULE_EXIT			cmd_vdisk_exit

static void cmd_vdisk_usage(struct vmm_chardev *cdev)
{
	vmm_cprintf(cdev, "Usage:\n");
	vmm_cprintf(cdev, "   vdisk help\n");
	vmm_cprintf(cdev, "   vdisk list\n");
	vmm_cprintf(cdev, "   vdisk info <vdisk_name>\n");
	vmm_cprintf(cdev, "   vdisk detach <vdisk_name>\n");
	vmm_cprintf(cdev, "   vdisk attach <vdisk_name> <block_device_name>\n");
}

static int cmd_vdisk_list_iter(struct vmm_vdisk *vdisk, void *data)
{
	int rc;
	char bname[VMM_FIELD_NAME_SIZE];
	struct vmm_chardev *cdev = data;

	rc = vmm_vdisk_current_block_device(vdisk, bname, sizeof(bname));
	vmm_cprintf(cdev, " %-30s %-17d %-30s\n",
		    vmm_vdisk_name(vdisk),  vmm_vdisk_block_size(vdisk),
		    (rc == VMM_OK) ? bname : "---");

	return VMM_OK;
}

static void cmd_vdisk_list(struct vmm_chardev *cdev)
{
	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");
	vmm_cprintf(cdev, " %-30s %-17s %-30s\n",
			  "Name", "Block Size", "Attached Block Device");
	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");
	vmm_vdisk_iterate(NULL, cdev, cmd_vdisk_list_iter);
	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");
}

static int cmd_vdisk_info(struct vmm_chardev *cdev,
			  const char *vdisk_name)
{
	struct vmm_vdisk *vdisk = vmm_vdisk_find(vdisk_name);
	if (!vdisk) {
		vmm_cprintf(cdev, "Failed to find virtual disk\n");
		return VMM_ENODEV;
	}

	vmm_cprintf(cdev,
		"Name        : %s\n"
		"Block Size  : %"PRIu32"\n"
		"Block Factor: %"PRIu32"\n"
		"Capacity    : %"PRIu64"\n"
		"Block Device: %s\n",
		vmm_vdisk_name(vdisk), vmm_vdisk_block_size(vdisk),
		vdisk->blk_factor, vmm_vdisk_capacity(vdisk),
		vdisk->blk ? vdisk->blk->name : "NONE");

	return VMM_OK;
}

static int cmd_vdisk_detach(struct vmm_chardev *cdev,
			    const char *vdisk_name)
{
	struct vmm_vdisk *vdisk = vmm_vdisk_find(vdisk_name);

	if (!vdisk) {
		vmm_cprintf(cdev, "Failed to find virtual disk\n");
		return VMM_ENODEV;
	}

	vmm_vdisk_detach_block_device(vdisk);

	return VMM_OK;
}

static int cmd_vdisk_attach(struct vmm_chardev *cdev,
			    const char *vdisk_name,
			    const char *bdev_name)
{
	struct vmm_vdisk *vdisk = vmm_vdisk_find(vdisk_name);

	if (!vdisk) {
		vmm_cprintf(cdev, "Failed to find virtual disk\n");
		return VMM_ENODEV;
	}

	vmm_vdisk_attach_block_device(vdisk, bdev_name);

	return VMM_OK;
}

static int cmd_vdisk_exec(struct vmm_chardev *cdev, int argc, char **argv)
{
	if (argc == 2) {
		if (strcmp(argv[1], "help") == 0) {
			cmd_vdisk_usage(cdev);
			return VMM_OK;
		} else if (strcmp(argv[1], "list") == 0) {
			cmd_vdisk_list(cdev);
			return VMM_OK;
		}
	} else if (argc == 3) {
		if (strcmp(argv[1], "detach") == 0) {
			return cmd_vdisk_detach(cdev, argv[2]);
		} else if (strcmp(argv[1], "info") == 0) {
			return cmd_vdisk_info(cdev, argv[2]);
		}
	} else if (argc == 4) {
		if (strcmp(argv[1], "attach") == 0) {
			return cmd_vdisk_attach(cdev, argv[2], argv[3]);
		}
	}
	cmd_vdisk_usage(cdev);
	return VMM_EFAIL;
}

static struct vmm_cmd cmd_vdisk = {
	.name = "vdisk",
	.desc = "virtual disk commands",
	.usage = cmd_vdisk_usage,
	.exec = cmd_vdisk_exec,
};

static int __init cmd_vdisk_init(void)
{
	return vmm_cmdmgr_register_cmd(&cmd_vdisk);
}

static void __exit cmd_vdisk_exit(void)
{
	vmm_cmdmgr_unregister_cmd(&cmd_vdisk);
}

VMM_DECLARE_MODULE(MODULE_DESC, 
			MODULE_AUTHOR, 
			MODULE_LICENSE, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
