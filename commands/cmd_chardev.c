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
 * @file cmd_chardev.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of chardev command
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_devtree.h>
#include <vmm_chardev.h>
#include <vmm_modules.h>
#include <vmm_cmdmgr.h>
#include <libs/stringlib.h>

#define MODULE_DESC			"Command chardev"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			cmd_chardev_init
#define	MODULE_EXIT			cmd_chardev_exit

static void cmd_chardev_usage(struct vmm_chardev *cdev)
{
	vmm_cprintf(cdev, "Usage:\n");
	vmm_cprintf(cdev, "   chardev help\n");
	vmm_cprintf(cdev, "   chardev list\n");
}

static int cmd_chardev_list_iter(struct vmm_chardev *cd, void *data)
{
	int rc;
	char path[256];
	struct vmm_chardev *cdev = data;

	if (cd->dev.parent && cd->dev.parent->of_node) {
		rc = vmm_devtree_getpath(path, sizeof(path),
					 cd->dev.parent->of_node);
		if (rc) {
			vmm_snprintf(path, sizeof(path),
				     "----- (error %d)", rc);
		}
	} else {
		strcpy(path, "-----");
	}
	vmm_cprintf(cdev, " %-24s %-53s\n", cd->name, path);

	return VMM_OK;
}

static void cmd_chardev_list(struct vmm_chardev *cdev)
{
	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");
	vmm_cprintf(cdev, " %-24s %-53s\n", 
			  "Name", "Device Path");
	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");
	vmm_chardev_iterate(NULL, cdev, cmd_chardev_list_iter);
	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");
}

static int cmd_chardev_exec(struct vmm_chardev *cdev, int argc, char **argv)
{
	if (argc == 2) {
		if (strcmp(argv[1], "help") == 0) {
			cmd_chardev_usage(cdev);
			return VMM_OK;
		} else if (strcmp(argv[1], "list") == 0) {
			cmd_chardev_list(cdev);
			return VMM_OK;
		}
	}
	if (argc < 3) {
		cmd_chardev_usage(cdev);
		return VMM_EFAIL;
	}
	return VMM_OK;
}

static struct vmm_cmd cmd_chardev = {
	.name = "chardev",
	.desc = "character device commands",
	.usage = cmd_chardev_usage,
	.exec = cmd_chardev_exec,
};

static int __init cmd_chardev_init(void)
{
	return vmm_cmdmgr_register_cmd(&cmd_chardev);
}

static void __exit cmd_chardev_exit(void)
{
	vmm_cmdmgr_unregister_cmd(&cmd_chardev);
}

VMM_DECLARE_MODULE(MODULE_DESC, 
			MODULE_AUTHOR, 
			MODULE_LICENSE, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
