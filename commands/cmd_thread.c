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
 * @file cmd_thread.c
 * @version 0.01
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @author Anup Patel (anup@brainfault.org)
 * @brief Command file for hypervisor threads control.
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_version.h>
#include <vmm_string.h>
#include <vmm_threads.h>
#include <vmm_modules.h>
#include <vmm_cmdmgr.h>

#define MODULE_VARID			cmd_thread_module
#define MODULE_NAME			"Command thread"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			cmd_thread_init
#define	MODULE_EXIT			cmd_thread_exit

void cmd_thread_usage(vmm_chardev_t *cdev)
{
	vmm_cprintf(cdev, "Usage:\n");
	vmm_cprintf(cdev, "   thread help\n");
	vmm_cprintf(cdev, "   thread list\n");
}

void cmd_thread_list(vmm_chardev_t *cdev)
{
	int rc, index, count;
	char state[10], name[64];
	vmm_thread_t *tinfo;
	vmm_cprintf(cdev, "----------------------------------------"
		   	  "----------------------------------------\n");
	vmm_cprintf(cdev, "| %-5s| %-6s| %-9s| %-51s|\n", 
		   	  "ID ", "Prio", "State", "Name");
	vmm_cprintf(cdev, "----------------------------------------"
		   	  "----------------------------------------\n");
	count = vmm_threads_count();
	for (index = 0; index < count; index++) {
		tinfo = vmm_threads_index2thread(index);
		switch (vmm_threads_get_state(tinfo)) {
		case VMM_THREAD_STATE_CREATED:
			vmm_strcpy(state, "Created");
			break;
		case VMM_THREAD_STATE_RUNNING:
			vmm_strcpy(state, "Running");
			break;
		case VMM_THREAD_STATE_SLEEPING:
			vmm_strcpy(state, "Sleeping");
			break;
		case VMM_THREAD_STATE_STOPPED:
			vmm_strcpy(state, "Stopped");
			break;
		default:
			vmm_strcpy(state, "Invalid");
			break;
		}
		if ((rc = vmm_threads_get_name(name, tinfo))) {
			vmm_strcpy(name, "(NA)");
		}
		vmm_cprintf(cdev, "| %-5d| %-6d| %-9s| %-51s|\n", 
				  vmm_threads_get_id(tinfo), 
				  vmm_threads_get_priority(tinfo), 
				  state, name);
	}
	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");
}

int cmd_thread_exec(vmm_chardev_t *cdev, int argc, char **argv)
{
	if (argc == 2) {
		if (vmm_strcmp(argv[1], "help") == 0) {
			cmd_thread_usage(cdev);
			return VMM_OK;
		} else if (vmm_strcmp(argv[1], "list") == 0) {
			cmd_thread_list(cdev);
			return VMM_OK;
		}
	}
	cmd_thread_usage(cdev);
	return VMM_EFAIL;
}

static vmm_cmd_t cmd_thread = {
	.name = "thread",
	.desc = "control commands for threads",
	.usage = cmd_thread_usage,
	.exec = cmd_thread_exec,
};

static int __init cmd_thread_init(void)
{
	return vmm_cmdmgr_register_cmd(&cmd_thread);
}

static void cmd_thread_exit(void)
{
	vmm_cmdmgr_unregister_cmd(&cmd_thread);
}

VMM_DECLARE_MODULE(MODULE_VARID, 
			MODULE_NAME, 
			MODULE_AUTHOR, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
