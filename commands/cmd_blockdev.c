/**
 * Copyright (c) 2010 Anup Patel.
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
 * @file cmd_blockdev.c
 * @version 0.01
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of blockdev command
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_string.h>
#include <vmm_devtree.h>
#include <vmm_blockdev.h>
#include <vmm_modules.h>
#include <vmm_cmdmgr.h>

#define MODULE_VARID			cmd_blockdev_module
#define MODULE_NAME			"Command blockdev"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			cmd_blockdev_init
#define	MODULE_EXIT			cmd_blockdev_exit

void cmd_blockdev_usage(vmm_chardev_t *cdev)
{
	vmm_cprintf(cdev, "Usage:\n");
	vmm_cprintf(cdev, "   blockdev help\n");
	vmm_cprintf(cdev, "   blockdev list\n");
}

void cmd_blockdev_list(vmm_chardev_t *cdev)
{
	int num, count;
	char path[1024];
	vmm_blockdev_t *bdev;
	count = vmm_blockdev_count();
	for (num = 0; num < count; num++) {
		bdev = vmm_blockdev_get(num);
		if (!bdev->dev) {
			vmm_cprintf(cdev, "%s: ---\n", bdev->name);
		} else {
			vmm_devtree_getpath(path, bdev->dev->node);
			vmm_cprintf(cdev, "%s: %s\n", bdev->name, path);
		}
	}
}

int cmd_blockdev_exec(vmm_chardev_t *cdev, int argc, char **argv)
{
	if (argc == 2) {
		if (vmm_strcmp(argv[1], "help") == 0) {
			cmd_blockdev_usage(cdev);
			return VMM_OK;
		} else if (vmm_strcmp(argv[1], "list") == 0) {
			cmd_blockdev_list(cdev);
			return VMM_OK;
		}
	}
	if (argc < 3) {
		cmd_blockdev_usage(cdev);
		return VMM_EFAIL;
	}
	return VMM_OK;
}

static vmm_cmd_t cmd_blockdev = {
	.name = "blockdev",
	.desc = "block device commands",
	.usage = cmd_blockdev_usage,
	.exec = cmd_blockdev_exec,
};

static int __init cmd_blockdev_init(void)
{
	return vmm_cmdmgr_register_cmd(&cmd_blockdev);
}

static void cmd_blockdev_exit(void)
{
	vmm_cmdmgr_unregister_cmd(&cmd_blockdev);
}

VMM_DECLARE_MODULE(MODULE_VARID, 
			MODULE_NAME, 
			MODULE_AUTHOR, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
