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
 * @file vmm_cmd_chardev.c
 * @version 0.01
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of chardev command
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_string.h>
#include <vmm_devtree.h>
#include <vmm_chardev.h>
#include <vmm_mterm.h>

void cmd_chardev_usage(void)
{
	vmm_printf("Usage:\n");
	vmm_printf("   chardev help\n");
	vmm_printf("   chardev list\n");
}

void cmd_chardev_list()
{
	int num, count;
	char path[1024];
	vmm_chardev_t *cdev;
	count = vmm_chardev_count();
	for (num = 0; num < count; num++) {
		cdev = vmm_chardev_get(num);
		if (!cdev->dev) {
			vmm_printf("%s: ---\n", cdev->name);
		} else {
			vmm_devtree_getpath(path, cdev->dev->node);
			vmm_printf("%s: %s\n", cdev->name, path);
		}
	}
}

int cmd_chardev_exec(int argc, char **argv)
{
	if (argc == 2) {
		if (vmm_strcmp(argv[1], "help") == 0) {
			cmd_chardev_usage();
			return VMM_OK;
		} else if (vmm_strcmp(argv[1], "list") == 0) {
			cmd_chardev_list();
			return VMM_OK;
		}
	}
	if (argc < 3) {
		cmd_chardev_usage();
		return VMM_EFAIL;
	}
	return VMM_OK;
}

VMM_DECLARE_CMD(chardev, "character device commands", cmd_chardev_exec, NULL);
