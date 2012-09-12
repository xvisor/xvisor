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
 * @file cmd_shutdown.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of shutdown command
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_main.h>
#include <vmm_modules.h>
#include <vmm_cmdmgr.h>

#define MODULE_DESC			"Command shutdown"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			cmd_shutdown_init
#define	MODULE_EXIT			cmd_shutdown_exit

void cmd_shutdown_usage(struct vmm_chardev *cdev)
{
	vmm_cprintf(cdev, "Usage: ");
	vmm_cprintf(cdev, "   shutdown\n");
}

int cmd_shutdown_exec(struct vmm_chardev *cdev, int argc, char **argv)
{
	/* Shutdown the hypervisor */
	vmm_shutdown();
	return VMM_OK;
}

static struct vmm_cmd cmd_shutdown = {
	.name = "shutdown",
	.desc = "shutdown hypervisor",
	.usage = cmd_shutdown_usage,
	.exec = cmd_shutdown_exec,
};

static int __init cmd_shutdown_init(void)
{
	return vmm_cmdmgr_register_cmd(&cmd_shutdown);
}

static void __exit cmd_shutdown_exit(void)
{
	vmm_cmdmgr_unregister_cmd(&cmd_shutdown);
}

VMM_DECLARE_MODULE(MODULE_DESC, 
			MODULE_AUTHOR, 
			MODULE_LICENSE, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
