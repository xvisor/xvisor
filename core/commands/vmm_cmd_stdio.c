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
 * @file vmm_cmd_stdio.c
 * @version 0.01
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of stdio command
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_string.h>
#include <vmm_devtree.h>
#include <vmm_chardev.h>
#include <vmm_mterm.h>

void cmd_stdio_usage(void)
{
	vmm_printf("Usage:\n");
	vmm_printf("   stdio help\n");
	vmm_printf("   stdio curdev\n");
	vmm_printf("   stdio chdev <chardev_name>\n");
}

void cmd_stdio_curdev(void)
{
	vmm_chardev_t *cdev = vmm_stdio_device();
	if (!cdev) {
		vmm_printf("Device: ---\n");
	} else {
		vmm_printf("Device: %s\n", cdev->name);
	}
}

void cmd_stdio_chdev(char *chardev_name)
{
	int ret;
	vmm_chardev_t *cdev = vmm_chardev_find(chardev_name);
	if (cdev) {
		vmm_printf("New device: %s\n", cdev->name);
		ret = vmm_stdio_change_device(cdev);
		if (ret) {
			vmm_printf("Failed to change device %s\n", cdev->name);
		}
	} else {
		vmm_printf("Device %s not found\n", chardev_name);
	}
}

int cmd_stdio_exec(int argc, char **argv)
{
	if (argc == 2) {
		if (vmm_strcmp(argv[1], "help") == 0) {
			cmd_stdio_usage();
			return VMM_OK;
		} else if (vmm_strcmp(argv[1], "curdev") == 0) {
			cmd_stdio_curdev();
			return VMM_OK;
		}
	}
	if (argc < 3) {
		cmd_stdio_usage();
		return VMM_EFAIL;
	}
	if (vmm_strcmp(argv[1], "chdev") == 0) {
		cmd_stdio_chdev(argv[2]);
	} else {
		cmd_stdio_usage();
		return VMM_EFAIL;
	}
	return VMM_OK;
}

VMM_DECLARE_CMD(stdio, "standered input/output configuration", cmd_stdio_exec,
		NULL);
