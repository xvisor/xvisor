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
 * @file cmd_version.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of version command
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_version.h>
#include <vmm_modules.h>
#include <vmm_cmdmgr.h>

#define MODULE_NAME			"Command version"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			cmd_version_init
#define	MODULE_EXIT			cmd_version_exit

void cmd_version_usage(struct vmm_chardev *cdev)
{
	vmm_cprintf(cdev, "Usage: ");
	vmm_cprintf(cdev, "   version\n");
}

int cmd_version_exec(struct vmm_chardev *cdev, int argc, char **argv)
{
	vmm_cprintf(cdev, "%s v%d.%d.%d (%s %s)\n", VMM_NAME, 
		    VMM_VERSION_MAJOR, VMM_VERSION_MINOR, VMM_VERSION_RELEASE,
		    __DATE__, __TIME__);
	return VMM_OK;
}

static struct vmm_cmd cmd_version = {
	.name = "version",
	.desc = "show version of hypervisor",
	.usage = cmd_version_usage,
	.exec = cmd_version_exec,
};

static int __init cmd_version_init(void)
{
	return vmm_cmdmgr_register_cmd(&cmd_version);
}

static void __exit cmd_version_exit(void)
{
	vmm_cmdmgr_unregister_cmd(&cmd_version);
}

VMM_DECLARE_MODULE(MODULE_NAME, 
			MODULE_AUTHOR, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
