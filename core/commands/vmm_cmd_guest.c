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
#include <vmm_host_aspace.h>
#include <vmm_elf.h>

void cmd_guest_usage(void)
{
	vmm_printf("Usage:\n");
	vmm_printf("   guest help\n");
	vmm_printf("   guest list\n");
	vmm_printf("   guest reset   <guest_num>\n");
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

int cmd_guest_reset(int num)
{
	int ret = VMM_EFAIL;
	vmm_guest_t *guest = vmm_scheduler_guest(num);
	if (guest) {
		if ((ret = vmm_scheduler_guest_reset(guest))) {
			vmm_printf("%s: Failed to reset\n", guest->node->name);
		} else {
			vmm_printf("%s: Reset done\n", guest->node->name);
		}
	} else {
		vmm_printf("Failed to find guest\n");
	}
	return ret;
}

int cmd_guest_kick(int num)
{
	int ret = VMM_EFAIL;
	vmm_guest_t *guest = vmm_scheduler_guest(num);
	if (guest) {
		if ((ret = vmm_scheduler_guest_kick(guest))) {
			vmm_printf("%s: Failed to kick\n", guest->node->name);
		} else {
			vmm_printf("%s: Kicked\n", guest->node->name);
		}
	} else {
		vmm_printf("Failed to find guest\n");
	}
	return ret;
}

int cmd_guest_pause(int num)
{
	int ret = VMM_EFAIL;
	vmm_guest_t *guest = vmm_scheduler_guest(num);
	if (guest) {
		;
		if ((ret = vmm_scheduler_guest_pause(guest))) {
			vmm_printf("%s: Failed to pause\n", guest->node->name);
		} else {
			vmm_printf("%s: Paused\n", guest->node->name);
		}
	} else {
		vmm_printf("Failed to find guest\n");
	}
	return ret;
}

int cmd_guest_resume(int num)
{
	int ret = VMM_EFAIL;
	vmm_guest_t *guest = vmm_scheduler_guest(num);
	if (guest) {
		if ((ret = vmm_scheduler_guest_resume(guest))) {
			vmm_printf("%s: Failed to resume\n", guest->node->name);
		} else {
			vmm_printf("%s: Resumed\n", guest->node->name);
		}
	} else {
		vmm_printf("Failed to find guest\n");
	}
	return ret;
}

int cmd_guest_halt(int num)
{
	int ret = VMM_EFAIL;
	vmm_guest_t *guest = vmm_scheduler_guest(num);
	if (guest) {
		if ((ret = vmm_scheduler_guest_halt(guest))) {
			vmm_printf("%s: Failed to halt\n", guest->node->name);
		} else {
			vmm_printf("%s: Halted\n", guest->node->name);
		}
	} else {
		vmm_printf("Failed to find guest\n");
	}
	return ret;
}

int cmd_guest_dumpreg(int num)
{
	int ret = VMM_EFAIL;
	vmm_guest_t *guest = vmm_scheduler_guest(num);
	if (guest) {
		if ((ret = vmm_scheduler_guest_dumpreg(guest))) {
			vmm_printf("%s: Failed to dumpreg\n", guest->node->name);
		}
	} else {
		vmm_printf("Failed to find guest\n");
	}
	return ret;
}

int cmd_guest_exec(int argc, char **argv)
{
	int num, count;
	u32 src_addr, dest_addr, size;
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
	if (vmm_strcmp(argv[1], "reset") == 0) {
		if (num == -1) {
			for (num = 0; num < count; num++) {
				ret = cmd_guest_reset(num);
				if (ret) {
					return ret;
				}
			}
		} else {
			return cmd_guest_reset(num);
		}
	} else if (vmm_strcmp(argv[1], "kick") == 0) {
		if (num == -1) {
			for (num = 0; num < count; num++) {
				ret = cmd_guest_kick(num);
				if (ret) {
					return ret;
				}
			}
		} else {
			return cmd_guest_kick(num);
		}
	} else if (vmm_strcmp(argv[1], "pause") == 0) {
		if (num == -1) {
			for (num = 0; num < count; num++) {
				ret = cmd_guest_pause(num);
				if (ret) {
					return ret;
				}
			}
		} else {
			return cmd_guest_pause(num);
		}
	} else if (vmm_strcmp(argv[1], "resume") == 0) {
		if (num == -1) {
			for (num = 0; num < count; num++) {
				ret = cmd_guest_resume(num);
				if (ret) {
					return ret;
				}
			}
		} else {
			return cmd_guest_resume(num);
		}
	} else if (vmm_strcmp(argv[1], "halt") == 0) {
		if (num == -1) {
			for (num = 0; num < count; num++) {
				ret = cmd_guest_halt(num);
				if (ret) {
					return ret;
				}
			}
		} else {
			return cmd_guest_halt(num);
		}
	} else if (vmm_strcmp(argv[1], "dumpreg") == 0) {
		if (num == -1) {
			for (num = 0; num < count; num++) {
				ret = cmd_guest_dumpreg(num);
				if (ret) {
					return ret;
				}
			}
		} else {
			return cmd_guest_dumpreg(num);
		}
	} else {
		cmd_guest_usage();
		return VMM_EFAIL;
	}
	return VMM_OK;
}

VMM_DECLARE_CMD(guest, "control commands for guest", cmd_guest_exec, NULL);
