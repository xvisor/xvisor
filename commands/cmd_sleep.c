/**
 * Copyright (c) 2015 Himanshu Chauhan
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
 * @file cmd_sleep.c
 * @author Himanshu Chauhan <hchauhan@xvisor-x86.org>
 * @brief Implementation of sleep command
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_version.h>
#include <vmm_modules.h>
#include <vmm_cmdmgr.h>
#include <vmm_delay.h>

#define MODULE_DESC			"Command sleep"
#define MODULE_AUTHOR			"Himanshu Chauhan"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			cmd_sleep_init
#define	MODULE_EXIT			cmd_sleep_exit

static void cmd_sleep_usage(struct vmm_chardev *cdev)
{
	vmm_cprintf(cdev, "Usage: ");
	vmm_cprintf(cdev, "   sleep <number of seconds>\n");
}

static int cmd_sleep_exec(struct vmm_chardev *cdev, int argc, char **argv)
{
	if (argc < 2) {
		cmd_sleep_usage(cdev);
		return VMM_EFAIL;
	}

	int secs = atoi(argv[1]);

	vmm_ssleep(secs);

	return VMM_OK;
}

static struct vmm_cmd cmd_sleep = {
	.name = "sleep",
	.desc = "Sleep the terminal thread of execution for given seconds",
	.usage = cmd_sleep_usage,
	.exec = cmd_sleep_exec,
};

static int __init cmd_sleep_init(void)
{
	return vmm_cmdmgr_register_cmd(&cmd_sleep);
}

static void __exit cmd_sleep_exit(void)
{
	vmm_cmdmgr_unregister_cmd(&cmd_sleep);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
