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
#include <vmm_delay.h>
#include <vmm_devtree.h>
#include <vmm_manager.h>
#include <vmm_scheduler.h>
#include <vmm_host_ram.h>
#include <vmm_host_vapool.h>
#include <vmm_host_aspace.h>
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
	vmm_cprintf(cdev, "   vcpu orphan_list\n");
	vmm_cprintf(cdev, "   vcpu normal_list\n");
	vmm_cprintf(cdev, "   vcpu monitor [<output_chardev_name>]\n");
	vmm_cprintf(cdev, "   vcpu reset   <vcpu_id>\n");
	vmm_cprintf(cdev, "   vcpu kick    <vcpu_id>\n");
	vmm_cprintf(cdev, "   vcpu pause   <vcpu_id>\n");
	vmm_cprintf(cdev, "   vcpu resume  <vcpu_id>\n");
	vmm_cprintf(cdev, "   vcpu halt    <vcpu_id>\n");
	vmm_cprintf(cdev, "   vcpu dumpreg <vcpu_id>\n");
	vmm_cprintf(cdev, "   vcpu dumpstat <vcpu_id>\n");
}

static int cmd_vcpu_help(struct vmm_chardev *cdev,
			 int argc, char **argv)
{
	cmd_vcpu_usage(cdev);
	return VMM_OK;
}

struct vcpu_list_priv {
	struct vmm_chardev *cdev;
	bool normal;
	bool orphan;
};

static int vcpu_list_iter(struct vmm_vcpu *vcpu, void *priv)
{
	u32 hcpu, afflen;
	char state[10];
	const struct vmm_cpumask *aff;
	struct vcpu_list_priv *p = priv;
	struct vmm_chardev *cdev = p->cdev;

	if (!(vcpu->is_normal && p->normal) &&
	    !(!vcpu->is_normal && p->orphan)) {
		return VMM_OK;
	}

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
	vmm_cprintf(cdev, " %s", "{");
	aff = vmm_manager_vcpu_get_affinity(vcpu);
	afflen = 0;
	for_each_cpu(hcpu, aff) {
		if (afflen) {
			vmm_cprintf(cdev, ",");
		}
		vmm_cprintf(cdev, "%d", hcpu);
		afflen++;
	}
	vmm_cprintf(cdev, "%s\n", "}");

	return VMM_OK;
}

static int vcpu_list(struct vmm_chardev *cdev, bool normal, bool orphan)
{
	int rc;
	struct vcpu_list_priv p;

	p.cdev = cdev;
	p.normal = normal;
	p.orphan = orphan;

	vmm_cprintf(cdev, "----------------------------------------"
			  "---------------------------------------\n");
	vmm_cprintf(cdev, " %-6s", "ID ");
#ifdef CONFIG_SMP
	vmm_cprintf(cdev, " %-6s", "CPU ");
#endif
	vmm_cprintf(cdev, " %-7s %-10s %-17s %-34s\n", 
		    "Prio", "State", "Name", "Affinity");
	vmm_cprintf(cdev, "----------------------------------------"
			  "---------------------------------------\n");

	rc = vmm_manager_vcpu_iterate(vcpu_list_iter, &p);

	vmm_cprintf(cdev, "----------------------------------------"
			  "---------------------------------------\n");

	return rc;
}

static int cmd_vcpu_list(struct vmm_chardev *cdev,
			 int argc, char **argv)
{
	return vcpu_list(cdev, TRUE, TRUE);
}

static int cmd_vcpu_orphan_list(struct vmm_chardev *cdev,
				int argc, char **argv)
{
	return vcpu_list(cdev, FALSE, TRUE);
}

static int cmd_vcpu_normal_list(struct vmm_chardev *cdev,
				int argc, char **argv)
{
	return vcpu_list(cdev, TRUE, FALSE);
}

static int cmd_vcpu_monitor(struct vmm_chardev *cdev,
			    int argc, char **argv)
{
	char ch;
	bool skip_sleep;
	bool found_escape_ch;
	u32 i, c, util;
	virtual_size_t vfree, vtotal;
	physical_size_t pfree, ptotal;
	struct vmm_chardev *ocdev = NULL;

	if (argc) {
		ocdev = vmm_chardev_find(argv[0]);
	}

	if (!ocdev) {
		ocdev = cdev;
	}

	while (1) {
		/* Reset cursor positon using VT100 command */
		vmm_cputs(ocdev, "\e[H");

		/* Clear entire screen using VT100 command */
		vmm_cputs(ocdev, "\e[J");

		/* Print CPU usage */
		i = 0;
		for_each_online_cpu(c) {
			vmm_cprintf(ocdev, "CPU%d:", c);
			util = udiv64(vmm_scheduler_idle_time(c) * 1000,
				      vmm_scheduler_get_sample_period(c));
			util = (util > 1000) ? 1000 : util;
			util = 1000 - util;
			vmm_cprintf(ocdev, " %d.%01d%%  ",
				    udiv32(util, 10), umod32(util, 10));
			i++;
			if (i % 4 == 0) {
				vmm_cputs(ocdev, "\n");
			}
		}
		if (i % 4) {
			vmm_cputs(ocdev, "\n");
		}

		/* Print VAPOOL usage */
		vfree = vmm_host_vapool_free_page_count();
		vfree *= VMM_PAGE_SIZE;
		vtotal = vmm_host_vapool_total_page_count();
		vtotal *= VMM_PAGE_SIZE;
		if (sizeof(virtual_size_t) == sizeof(u64)) {
			vmm_cprintf(ocdev, "VAPOOL: free %llKiB  "
				"used %llKiB  total %llKiB\n",
				vfree/1024, (vtotal-vfree)/1024, vtotal/1024);
		} else {
			vmm_cprintf(ocdev, "VAPOOL: free %dKiB  "
				"used %dKiB  total %dKiB\n",
				vfree/1024, (vtotal-vfree)/1024, vtotal/1024);
		}
		/* Print RAM usage */
		pfree = vmm_host_ram_total_free_frames();
		pfree *= VMM_PAGE_SIZE;
		ptotal = vmm_host_ram_total_frame_count();
		ptotal *= VMM_PAGE_SIZE;
		if (sizeof(physical_size_t) == sizeof(u64)) {
			vmm_cprintf(ocdev, "RAM: free %llKiB  "
				"used %llKiB  total %llKiB\n",
				pfree/1024, (ptotal-pfree)/1024, ptotal/1024);
		} else {
			vmm_cprintf(ocdev, "RAM: free %dKiB  "
				"used %dKiB  total %dKiB\n",
				pfree/1024, (ptotal-pfree)/1024, ptotal/1024);
		}

		/* Print VCPU list */
		vcpu_list(ocdev, TRUE, TRUE);

		/* Look for escape character 'q' */
		ch = 0;
		skip_sleep = FALSE;
		found_escape_ch = FALSE;
		while (!vmm_scanchars(cdev, &ch, 1, FALSE)) {
			skip_sleep = TRUE;
			if (ch == 'q') {
				found_escape_ch = TRUE;
				break;
			}
		}
		if (found_escape_ch) {
			break;
		}

		/* Sleep for 1 seconds */
		if (!skip_sleep) {
			vmm_ssleep(1);
		}
	}

	return VMM_OK;
}

