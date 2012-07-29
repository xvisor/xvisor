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
 * @file cmd_input.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of input command
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_string.h>
#include <vmm_devtree.h>
#include <vmm_modules.h>
#include <vmm_cmdmgr.h>
#include <input/vmm_input.h>

#define MODULE_VARID			cmd_input_module
#define MODULE_NAME			"Command input"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			cmd_input_init
#define	MODULE_EXIT			cmd_input_exit

void cmd_input_usage(struct vmm_chardev *cdev)
{
	vmm_cprintf(cdev, "Usage:\n");
	vmm_cprintf(cdev, "   input help\n");
	vmm_cprintf(cdev, "   input devices\n");
	vmm_cprintf(cdev, "   input handlers\n");
}

void cmd_input_devices(struct vmm_chardev *cdev)
{
	int num, count;
	struct vmm_input_dev *idev;
	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");
	vmm_cprintf(cdev, " %-18s %-24s %-8s %-8s %-8s %-8s\n", 
			  "Phys", "Name", "BusType", 
			  "Vendor", "Product", "Version");
	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");
	count = vmm_input_count_device();
	for (num = 0; num < count; num++) {
		idev = vmm_input_get_device(num);
		vmm_cprintf(cdev, " %-18s %-24s 0x%-6x 0x%-6x 0x%-6x 0x%-6x\n", 
				  idev->phys, idev->name, 
				  idev->id.bustype, idev->id.vendor, 
				  idev->id.product, idev->id.version);
	}
	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");
}

void cmd_input_handlers(struct vmm_chardev *cdev)
{
	int num, count;
	struct vmm_input_handler *ihnd;
	count = vmm_input_count_handler();
	for (num = 0; num < count; num++) {
		ihnd = vmm_input_get_handler(num);
		vmm_cprintf(cdev, "%d: %s\n", num, ihnd->name);
	}
}

int cmd_input_exec(struct vmm_chardev *cdev, int argc, char **argv)
{
	if (argc == 2) {
		if (vmm_strcmp(argv[1], "help") == 0) {
			cmd_input_usage(cdev);
			return VMM_OK;
		} else if (vmm_strcmp(argv[1], "devices") == 0) {
			cmd_input_devices(cdev);
			return VMM_OK;
		} else if (vmm_strcmp(argv[1], "handlers") == 0) {
			cmd_input_handlers(cdev);
			return VMM_OK;
		}
	}
	cmd_input_usage(cdev);
	return VMM_EFAIL;
}

static struct vmm_cmd cmd_input = {
	.name = "input",
	.desc = "input device commands",
	.usage = cmd_input_usage,
	.exec = cmd_input_exec,
};

static int __init cmd_input_init(void)
{
	return vmm_cmdmgr_register_cmd(&cmd_input);
}

static void cmd_input_exit(void)
{
	vmm_cmdmgr_unregister_cmd(&cmd_input);
}

VMM_DECLARE_MODULE(MODULE_VARID, 
			MODULE_NAME, 
			MODULE_AUTHOR, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
