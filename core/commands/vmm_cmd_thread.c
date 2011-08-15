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
 * @file vmm_cmd_thread.c
 * @version 0.01
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @author Anup Patel (anup@brainfault.org)
 * @brief Command file for hypervisor threads control.
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_version.h>
#include <vmm_mterm.h>
#include <vmm_string.h>
#include <vmm_threads.h>

void cmd_thread_usage(void)
{
	vmm_printf("Usage:\n");
	vmm_printf("   thread help\n");
	vmm_printf("   thread list\n");
}

void cmd_thread_list(void)
{
	int rc, index, count;
	char state[10], name[64];
	vmm_thread_t *tinfo;
	vmm_printf("----------------------------------------"
		   "----------------------------------------\n");
	vmm_printf("| %-5s| %-9s| %-59s|\n", 
		   "ID ", "State", "Name");
	vmm_printf("----------------------------------------"
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
		vmm_printf("| %-5d| %-9s| %-59s|\n", 
					   vmm_threads_get_id(tinfo), 
					   state, name);
	}
	vmm_printf("----------------------------------------"
		   "----------------------------------------\n");
}

int cmd_thread_exec(int argc, char **argv)
{
	if (argc == 2) {
		if (vmm_strcmp(argv[1], "help") == 0) {
			cmd_thread_usage();
			return VMM_OK;
		} else if (vmm_strcmp(argv[1], "list") == 0) {
			cmd_thread_list();
			return VMM_OK;
		}
	}
	cmd_thread_usage();
	return VMM_EFAIL;
}

VMM_DECLARE_CMD(thread, "control commands for threads",
		cmd_thread_exec, NULL);
