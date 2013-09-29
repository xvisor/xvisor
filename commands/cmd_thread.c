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
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @author Anup Patel (anup@brainfault.org)
 * @brief Command file for hypervisor threads control.
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_version.h>
#include <vmm_threads.h>
#include <vmm_modules.h>
#include <vmm_cmdmgr.h>
#include <libs/stringlib.h>

#define MODULE_DESC			"Command thread"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			cmd_thread_init
#define	MODULE_EXIT			cmd_thread_exit

void cmd_thread_usage(struct vmm_chardev *cdev)
{
	vmm_cprintf(cdev, "Usage:\n");
	vmm_cprintf(cdev, "   thread help\n");
	vmm_cprintf(cdev, "   thread list\n");
}

void cmd_thread_list(struct vmm_chardev *cdev)
{
	int rc, index, count;
	char state[10], name[VMM_FIELD_NAME_SIZE];
	struct vmm_thread *tinfo;
	vmm_cprintf(cdev, "----------------------------------------"
		   	  "----------------------------------------\n");
	vmm_cprintf(cdev, " %-6s %-7s %-10s %-53s\n", 
		   	  "ID ", "Prio", "State", "Name");
	vmm_cprintf(cdev, "----------------------------------------"
		   	  "----------------------------------------\n");
	count = vmm_threads_count();
	for (index = 0; index < count; index++) {
		tinfo = vmm_threads_index2thread(index);
		switch (vmm_threads_get_state(tinfo)) {
		case VMM_THREAD_STATE_CREATED:
			strcpy(state, "Created");
			break;
		case VMM_THREAD_STATE_RUNNING:
			strcpy(state, "Running");
			break;
		case VMM_THREAD_STATE_SLEEPING:
			strcpy(state, "Sleeping");
			break;
		case VMM_THREAD_STATE_STOPPED:
			strcpy(state, "Stopped");
			break;
		default:
			strcpy(state, "Invalid");
			break;
		}
		if ((rc = vmm_threads_get_name(name, tinfo))) {
			strcpy(name, "(NA)");
		}
		vmm_cprintf(cdev, " %-6d %-7d %-10s %-53s\n", 
				  vmm_threads_get_id(tinfo), 
				  vmm_threads_get_priority(tinfo), 
				  state, name);
	}
	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");
}

int cmd_thread_exec(struct vmm_chardev *cdev, int argc, char **argv)
{
	if (argc == 2) {
		if (strcmp(argv[1], "help") == 0) {
			cmd_thread_usage(cdev);
			return VMM_OK;
		} else if (strcmp(argv[1], "list") == 0) {
			cmd_thread_list(cdev);
			return VMM_OK;
		}
	}
	cmd_thread_usage(cdev);
	return VMM_EFAIL;
}

static struct vmm_cmd cmd_thread = {
	.name = "thread",
	.desc = "control commands for threads",
	.usage = cmd_thread_usage,
	.exec = cmd_thread_exec,
};

static int __init cmd_thread_init(void)
{
	return vmm_cmdmgr_register_cmd(&cmd_thread);
}

static void __exit cmd_thread_exit(void)
{
	vmm_cmdmgr_unregister_cmd(&cmd_thread);
}

VMM_DECLARE_MODULE(MODULE_DESC, 
			MODULE_AUTHOR, 
			MODULE_LICENSE, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
