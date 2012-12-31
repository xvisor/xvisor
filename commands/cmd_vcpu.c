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
 * @file cmd_vcpu.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of vcpu command
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_devtree.h>
#include <vmm_manager.h>
#include <vmm_modules.h>
#include <vmm_cmdmgr.h>
#include <libs/stringlib.h>

#define MODULE_DESC			"Command vcpu"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			cmd_vcpu_init
#define	MODULE_EXIT			cmd_vcpu_exit

static void cmd_vcpu_usage(struct vmm_chardev *cdev)
{
	vmm_cprintf(cdev, "Usage:\n");
	vmm_cprintf(cdev, "   vcpu help\n");
	vmm_cprintf(cdev, "   vcpu list\n");
	vmm_cprintf(cdev, "   vcpu reset   <vcpu_id>\n");
	vmm_cprintf(cdev, "   vcpu kick    <vcpu_id>\n");
	vmm_cprintf(cdev, "   vcpu pause   <vcpu_id>\n");
	vmm_cprintf(cdev, "   vcpu resume  <vcpu_id>\n");
	vmm_cprintf(cdev, "   vcpu halt    <vcpu_id>\n");
	vmm_cprintf(cdev, "   vcpu dumpreg <vcpu_id>\n");
	vmm_cprintf(cdev, "   vcpu dumpstat <vcpu_id>\n");
}

static int cmd_vcpu_help(struct vmm_chardev *cdev, int dummy)
{
	cmd_vcpu_usage(cdev);
	return VMM_OK;
}

static int cmd_vcpu_list(struct vmm_chardev *cdev, int dummy)
{
	int id, count;
	char state[10];
	char path[256];
	struct vmm_vcpu *vcpu;
	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");
	vmm_cprintf(cdev, " %-6s", "ID ");
#ifdef CONFIG_SMP
	vmm_cprintf(cdev, " %-6s", "CPU ");
#endif
	vmm_cprintf(cdev, " %-7s %-10s %-17s %-35s\n", 
		    "Prio", "State", "Name", "Device Path");
	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");
	count = vmm_manager_max_vcpu_count();
	for (id = 0; id < count; id++) {
		if (!(vcpu = vmm_manager_vcpu(id))) {
			continue;
		}
		switch (vcpu->state) {
		case VMM_VCPU_STATE_UNKNOWN:
			strcpy(state, "Unknown");
			break;
		case VMM_VCPU_STATE_RESET:
			strcpy(state, "Reset");
			break;
		case VMM_VCPU_STATE_READY:
			strcpy(state, "Ready");
			break;
		case VMM_VCPU_STATE_RUNNING:
			strcpy(state, "Running");
			break;
		case VMM_VCPU_STATE_PAUSED:
			strcpy(state, "Paused");
			break;
		case VMM_VCPU_STATE_HALTED:
			strcpy(state, "Halted");
			break;
		default:
			strcpy(state, "Invalid");
			break;
		}
		vmm_cprintf(cdev, " %-6d", id);
#ifdef CONFIG_SMP
		vmm_cprintf(cdev, " %-6d", vcpu->hcpu);
#endif
		vmm_cprintf(cdev, " %-7d %-10s %-17s", vcpu->priority, state, vcpu->name);
		if (vcpu->node) {
			vmm_devtree_getpath(path, vcpu->node);
			vmm_cprintf(cdev, " %-35s\n", path); 
		} else {
			vmm_cprintf(cdev, " %-35s\n", "(NA)"); 
		}
	}
	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");
	return VMM_OK;
}

static int cmd_vcpu_reset(struct vmm_chardev *cdev, int id)
{
	int ret = VMM_EFAIL;
	struct vmm_vcpu *vcpu = vmm_manager_vcpu(id);
	if (vcpu) {
		if ((ret = vmm_manager_vcpu_reset(vcpu))) {
			vmm_cprintf(cdev, "%s: Failed to reset\n", vcpu->name);
		} else {
			vmm_cprintf(cdev, "%s: Reset done\n", vcpu->name);
		}
	} else {
		vmm_cprintf(cdev, "Failed to find vcpu\n");
	}
	return ret;
}

static int cmd_vcpu_kick(struct vmm_chardev *cdev, int id)
{
	int ret = VMM_EFAIL;
	struct vmm_vcpu *vcpu = vmm_manager_vcpu(id);
	if (vcpu) {
		if ((ret = vmm_manager_vcpu_kick(vcpu))) {
			vmm_cprintf(cdev, "%s: Failed to kick\n", vcpu->name);
		} else {
			vmm_cprintf(cdev, "%s: Kicked\n", vcpu->name);
		}
	} else {
		vmm_cprintf(cdev, "Failed to find vcpu\n");
	}
	return ret;
}

