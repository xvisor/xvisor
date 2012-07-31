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
 * @file cmd_stdio.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of stdio command
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_string.h>
#include <vmm_devtree.h>
#include <vmm_chardev.h>
#include <vmm_modules.h>
#include <vmm_cmdmgr.h>

#define MODULE_NAME			"Command stdio"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			cmd_stdio_init
#define	MODULE_EXIT			cmd_stdio_exit

void cmd_stdio_usage(struct vmm_chardev *cdev)
{
	vmm_cprintf(cdev, "Usage:\n");
	vmm_cprintf(cdev, "   stdio help\n");
	vmm_cprintf(cdev, "   stdio curdev\n");
	vmm_cprintf(cdev, "   stdio chdev <chardev_name>\n");
}

int cmd_stdio_curdev(struct vmm_chardev *cdev)
{
	struct vmm_chardev *cd;
	cd = vmm_stdio_device();
	if (!cd) {
		vmm_cprintf(cdev, "Current Device : ---\n");
	} else {
		vmm_cprintf(cdev, "Current Device : %s\n", cd->name);
	}
	return VMM_OK;
}

int cmd_stdio_chdev(struct vmm_chardev *cdev, char *chardev_name)
{
	int ret;
	struct vmm_chardev *cd = vmm_chardev_find(chardev_name);
	if (cd) {
		vmm_cprintf(cdev, 
			    "New I/O Device: %s\n", 
			    cd->name);
		if ((ret = vmm_stdio_change_device(cd))) {
			vmm_cprintf(cdev, 
				    "Failed to change device %s\n",
				    cd->name);
			return ret;
		}
	} else {
		vmm_cprintf(cdev, "Device %s not found\n", chardev_name);
		return VMM_EFAIL;
	}
	return VMM_OK;
}

int cmd_stdio_exec(struct vmm_chardev *cdev, int argc, char **argv)
{
	if (argc == 2) {
		if (vmm_strcmp(argv[1], "help") == 0) {
			cmd_stdio_usage(cdev);
			return VMM_OK;
		} else if (vmm_strcmp(argv[1], "curdev") == 0) {
			return cmd_stdio_curdev(cdev);
		}
	}
	if (argc < 3) {
		cmd_stdio_usage(cdev);
		return VMM_EFAIL;
	}
	if (vmm_strcmp(argv[1], "chdev") == 0) {
		return cmd_stdio_chdev(cdev, argv[2]);
	} else {
		cmd_stdio_usage(cdev);
		return VMM_EFAIL;
	}
	return VMM_OK;
}

static struct vmm_cmd cmd_stdio = {
	.name = "stdio",
	.desc = "standard I/O configuration",
	.usage = cmd_stdio_usage,
	.exec = cmd_stdio_exec,
};

static int __init cmd_stdio_init(void)
{
	return vmm_cmdmgr_register_cmd(&cmd_stdio);
}

static void __exit cmd_stdio_exit(void)
{
	vmm_cmdmgr_unregister_cmd(&cmd_stdio);
}

VMM_DECLARE_MODULE(MODULE_NAME, 
			MODULE_AUTHOR, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