static int cmd_vcpu_reset(struct vmm_chardev *cdev,
			  int argc, char **argv)
{
	int ret, id;
	struct vmm_vcpu *vcpu;

	if (!argc) {
		vmm_cprintf(cdev, "Must provide vcpu ID\n");
		return VMM_EINVALID;
	}
	id = atoi(argv[0]);

	vcpu = vmm_manager_vcpu(id);
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

static int cmd_vcpu_kick(struct vmm_chardev *cdev,
			 int argc, char **argv)
{
	int ret, id;
	struct vmm_vcpu *vcpu;

	if (!argc) {
		vmm_cprintf(cdev, "Must provide vcpu ID\n");
		return VMM_EINVALID;
	}
	id = atoi(argv[0]);

	vcpu = vmm_manager_vcpu(id);
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

static int cmd_vcpu_pause(struct vmm_chardev *cdev,
			  int argc, char **argv)
{
	int ret, id;
	struct vmm_vcpu *vcpu;

	if (!argc) {
		vmm_cprintf(cdev, "Must provide vcpu ID\n");
		return VMM_EINVALID;
	}
	id = atoi(argv[0]);

	vcpu = vmm_manager_vcpu(id);
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

static int cmd_vcpu_resume(struct vmm_chardev *cdev,
			   int argc, char **argv)
{
	int ret, id;
	struct vmm_vcpu *vcpu;

	if (!argc) {
		vmm_cprintf(cdev, "Must provide vcpu ID\n");
		return VMM_EINVALID;
	}
	id = atoi(argv[0]);

	vcpu = vmm_manager_vcpu(id);
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

static int cmd_vcpu_halt(struct vmm_chardev *cdev,
			 int argc, char **argv)
{
	int ret, id;
	struct vmm_vcpu *vcpu;

	if (!argc) {
		vmm_cprintf(cdev, "Must provide vcpu ID\n");
		return VMM_EINVALID;
	}
	id = atoi(argv[0]);

	vcpu = vmm_manager_vcpu(id);
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

static int cmd_vcpu_dumpreg(struct vmm_chardev *cdev,
			    int argc, char **argv)
{
	int id;
	struct vmm_vcpu *vcpu;

	if (!argc) {
		vmm_cprintf(cdev, "Must provide vcpu ID\n");
		return VMM_EINVALID;
	}
	id = atoi(argv[0]);

	vcpu = vmm_manager_vcpu(id);
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

static int cmd_vcpu_dumpstat(struct vmm_chardev *cdev,
			     int argc, char **argv)
{
	int ret, id;
	u8 priority;
	u32 h, m, s, ms;
	u32 state, hcpu, reset_count;
	u64 last_reset_nsecs, total_nsecs;
	u64 ready_nsecs, running_nsecs, paused_nsecs, halted_nsecs;
	struct vmm_vcpu *vcpu;

	if (!argc) {
		vmm_cprintf(cdev, "Must provide vcpu ID\n");
		return VMM_EINVALID;
	}
	id = atoi(argv[0]);

	vcpu = vmm_manager_vcpu(id);
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
	int (*function) (struct vmm_chardev *, int, char **);
} const command[] = {
	{"help", cmd_vcpu_help},
	{"list", cmd_vcpu_list},
	{"orphan_list", cmd_vcpu_orphan_list},
	{"normal_list", cmd_vcpu_normal_list},
	{"monitor", cmd_vcpu_monitor},
	{"reset", cmd_vcpu_reset},
	{"kick", cmd_vcpu_kick},
	{"pause", cmd_vcpu_pause},
	{"resume", cmd_vcpu_resume},
	{"halt", cmd_vcpu_halt},
	{"dumpreg", cmd_vcpu_dumpreg},
	{"dumpstat", cmd_vcpu_dumpstat},
	{NULL, NULL},
};
	
static int cmd_vcpu_exec(struct vmm_chardev *cdev,
			 int argc, char **argv)
{
	int index = 0;

	if ((argc == 1) || (argc > 3)) {
		cmd_vcpu_usage(cdev);
		return VMM_EFAIL;
	}

	while (command[index].name) {
		if (strcmp(argv[1], command[index].name) == 0) {
			return command[index].function(cdev,
						argc-2, &argv[2]);
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