static int cmd_vcpu_pause(struct vmm_chardev *cdev, int id)
{
	int ret = VMM_EFAIL;
	struct vmm_vcpu *vcpu = vmm_manager_vcpu(id);
	if (vcpu) {
		;
		if ((ret = vmm_manager_vcpu_pause(vcpu))) {
			vmm_cprintf(cdev, "%s: Failed to pause\n", vcpu->name);
		} else {
			vmm_cprintf(cdev, "%s: Paused\n", vcpu->name);
		}
	} else {
		vmm_cprintf(cdev, "Failed to find vcpu\n");
	}
	return ret;
}

static int cmd_vcpu_resume(struct vmm_chardev *cdev, int id)
{
	int ret = VMM_EFAIL;
	struct vmm_vcpu *vcpu = vmm_manager_vcpu(id);
	if (vcpu) {
		if ((ret = vmm_manager_vcpu_resume(vcpu))) {
			vmm_cprintf(cdev, "%s: Failed to resume\n", 
					  vcpu->name);
		} else {
			vmm_cprintf(cdev, "%s: Resumed\n", vcpu->name);
		}
	} else {
		vmm_cprintf(cdev, "Failed to find vcpu\n");
	}
	return ret;
}

static int cmd_vcpu_halt(struct vmm_chardev *cdev, int id)
{
	int ret = VMM_EFAIL;
	struct vmm_vcpu *vcpu = vmm_manager_vcpu(id);
	if (vcpu) {
		if ((ret = vmm_manager_vcpu_halt(vcpu))) {
			vmm_cprintf(cdev, "%s: Failed to halt\n", vcpu->name);
		} else {
			vmm_cprintf(cdev, "%s: Halted\n", vcpu->name);
		}
	} else {
		vmm_cprintf(cdev, "Failed to find vcpu\n");
	}
	return ret;
}

static int cmd_vcpu_dumpreg(struct vmm_chardev *cdev, int id)
{
	int ret = VMM_EFAIL;
	struct vmm_vcpu *vcpu = vmm_manager_vcpu(id);
	if (vcpu) {
		if ((ret = vmm_manager_vcpu_dumpreg(vcpu))) {
			vmm_cprintf(cdev, "%s: Failed to dumpreg\n", 
					  vcpu->name);
		}
	} else {
		vmm_cprintf(cdev, "Failed to find vcpu\n");
	}
	return ret;
}

static int cmd_vcpu_dumpstat(struct vmm_chardev *cdev, int id)
{
	int ret = VMM_EFAIL;
	struct vmm_vcpu *vcpu = vmm_manager_vcpu(id);
	if (vcpu) {
		if ((ret = vmm_manager_vcpu_dumpstat(vcpu))) {
			vmm_cprintf(cdev, "%s: Failed to dumpstat\n", 
					  vcpu->name);
		}
	} else {
		vmm_cprintf(cdev, "Failed to find vcpu\n");
	}
	return ret;
}

static const struct {
	char *name;
	int (*function) (struct vmm_chardev *, int);
} const command[] = {
	{"help", cmd_vcpu_help},
	{"list", cmd_vcpu_list},
	{"reset", cmd_vcpu_reset},
	{"kick", cmd_vcpu_kick},
	{"pause", cmd_vcpu_pause},
	{"resume", cmd_vcpu_resume},
	{"halt", cmd_vcpu_halt},
	{"dumpreg", cmd_vcpu_dumpreg},
	{"dumpstat", cmd_vcpu_dumpstat},
	{NULL, NULL},
};
	
static int cmd_vcpu_exec(struct vmm_chardev *cdev, int argc, char **argv)
{
	int id = -1;
	int index = 0;

	if ((argc == 1) || (argc > 3)) {
		cmd_vcpu_usage(cdev);
		return VMM_EFAIL;
	}

	if (argc == 3) {
		id = str2int(argv[2], 10);
	}
	
	while (command[index].name) {
		if (strcmp(argv[1], command[index].name) == 0) {
			return command[index].function(cdev, id);
		}
		index++;
	}

	cmd_vcpu_usage(cdev);

	return VMM_EFAIL;
}

static struct vmm_cmd cmd_vcpu = {
	.name = "vcpu",
	.desc = "control commands for vcpu",
	.usage = cmd_vcpu_usage,
	.exec = cmd_vcpu_exec,
};

static int __init cmd_vcpu_init(void)
{
	return vmm_cmdmgr_register_cmd(&cmd_vcpu);
}

static void __exit cmd_vcpu_exit(void)
{
	vmm_cmdmgr_unregister_cmd(&cmd_vcpu);
}

VMM_DECLARE_MODULE(MODULE_DESC, 
			MODULE_AUTHOR, 
			MODULE_LICENSE, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
