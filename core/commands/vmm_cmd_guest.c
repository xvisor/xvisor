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
 * @file vmm_cmd_guest.c
 * @version 0.01
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of guest command
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_string.h>
#include <vmm_devtree.h>
#include <vmm_scheduler.h>
#include <vmm_mterm.h>

void cmd_guest_usage(void)
{
	vmm_printf("Usage:\n");
	vmm_printf("   guest help\n");
	vmm_printf("   guest list\n");
	vmm_printf("   guest kick    <guest_num>\n");
	vmm_printf("   guest pause   <guest_num>\n");
	vmm_printf("   guest resume  <guest_num>\n");
	vmm_printf("   guest halt    <guest_num>\n");
	vmm_printf("   guest dumpreg <guest_num>\n");
	vmm_printf("Note:\n");
	vmm_printf("   if guest_num is -1 then it means all guests\n");
}

void cmd_guest_list()
{
	int num, count;
	char path[256];
	vmm_guest_t *guest;
	vmm_printf("----------------------------------------"
		   "--------------------\n");
	vmm_printf("| %-5s| %-16s| %-32s|\n", "Num", "Name", "Device Path");
	vmm_printf("----------------------------------------"
		   "--------------------\n");
	count = vmm_scheduler_guest_count();
	for (num = 0; num < count; num++) {
		guest = vmm_scheduler_guest(num);
		vmm_devtree_getpath(path, guest->node);
		vmm_printf("| %-5d| %-16s| %-32s|\n", num, guest->node->name,
			   path);
	}
	vmm_printf("----------------------------------------"
		   "--------------------\n");
}

int do_vcpu_cmd(char *cmd, int guest_num)
{
	int ret;
	char cmdstr[128];
	struct dlist *lentry;
	vmm_vcpu_t *vcpu;
	vmm_guest_t *guest;
	guest = vmm_scheduler_guest(guest_num);
	if (guest) {
		list_for_each(lentry, &guest->vcpu_list) {
			vcpu = list_entry(lentry, vmm_vcpu_t, head);
			vmm_sprintf(cmdstr, "vcpu %s %d", cmd, vcpu->num);
			ret = vmm_mterm_proc_cmdstr(cmdstr);
			if (ret)
				return ret;
		}
	} else {
		vmm_printf("Failed to find guest\n");
		return VMM_EFAIL;
	}
	return VMM_OK;
}

int cmd_guest_exec(int argc, char **argv)
{
	int num, count;
	int ret;
	if (argc == 2) {
		if (vmm_strcmp(argv[1], "help") == 0) {
			cmd_guest_usage();
			return VMM_OK;
		} else if (vmm_strcmp(argv[1], "list") == 0) {
			cmd_guest_list();
			return VMM_OK;
		}
	}
	if (argc < 3) {
		cmd_guest_usage();
		return VMM_EFAIL;
	}
	num = vmm_str2int(argv[2], 10);
	count = vmm_scheduler_guest_count();
	if (vmm_strcmp(argv[1], "kick") == 0 ||
	    vmm_strcmp(argv[1], "pause") == 0 ||
	    vmm_strcmp(argv[1], "resume") == 0 ||
	    vmm_strcmp(argv[1], "halt") == 0 ||
	    vmm_strcmp(argv[1], "dumpreg") == 0) {
		if (num == -1) {
			for (num = 0; num < count; num++) {
				ret = do_vcpu_cmd(argv[1], num);
				if (ret) {
					return ret;
				}
			}
		} else {
			return do_vcpu_cmd(argv[1], num);;
		}
	} else {
		cmd_guest_usage();
		return VMM_EFAIL;
	}
	return VMM_OK;
}

VMM_DECLARE_CMD(guest, "control commands for guest", cmd_guest_exec, NULL);
