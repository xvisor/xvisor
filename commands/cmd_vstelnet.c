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
 * @file cmd_vstelnet.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of vstelnet command
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_modules.h>
#include <vmm_cmdmgr.h>
#include <libs/stringlib.h>
#include <libs/vstelnet.h>

#define MODULE_DESC			"Command vstelnet"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			cmd_vstelnet_init
#define	MODULE_EXIT			cmd_vstelnet_exit

static void cmd_vstelnet_usage(struct vmm_chardev *cdev)
{
	vmm_cprintf(cdev, "Usage:\n");
	vmm_cprintf(cdev, "   vstelnet help\n");
	vmm_cprintf(cdev, "   vstelnet list\n");
	vmm_cprintf(cdev, "   vstelnet create  <port_num> <vserial_name>\n");
	vmm_cprintf(cdev, "   vstelnet destroy <port_num>\n");
}

static void cmd_vstelnet_list(struct vmm_chardev *cdev)
{
	int i, count;
	struct vstelnet *vst;

	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");
	vmm_cprintf(cdev, " %-9s %-69s\n", 
			 "Port", "Vserial Name");
	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");

	count = vstelnet_count();
	for (i = 0; i < count; i++) {
		if (!(vst = vstelnet_get(i))) {
			continue;
		}
		vmm_cprintf(cdev, " %-9d %-69s\n", vst->port, vst->vser->name);
	}

	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");
}

static int cmd_vstelnet_create(struct vmm_chardev *cdev, 
				u32 port, const char *vser)
{
	struct vstelnet *vst = NULL;

	vst = vstelnet_create(port, vser);
	if (!vst) {
		vmm_cprintf(cdev, "Error: failed to create "
				  "vstelnet for %s\n", vser);
		return VMM_EFAIL;
	}

	vmm_cprintf(cdev, "Created vstelnet for %s @ %d\n", vser, port);

	return VMM_OK;
}

static int cmd_vstelnet_destroy(struct vmm_chardev *cdev, u32 port)
{
	int ret = VMM_EFAIL;
	struct vstelnet *vst = vstelnet_find(port);

	if (vst) {
		if ((ret = vstelnet_destroy(vst))) {
			vmm_cprintf(cdev, "Failed to destroy vstelnet "
					  "at port %d\n", port);
		} else {
			vmm_cprintf(cdev, "Destroyed vstelnet at port %d\n", port);
		}
	} else {
		vmm_cprintf(cdev, "Failed to find vstelnet at port %d\n", port);
	}

	return ret;
}

static int cmd_vstelnet_exec(struct vmm_chardev *cdev, int argc, char **argv)
{
	u32 port;
	if (argc == 2) {
		if (strcmp(argv[1], "help") == 0) {
			cmd_vstelnet_usage(cdev);
			return VMM_OK;
		} else if (strcmp(argv[1], "list") == 0) {
			cmd_vstelnet_list(cdev);
			return VMM_OK;
		}
	}
	if ((strcmp(argv[1], "create") == 0) && (argc == 4)) {
		port = strtoul(argv[2], NULL, 0);
		return cmd_vstelnet_create(cdev, port, argv[3]);
	} else if ((strcmp(argv[1], "destroy") == 0) && (argc == 3)) {
		port = strtoul(argv[2], NULL, 0);
		return cmd_vstelnet_destroy(cdev, port);
	}
	cmd_vstelnet_usage(cdev);
	return VMM_EFAIL;
}

static struct vmm_cmd cmd_vstelnet = {
	.name = "vstelnet",
	.desc = "commands for vserial telnet access",
	.usage = cmd_vstelnet_usage,
	.exec = cmd_vstelnet_exec,
};

static int __init cmd_vstelnet_init(void)
{
	return vmm_cmdmgr_register_cmd(&cmd_vstelnet);
}

static void __exit cmd_vstelnet_exit(void)
{
	vmm_cmdmgr_unregister_cmd(&cmd_vstelnet);
}

VMM_DECLARE_MODULE(MODULE_DESC, 
			MODULE_AUTHOR, 
			MODULE_LICENSE, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
