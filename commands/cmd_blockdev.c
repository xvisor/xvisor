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
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of blockdev command
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_devtree.h>
#include <vmm_modules.h>
#include <vmm_cmdmgr.h>
#include <stringlib.h>
#include <block/vmm_blockdev.h>

#define MODULE_DESC			"Command blockdev"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(VMM_BLOCKDEV_CLASS_IPRIORITY+1)
#define	MODULE_INIT			cmd_blockdev_init
#define	MODULE_EXIT			cmd_blockdev_exit

void cmd_blockdev_usage(struct vmm_chardev *cdev)
{
	vmm_cprintf(cdev, "Usage:\n");
	vmm_cprintf(cdev, "   blockdev help\n");
	vmm_cprintf(cdev, "   blockdev list\n");
}

void cmd_blockdev_list(struct vmm_chardev *cdev)
{
	int num, count;
	struct vmm_blockdev *bdev;
	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");
	vmm_cprintf(cdev, " %-16s %-16s %-16s %-11s %-16s\n", 
			  "Name", "Parent", "Start LBA", "Blk Sz", "Blk Cnt");
	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");
	count = vmm_blockdev_count();
	for (num = 0; num < count; num++) {
		bdev = vmm_blockdev_get(num);
		vmm_cprintf(cdev, " %-16s %-16s %-16ll %-11d %-16ll\n", 
			    bdev->name, 
			    (bdev->parent) ? bdev->parent->name : "---",
			    bdev->start_lba, 
			    bdev->block_size, 
			    bdev->num_blocks);
	}
	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");

}

int cmd_blockdev_exec(struct vmm_chardev *cdev, int argc, char **argv)
{
	if (argc == 2) {
		if (strcmp(argv[1], "help") == 0) {
			cmd_blockdev_usage(cdev);
			return VMM_OK;
		} else if (strcmp(argv[1], "list") == 0) {
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

static struct vmm_cmd cmd_blockdev = {
	.name = "blockdev",
	.desc = "block device commands",
	.usage = cmd_blockdev_usage,
	.exec = cmd_blockdev_exec,
};

static int __init cmd_blockdev_init(void)
{
	return vmm_cmdmgr_register_cmd(&cmd_blockdev);
}

static void __exit cmd_blockdev_exit(void)
{
	vmm_cmdmgr_unregister_cmd(&cmd_blockdev);
}

VMM_DECLARE_MODULE(MODULE_DESC, 
			MODULE_AUTHOR, 
			MODULE_LICENSE, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
