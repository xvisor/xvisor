/**
 * Copyright (c) 2018 Anup Patel.
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
 * @file cmd_clk.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of clk command
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_modules.h>
#include <vmm_cmdmgr.h>
#include <drv/clk.h>

#define MODULE_DESC			"Command clk"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			cmd_clk_init
#define	MODULE_EXIT			cmd_clk_exit

static void cmd_clk_usage(struct vmm_chardev *cdev)
{
	vmm_cprintf(cdev, "Usage: \n");
	vmm_cprintf(cdev, "   clk help\n");
	vmm_cprintf(cdev, "   clk dump\n");
	vmm_cprintf(cdev, "   clk summary\n");
}

static int cmd_clk_dump(struct vmm_chardev *cdev)
{
	return clk_dump(cdev);
}

static int cmd_clk_summary(struct vmm_chardev *cdev)
{
	return clk_summary_show(cdev);
}

static int cmd_clk_exec(struct vmm_chardev *cdev, int argc, char **argv)
{
	if (argc != 2) {
		goto err;
	}

	if (strcmp(argv[1], "help") == 0) {
		cmd_clk_usage(cdev);
		return VMM_OK;
	} else if (strcmp(argv[1], "dump") == 0) {
		return cmd_clk_dump(cdev);
	} else if (strcmp(argv[1], "summary") == 0) {
		return cmd_clk_summary(cdev);
	}

err:
	cmd_clk_usage(cdev);
	return VMM_EINVALID;
}

static struct vmm_cmd cmd_clk = {
	.name = "clk",
	.desc = "clk commands",
	.usage = cmd_clk_usage,
	.exec = cmd_clk_exec,
};

static int __init cmd_clk_init(void)
{
	return vmm_cmdmgr_register_cmd(&cmd_clk);
}

static void __exit cmd_clk_exit(void)
{
	vmm_cmdmgr_unregister_cmd(&cmd_clk);
}

VMM_DECLARE_MODULE(MODULE_DESC,
		   MODULE_AUTHOR,
		   MODULE_LICENSE,
		   MODULE_IPRIORITY,
		   MODULE_INIT,
		   MODULE_EXIT);
