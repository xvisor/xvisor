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
 * @file cmd_vdisplay.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of vdisplay command
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_devtree.h>
#include <vmm_modules.h>
#include <vmm_cmdmgr.h>
#include <vio/vmm_vdisplay.h>
#include <libs/stringlib.h>

#define MODULE_DESC			"Command vdisplay"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			cmd_vdisplay_init
#define	MODULE_EXIT			cmd_vdisplay_exit

void cmd_vdisplay_usage(struct vmm_chardev *cdev)
{
	vmm_cprintf(cdev, "Usage:\n");
	vmm_cprintf(cdev, "   vdisplay help\n");
	vmm_cprintf(cdev, "   vdisplay list\n");
}

void cmd_vdisplay_list(struct vmm_chardev *cdev)
{
	int num, count;
	struct vmm_vdisplay *vdis;
	vmm_cprintf(cdev, "----------------------------------------\n");
	vmm_cprintf(cdev, " %-39s\n", "Name");
	vmm_cprintf(cdev, "----------------------------------------\n");
	count = vmm_vdisplay_count();
	for (num = 0; num < count; num++) {
		vdis = vmm_vdisplay_get(num);
		vmm_cprintf(cdev, " %-39s\n", vdis->name);
	}
	vmm_cprintf(cdev, "----------------------------------------\n");
}

int cmd_vdisplay_exec(struct vmm_chardev *cdev, int argc, char **argv)
{
	if (argc == 2) {
		if (strcmp(argv[1], "help") == 0) {
			cmd_vdisplay_usage(cdev);
			return VMM_OK;
		} else if (strcmp(argv[1], "list") == 0) {
			cmd_vdisplay_list(cdev);
			return VMM_OK;
		}
	}
	cmd_vdisplay_usage(cdev);
	return VMM_EFAIL;
}

static struct vmm_cmd cmd_vdisplay = {
	.name = "vdisplay",
	.desc = "virtual display commands",
	.usage = cmd_vdisplay_usage,
	.exec = cmd_vdisplay_exec,
};

static int __init cmd_vdisplay_init(void)
{
	return vmm_cmdmgr_register_cmd(&cmd_vdisplay);
}

static void __exit cmd_vdisplay_exit(void)
{
	vmm_cmdmgr_unregister_cmd(&cmd_vdisplay);
}

VMM_DECLARE_MODULE(MODULE_DESC, 
			MODULE_AUTHOR, 
			MODULE_LICENSE, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
