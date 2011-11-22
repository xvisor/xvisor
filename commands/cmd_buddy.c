/**
 * Copyright (c) 2010 Himanshu Chauhan.
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
 * @file cmd_buddy.c
 * @version 0.01
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief Implementation of buddy allocator current usage.
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_version.h>
#include <vmm_string.h>
#include <mm/vmm_buddy.h>
#include <vmm_modules.h>
#include <vmm_cmdmgr.h>

#define MODULE_VARID			cmd_buddy_module
#define MODULE_NAME			"Command buddy"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			cmd_buddy_init
#define	MODULE_EXIT			cmd_buddy_exit

void cmd_buddy_usage(vmm_chardev_t *cdev)
{
	vmm_cprintf(cdev, "Usage: \n");
	vmm_cprintf(cdev, "    - buddy state\n");
	vmm_cprintf(cdev, "        Show current allocation state.\n");
	vmm_cprintf(cdev, "    - buddy hk-state\n");
	vmm_cprintf(cdev, "        Show current house keeping state.\n");
}

int cmd_buddy_exec(vmm_chardev_t *cdev, int argc, char **argv)
{
	if (argc <= 1) {
		cmd_buddy_usage(cdev);
		return -1;
	}

	if (!vmm_strcmp(argv[1], "state")) {
		buddy_print_state(cdev);
	} else if (!vmm_strcmp(argv[1], "hk-state")) {
		buddy_print_hk_state(cdev);
	} else {
		vmm_cprintf(cdev, "buddy %s: Unknown command.\n");
		cmd_buddy_usage(cdev);
		return -1;
	}

	return VMM_OK;
}

static vmm_cmd_t cmd_buddy = {
	.name = "buddy",
	.desc = "show current buddy heap state.",
	.usage = cmd_buddy_usage,
	.exec = cmd_buddy_exec,
};

static int __init_section cmd_buddy_init(void)
{
	return vmm_cmdmgr_register_cmd(&cmd_buddy);
}

static void cmd_buddy_exit(void)
{
	vmm_cmdmgr_unregister_cmd(&cmd_buddy);
}

VMM_DECLARE_MODULE(MODULE_VARID, 
			MODULE_NAME, 
			MODULE_AUTHOR, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
