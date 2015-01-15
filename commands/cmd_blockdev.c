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
#include <vmm_heap.h>
#include <block/vmm_blockdev.h>
#include <libs/stringlib.h>

#define MODULE_DESC			"Command blockdev"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(VMM_BLOCKDEV_CLASS_IPRIORITY+1)
#define	MODULE_INIT			cmd_blockdev_init
#define	MODULE_EXIT			cmd_blockdev_exit

static void cmd_blockdev_usage(struct vmm_chardev *cdev)
{
	vmm_cprintf(cdev, "Usage:\n");
	vmm_cprintf(cdev, "   blockdev help\n");
	vmm_cprintf(cdev, "   blockdev list\n");
	vmm_cprintf(cdev, "   blockdev info <name>\n");
	vmm_cprintf(cdev, "   blockdev dump8 <name> [length] [offset]\n");
}

static int cmd_blockdev_info(struct vmm_chardev *cdev,
			     struct vmm_blockdev *bdev)
{
	vmm_cprintf(cdev, "Name       : %s\n", bdev->name);
	vmm_cprintf(cdev, "Parent     : %s\n",
				(bdev->parent) ? bdev->parent->name : "---");
	vmm_cprintf(cdev, "Description: %s\n", bdev->desc);
	vmm_cprintf(cdev, "Access     : %s\n",
		(bdev->flags & VMM_BLOCKDEV_RW) ? "Read-Write" : "Read-Only");
	vmm_cprintf(cdev, "Start LBA  : %ll\n", bdev->start_lba);
	vmm_cprintf(cdev, "Block Size : %d\n", bdev->block_size);
	vmm_cprintf(cdev, "Block Count: %ll\n", bdev->num_blocks);

	return VMM_OK;
}

static int cmd_blockdev_list_iter(struct vmm_blockdev *bdev, void *data)
{
	struct vmm_chardev *cdev = data;

	vmm_cprintf(cdev, " %-16s %-16s %-16ll %-11d %-16ll\n",
		    bdev->name,  (bdev->parent) ? bdev->parent->name : "---",
		    bdev->start_lba, bdev->block_size, bdev->num_blocks);

	return VMM_OK;
}

static void cmd_blockdev_list(struct vmm_chardev *cdev)
{
	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");
	vmm_cprintf(cdev, " %-16s %-16s %-16s %-11s %-16s\n",
			  "Name", "Parent", "Start LBA", "Blk Sz", "Blk Cnt");
	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");
	vmm_blockdev_iterate(NULL, cdev, cmd_blockdev_list_iter);
	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");
}

static int cmd_blockdev_dump8(struct vmm_chardev *cdev,
			      struct vmm_blockdev *bdev,
			      int argc, char *argv[])
{
#define BLOCKDEV_DUMP_BUF_SZ	128
	u8 data[BLOCKDEV_DUMP_BUF_SZ];
	u64 off = 0, count = bdev->block_size;
	u64 i, pos, sz, rdsz;

	if (argc >= 1)
		count = strtoul(argv[0], NULL, 10);

	if (!count) {
		vmm_cprintf(cdev, "Error, 0 data to read\n");
		return VMM_EINVALID;
	}

	if (argc >= 2) {
		if (argv[1][0] && (argv[1][1] == 'x'))
			off = strtoull(argv[1], NULL, 16);
		else
			off = strtoull(argv[1], NULL, 10);
	}

	pos = 0;
	while (count) {
		sz = (count < BLOCKDEV_DUMP_BUF_SZ) ?
				count : BLOCKDEV_DUMP_BUF_SZ;

		rdsz = vmm_blockdev_rw(bdev, VMM_REQUEST_READ, data, off, sz);
		if (rdsz != sz) {
			vmm_cprintf(cdev, "Error, read %lld byte(s)\n", rdsz);
			break;
		}

		for (i = 0; i < rdsz; i++) {
			if (pos % 8 == 0)
				vmm_cprintf(cdev, "0x%08x:", i + off);
			vmm_cprintf(cdev, " 0x%02x", data[i]);
			if (pos % 8 == 7)
				vmm_cprintf(cdev, "\n");
			pos++;
		}

		count -= sz;
		off += sz;
	}

	if (pos % 8 != 0)
		vmm_cprintf(cdev, "\n");

	return VMM_OK;
}

static int cmd_blockdev_exec(struct vmm_chardev *cdev, int argc, char **argv)
{
	struct vmm_blockdev *bdev = NULL;

	if (argc == 2) {
		if (strcmp(argv[1], "help") == 0) {
			cmd_blockdev_usage(cdev);
			return VMM_OK;
		} else if (strcmp(argv[1], "list") == 0) {
			cmd_blockdev_list(cdev);
			return VMM_OK;
		}
	} else if (argc >= 3) {
		bdev = vmm_blockdev_find(argv[2]);

		if (!bdev) {
			vmm_cprintf(cdev, "Error: cannot find blockdev %s\n",
				    argv[2]);
			return VMM_EINVALID;
		}

		if (strcmp(argv[1], "info") == 0) {
			return cmd_blockdev_info(cdev, bdev);
		} else if (strcmp(argv[1], "dump8") == 0) {
			return cmd_blockdev_dump8(cdev, bdev,
						 argc - 3, argv + 3);
		}
	}
	cmd_blockdev_usage(cdev);
	return VMM_EFAIL;
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
