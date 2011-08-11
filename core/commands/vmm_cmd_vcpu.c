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
 * @file vmm_cmd_vcpu.c
 * @version 0.01
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of vcpu command
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_string.h>
#include <vmm_devtree.h>
#include <vmm_manager.h>
#include <vmm_mterm.h>

void cmd_vcpu_usage(void)
{
	vmm_printf("Usage:\n");
	vmm_printf("   vcpu help\n");
	vmm_printf("   vcpu list\n");
	vmm_printf("   vcpu reset   <vcpu_num>\n");
	vmm_printf("   vcpu kick    <vcpu_num>\n");
	vmm_printf("   vcpu pause   <vcpu_num>\n");
	vmm_printf("   vcpu resume  <vcpu_num>\n");
	vmm_printf("   vcpu halt    <vcpu_num>\n");
	vmm_printf("   vcpu dumpreg <vcpu_num>\n");
}

void cmd_vcpu_list()
{
	int num, count;
	char state[10];
	char path[256];
	vmm_vcpu_t *vcpu;
	vmm_printf("----------------------------------------"
		   "----------------------------------------\n");
	vmm_printf("| %-5s| %-9s| %-16s| %-41s|\n", 
		   "Num", "State", "Name", "Device Path");
	vmm_printf("----------------------------------------"
		   "----------------------------------------\n");
	count = vmm_manager_vcpu_count();
	for (num = 0; num < count; num++) {
		vcpu = vmm_manager_vcpu(num);
		switch (vcpu->state) {
		case VMM_VCPU_STATE_UNKNOWN:
			vmm_strcpy(state, "Unknown");
			break;
		case VMM_VCPU_STATE_RESET:
			vmm_strcpy(state, "Reset");
			break;
		case VMM_VCPU_STATE_READY:
			vmm_strcpy(state, "Ready");
			break;
		case VMM_VCPU_STATE_RUNNING:
			vmm_strcpy(state, "Running");
			break;
		case VMM_VCPU_STATE_PAUSED:
			vmm_strcpy(state, "Paused");
			break;
		case VMM_VCPU_STATE_HALTED:
			vmm_strcpy(state, "Halted");
			break;
		default:
			vmm_strcpy(state, "Invalid");
			break;
		}
		if (vcpu->guest) {
			vmm_devtree_getpath(path, vcpu->node);
			vmm_printf("| %-5d| %-9s| %-16s| %-41s|\n", 
				   num, state, vcpu->name, path);
		} else {
			vmm_printf("| %-5d| %-9s| %-16s| %-41s|\n", 
				   num, state, vcpu->name, "(NA)");
		}
	}
	vmm_printf("----------------------------------------"
		   "----------------------------------------\n");
}

int cmd_vcpu_reset(int num)
{
	int ret = VMM_EFAIL;
	vmm_vcpu_t *vcpu = vmm_manager_vcpu(num);
	if (vcpu) {
		if ((ret = vmm_manager_vcpu_reset(vcpu))) {
			vmm_printf("%s: Failed to reset\n", vcpu->name);
		} else {
			vmm_printf("%s: Reset done\n", vcpu->name);
		}
	} else {
		vmm_printf("Failed to find vcpu\n");
	}
	return ret;
}

int cmd_vcpu_kick(int num)
{
	int ret = VMM_EFAIL;
	vmm_vcpu_t *vcpu = vmm_manager_vcpu(num);
	if (vcpu) {
		if ((ret = vmm_manager_vcpu_kick(vcpu))) {
			vmm_printf("%s: Failed to kick\n", vcpu->name);
		} else {
			vmm_printf("%s: Kicked\n", vcpu->name);
		}
	} else {
		vmm_printf("Failed to find vcpu\n");
	}
	return ret;
}

int cmd_vcpu_pause(int num)
{
	int ret = VMM_EFAIL;
	vmm_vcpu_t *vcpu = vmm_manager_vcpu(num);
	if (vcpu) {
		;
		if ((ret = vmm_manager_vcpu_pause(vcpu))) {
			vmm_printf("%s: Failed to pause\n", vcpu->name);
		} else {
			vmm_printf("%s: Paused\n", vcpu->name);
		}
	} else {
		vmm_printf("Failed to find vcpu\n");
	}
	return ret;
}

int cmd_vcpu_resume(int num)
{
	int ret = VMM_EFAIL;
	vmm_vcpu_t *vcpu = vmm_manager_vcpu(num);
	if (vcpu) {
		if ((ret = vmm_manager_vcpu_resume(vcpu))) {
			vmm_printf("%s: Failed to resume\n", vcpu->name);
		} else {
			vmm_printf("%s: Resumed\n", vcpu->name);
		}
	} else {
		vmm_printf("Failed to find vcpu\n");
	}
	return ret;
}

int cmd_vcpu_halt(int num)
{
	int ret = VMM_EFAIL;
	vmm_vcpu_t *vcpu = vmm_manager_vcpu(num);
	if (vcpu) {
		if ((ret = vmm_manager_vcpu_halt(vcpu))) {
			vmm_printf("%s: Failed to halt\n", vcpu->name);
		} else {
			vmm_printf("%s: Halted\n", vcpu->name);
		}
	} else {
		vmm_printf("Failed to find vcpu\n");
	}
	return ret;
}

int cmd_vcpu_dumpreg(int num)
{
	int ret = VMM_EFAIL;
	vmm_vcpu_t *vcpu = vmm_manager_vcpu(num);
	if (vcpu) {
		if ((ret = vmm_manager_vcpu_dumpreg(vcpu))) {
			vmm_printf("%s: Failed to dumpreg\n", vcpu->name);
		}
	} else {
		vmm_printf("Failed to find vcpu\n");
	}
	return ret;
}

int cmd_vcpu_exec(int argc, char **argv)
{
	int num;
	if (argc == 2) {
		if (vmm_strcmp(argv[1], "help") == 0) {
			cmd_vcpu_usage();
			return VMM_OK;
		} else if (vmm_strcmp(argv[1], "list") == 0) {
			cmd_vcpu_list();
			return VMM_OK;
		}
	}
	if (argc < 3) {
		cmd_vcpu_usage();
		return VMM_EFAIL;
	}
	num = vmm_str2int(argv[2], 10);
	if (vmm_strcmp(argv[1], "reset") == 0) {
		return cmd_vcpu_reset(num);
	} else if (vmm_strcmp(argv[1], "kick") == 0) {
		return cmd_vcpu_kick(num);
	} else if (vmm_strcmp(argv[1], "pause") == 0) {
		return cmd_vcpu_pause(num);
	} else if (vmm_strcmp(argv[1], "resume") == 0) {
		return cmd_vcpu_resume(num);
	} else if (vmm_strcmp(argv[1], "halt") == 0) {
		return cmd_vcpu_halt(num);
	} else if (vmm_strcmp(argv[1], "dumpreg") == 0) {
		return cmd_vcpu_dumpreg(num);
	} else {
		cmd_vcpu_usage();
		return VMM_EFAIL;
	}
	return VMM_OK;
}

VMM_DECLARE_CMD(vcpu, "control commands for vcpu", cmd_vcpu_exec, NULL);
