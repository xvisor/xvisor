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
#include <arch_vcpu.h>
#include <libs/mathlib.h>
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

static int cmd_vcpu_list_iter(struct vmm_vcpu *vcpu, void *priv)
{
	char state[10];
	char path[256];
#ifdef CONFIG_SMP
	u32 hcpu;
#endif
	struct vmm_chardev *cdev = priv;

	switch (vmm_manager_vcpu_get_state(vcpu)) {
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
	vmm_cprintf(cdev, " %-6d", vcpu->id);
#ifdef CONFIG_SMP
	vmm_manager_vcpu_get_hcpu(vcpu, &hcpu);
	vmm_cprintf(cdev, " %-6d", hcpu);
#endif
	vmm_cprintf(cdev, " %-7d %-10s %-17s", vcpu->priority, state, vcpu->name);
	if (vcpu->node) {
		vmm_devtree_getpath(path, vcpu->node);
		vmm_cprintf(cdev, " %-34s\n", path); 
	} else {
		vmm_cprintf(cdev, " %-34s\n", "(NA)"); 
	}

	return VMM_OK;
}

static int cmd_vcpu_list(struct vmm_chardev *cdev, int dummy)
{
	int rc;

	vmm_cprintf(cdev, "----------------------------------------"
			  "---------------------------------------\n");
	vmm_cprintf(cdev, " %-6s", "ID ");
#ifdef CONFIG_SMP
	vmm_cprintf(cdev, " %-6s", "CPU ");
#endif
	vmm_cprintf(cdev, " %-7s %-10s %-17s %-34s\n", 
		    "Prio", "State", "Name", "Device Path");
	vmm_cprintf(cdev, "----------------------------------------"
			  "---------------------------------------\n");

	rc = vmm_manager_vcpu_iterate(cmd_vcpu_list_iter, cdev);

	vmm_cprintf(cdev, "----------------------------------------"
			  "---------------------------------------\n");

	return rc;
}

static int cmd_vcpu_reset(struct vmm_chardev *cdev, int id)
{
	int ret;
	struct vmm_vcpu *vcpu = vmm_manager_vcpu(id);

	if (!vcpu) {
		vmm_cprintf(cdev, "Failed to find vcpu\n");
		return VMM_EFAIL;
	}

	if ((ret = vmm_manager_vcpu_reset(vcpu))) {
		vmm_cprintf(cdev, "%s: Failed to reset\n", vcpu->name);
	} else {
		vmm_cprintf(cdev, "%s: Reset\n", vcpu->name);
	}

	return ret;
}

static int cmd_vcpu_kick(struct vmm_chardev *cdev, int id)
{
	int ret;
	struct vmm_vcpu *vcpu = vmm_manager_vcpu(id);

	if (!vcpu) {
		vmm_cprintf(cdev, "Failed to find vcpu\n");
		return VMM_EFAIL;
	}

	if ((ret = vmm_manager_vcpu_kick(vcpu))) {
		vmm_cprintf(cdev, "%s: Failed to kick\n", vcpu->name);
	} else {
		vmm_cprintf(cdev, "%s: Kicked\n", vcpu->name);
	}

	return ret;
}

static int cmd_vcpu_pause(struct vmm_chardev *cdev, int id)
{
	int ret;
	struct vmm_vcpu *vcpu = vmm_manager_vcpu(id);

	if (!vcpu) {
		vmm_cprintf(cdev, "Failed to find vcpu\n");
		return VMM_EFAIL;
	}

	if ((ret = vmm_manager_vcpu_pause(vcpu))) {
		vmm_cprintf(cdev, "%s: Failed to pause\n", vcpu->name);
	} else {
		vmm_cprintf(cdev, "%s: Paused\n", vcpu->name);
	}

	return ret;
}

static int cmd_vcpu_resume(struct vmm_chardev *cdev, int id)
{
	int ret;
	struct vmm_vcpu *vcpu = vmm_manager_vcpu(id);

	if (!vcpu) {
		vmm_cprintf(cdev, "Failed to find vcpu\n");
		return VMM_EFAIL;
	}

	if ((ret = vmm_manager_vcpu_resume(vcpu))) {
		vmm_cprintf(cdev, "%s: Failed to resume\n", 
				  vcpu->name);
	} else {
		vmm_cprintf(cdev, "%s: Resumed\n", vcpu->name);
	}

	return ret;
}

static int cmd_vcpu_halt(struct vmm_chardev *cdev, int id)
{
	int ret;
	struct vmm_vcpu *vcpu = vmm_manager_vcpu(id);

	if (!vcpu) {
		vmm_cprintf(cdev, "Failed to find vcpu\n");
		return VMM_EFAIL;
	}

	if ((ret = vmm_manager_vcpu_halt(vcpu))) {
		vmm_cprintf(cdev, "%s: Failed to halt\n", vcpu->name);
	} else {
		vmm_cprintf(cdev, "%s: Halted\n", vcpu->name);
	}

	return ret;
}

static int cmd_vcpu_dumpreg(struct vmm_chardev *cdev, int id)
{
	struct vmm_vcpu *vcpu = vmm_manager_vcpu(id);

	if (!vcpu) {
		vmm_cprintf(cdev, "Failed to find vcpu\n");
		return VMM_EFAIL;
	}

	/* Architecture specific dumpreg */
	arch_vcpu_regs_dump(cdev, vcpu);

	return VMM_OK;
}

static void nsecs_to_hhmmsstt(u64 nsecs, 
			u32 *hours, u32 *mins, u32 *secs, u32 *msecs)
{
	nsecs = udiv64(nsecs, 1000000ULL);

	*msecs = umod64(nsecs, 1000ULL);
	nsecs = udiv64(nsecs, 1000ULL);

	*secs = umod64(nsecs, 60ULL);
	nsecs = udiv64(nsecs, 60ULL);

	*mins = umod64(nsecs, 60ULL);
	nsecs = udiv64(nsecs, 60ULL);

	*hours = umod64(nsecs, 60ULL);
	nsecs = udiv64(nsecs, 60ULL);
}

static int cmd_vcpu_dumpstat(struct vmm_chardev *cdev, int id)
{
	int ret;
	u8 priority;
	u32 h, m, s, ms;
	u32 state, hcpu, reset_count;
	u64 last_reset_nsecs, total_nsecs;
	u64 ready_nsecs, running_nsecs, paused_nsecs, halted_nsecs;
	struct vmm_vcpu *vcpu = vmm_manager_vcpu(id);

	if (!vcpu) {
		vmm_cprintf(cdev, "Failed to find vcpu\n");
		return VMM_EFAIL;
	}

	/* Retrive general statistics*/
	ret = vmm_manager_vcpu_stats(vcpu, &state, &priority, &hcpu,
					&reset_count, &last_reset_nsecs,
					&ready_nsecs, &running_nsecs,
					&paused_nsecs, &halted_nsecs);
	if (ret) {
		vmm_cprintf(cdev, "%s: Failed to get stats\n", 
				  vcpu->name);
		return ret;
	}

	/* General statistics */
	vmm_cprintf(cdev, "Name             : %s\n", vcpu->name);
	vmm_cprintf(cdev, "State            : ");
	switch (state) {
	case VMM_VCPU_STATE_UNKNOWN:
		vmm_cprintf(cdev, "Unknown\n");
		break;
	case VMM_VCPU_STATE_RESET:
		vmm_cprintf(cdev, "Reset\n");
		break;
	case VMM_VCPU_STATE_READY:
		vmm_cprintf(cdev, "Ready\n");
		break;
	case VMM_VCPU_STATE_RUNNING:
		vmm_cprintf(cdev, "Running\n");
		break;
	case VMM_VCPU_STATE_PAUSED:
		vmm_cprintf(cdev, "Paused\n");
		break;
	case VMM_VCPU_STATE_HALTED:
		vmm_cprintf(cdev, "Halted\n");
		break;
	default:
		vmm_cprintf(cdev, "Invalid\n");
		break;
	}
	vmm_cprintf(cdev, "Priority         : %d\n", priority);
#ifdef CONFIG_SMP
	vmm_cprintf(cdev, "Host CPU         : %d\n", hcpu);
#endif
	vmm_cprintf(cdev, "\n");
	nsecs_to_hhmmsstt(ready_nsecs, &h, &m, &s, &ms);
	vmm_cprintf(cdev, "Ready Time       : %d:%02d:%02d:%03d\n", 
			  h, m, s, ms);
	nsecs_to_hhmmsstt(running_nsecs, &h, &m, &s, &ms);
	vmm_cprintf(cdev, "Running Time     : %d:%02d:%02d:%03d\n", 
			  h, m, s, ms);
	nsecs_to_hhmmsstt(paused_nsecs, &h, &m, &s, &ms);
	vmm_cprintf(cdev, "Paused Time      : %d:%02d:%02d:%03d\n", 
			  h, m, s, ms);
	nsecs_to_hhmmsstt(halted_nsecs, &h, &m, &s, &ms);
	vmm_cprintf(cdev, "Halted Time      : %d:%02d:%02d:%03d\n", 
			  h, m, s, ms);
	total_nsecs =  ready_nsecs;
	total_nsecs += running_nsecs;
	total_nsecs += paused_nsecs;
	total_nsecs += halted_nsecs;
	nsecs_to_hhmmsstt(total_nsecs, &h, &m, &s, &ms);
	vmm_cprintf(cdev, "Total Time       : %d:%02d:%02d:%03d\n", 
			  h, m, s, ms);
	vmm_cprintf(cdev, "\n");
	vmm_cprintf(cdev, "Reset Count      : %d\n", reset_count);
	nsecs_to_hhmmsstt(last_reset_nsecs, &h, &m, &s, &ms);
	vmm_cprintf(cdev, "Last Reset Since : %d:%02d:%02d:%03d\n", 
			  h, m, s, ms);
	vmm_cprintf(cdev, "\n");

	/* Architecture specific dumpstat */
	arch_vcpu_stat_dump(cdev, vcpu);

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
		id = atoi(argv[2]);
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
