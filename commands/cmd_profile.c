/**
 * Copyright (c) 2011 Jean-Christophe Dubois.
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
 * @file cmd_profile.c
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @brief Implementation of profile command
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_modules.h>
#include <vmm_cmdmgr.h>
#include <vmm_heap.h>
#include <vmm_timer.h>
#include <vmm_profiler.h>
#include <arch_atomic.h>
#include <arch_atomic64.h>
#include <libs/stringlib.h>
#include <libs/mathlib.h>
#include <libs/kallsyms.h>
#include <libs/libsort.h>

#define MODULE_DESC			"Command profile"
#define MODULE_AUTHOR			"Jean-Christophe Dubois"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			cmd_profile_init
#define	MODULE_EXIT			cmd_profile_exit

static bool cmd_profile_updated = FALSE;

static void cmd_profile_usage(struct vmm_chardev *cdev)
{
	vmm_cprintf(cdev, "Usage: \n");
	vmm_cprintf(cdev, "   profile help\n");
	vmm_cprintf(cdev, "   profile start\n");
	vmm_cprintf(cdev, "   profile stop\n");
	vmm_cprintf(cdev, "   profile status\n");
	vmm_cprintf(cdev,
		    "   profile dump [name|count|total_time|single_time]\n");
}

static int cmd_profile_help(struct vmm_chardev *cdev, char *dummy)
{
	cmd_profile_usage(cdev);

	return VMM_OK;
}

static int cmd_profile_status(struct vmm_chardev *cdev, char *dummy)
{
	if (vmm_profiler_isactive()) {
		vmm_cprintf(cdev, "profile function is running\n");
	} else {
		vmm_cprintf(cdev, "profile function is not running\n");
	}

	return VMM_OK;
}

static int cmd_profile_name_cmp(void *m, size_t a, size_t b)
{
	int retval = 0;
	struct vmm_profiler_stat *ptr = m;
	char name_a[KSYM_NAME_LEN], name_b[KSYM_NAME_LEN];

	name_a[0] = name_b[0] = name_a[KSYM_NAME_LEN - 1] =
	    name_b[KSYM_NAME_LEN - 1] = 0;

	kallsyms_expand_symbol(kallsyms_get_symbol_offset
			       (ptr[a].counter[0].index), name_a);
	kallsyms_expand_symbol(kallsyms_get_symbol_offset
			       (ptr[b].counter[0].index), name_b);

	retval = strncmp(name_a, name_b, KSYM_NAME_LEN) < 0 ? 1 : 0;

	return retval;
}

static u32 cmd_profile_compute_count(struct vmm_profiler_stat *ptr)
{
	int i;
	u32 count = 0;

	for (i = 0; i < VMM_PROFILE_ARRAY_SIZE; i++) {
		count += arch_atomic_read(&ptr->counter[i].count);
	}

	return count;
}

static int cmd_profile_count_cmp(void *m, size_t a, size_t b)
{
	struct vmm_profiler_stat *ptr = m;
	u32 count_a = cmd_profile_compute_count(&ptr[a]);
	u32 count_b = cmd_profile_compute_count(&ptr[b]);

	if (count_a < count_b) {
		return 1;
	} else if ((count_a == count_b)
		   && arch_atomic_read(&ptr[a].counter[0].count)
		   && arch_atomic_read(&ptr[b].counter[0].count)) {
		return cmd_profile_name_cmp(m, a, b);
	} else {
		return 0;
	}
}

static u64 cmd_profile_compute_total_time(struct vmm_profiler_stat *ptr)
{
	int i;
	u64 time = 0;

	for (i = 0; i < VMM_PROFILE_ARRAY_SIZE; i++) {
		if (arch_atomic_read(&ptr->counter[i].count))
			time += arch_atomic64_read(&ptr->counter[i].total_time);
	}

	return time;
}

static int cmd_profile_total_time_cmp(void *m, size_t a, size_t b)
{
	struct vmm_profiler_stat *ptr = m;
	u64 time_a = cmd_profile_compute_total_time(&ptr[a]);
	u64 time_b = cmd_profile_compute_total_time(&ptr[b]);

	if (time_a < time_b) {
		return 1;
	} else if ((time_a == time_b)
		   && arch_atomic_read(&ptr[a].counter[0].count)
		   && arch_atomic_read(&ptr[b].counter[0].count)) {
		return cmd_profile_name_cmp(m, a, b);
	} else {
		return 0;
	}
}

static u64 cmd_profile_compute_time_per_call(struct vmm_profiler_stat *ptr)
{
	u64 time = 0;
	u64 count = cmd_profile_compute_count(ptr);

	if (count) {
		time = cmd_profile_compute_total_time(ptr);
		time = udiv64(time, count);
	}

	return time;
}

static int cmd_profile_time_per_call_cmp(void *m, size_t a, size_t b)
{
	struct vmm_profiler_stat *ptr = m;
	int time_a = cmd_profile_compute_time_per_call(&ptr[a]);
	int time_b = cmd_profile_compute_time_per_call(&ptr[b]);

	if (time_a < time_b) {
		return 1;
	} else if ((time_a == time_b)
		   && arch_atomic_read(&ptr[a].counter[0].count)
		   && arch_atomic_read(&ptr[b].counter[0].count)) {
		return cmd_profile_name_cmp(m, a, b);
	} else {
		return 0;
	}
}

static void cmd_profile_swap(void *m, size_t a, size_t b)
{
	struct vmm_profiler_stat tmp;
	struct vmm_profiler_stat *ptr = m;

	tmp = ptr[a];
	ptr[a] = ptr[b];
	ptr[b] = tmp;
}

static int cmd_profile_stat_update(void *data, const char *name,
				   unsigned long addr)
{
	int i, count;
	struct vmm_profiler_stat *ptr = data;
	u32 index = kallsyms_get_symbol_pos(addr, NULL, NULL);

	ptr += index;

	for (i = 0; i < VMM_PROFILE_ARRAY_SIZE; i++) {
		ptr->counter[i].index = index;
		count = arch_atomic_read(&ptr->counter[i].count);
		/* we need to compute "time_per_call" */
		if (count) {
			arch_atomic64_write(&ptr->counter[i].time_per_call,
			    udiv64(arch_atomic64_read(&ptr->counter[i].total_time), count));
		} else {
			arch_atomic64_write(&ptr->counter[i].time_per_call, 0);
		}
	}

	/* mark unknown parent as such */
	ptr->counter[VMM_PROFILE_ARRAY_SIZE - 1].parent_index =
	    VMM_PROFILE_OTHER_PARENT;

	return VMM_OK;
}

static const struct {
	char *name;
	int (*function) (void *, size_t, size_t);
} const filters[] = {
	{"count", cmd_profile_count_cmp},
	{"total_time", cmd_profile_total_time_cmp},
	{"single_time", cmd_profile_time_per_call_cmp},
	{"name", cmd_profile_name_cmp},
	{NULL, NULL},
};

static u32 ns_to_micros(u64 count)
{
	if (count > ((u64) 0xffffffff * 1000)) {
		return 0xffffffff;
	} else {
		return (u32) udiv64(count, 1000);
	}
}

static int cmd_profile_dump(struct vmm_chardev *cdev, char *filter_mode)
{
	int index = 0;
	int (*cmp_function) (void *, size_t, size_t) = cmd_profile_count_cmp;
	struct vmm_profiler_stat *stat_array = vmm_profiler_get_stat_array();

	if (stat_array == NULL) {
		vmm_cprintf(cdev, "Profiler stat pointer is NULL\n");
		return VMM_EFAIL;
	}

	if (vmm_profiler_isactive()) {
		vmm_cprintf(cdev, "Can't dump while profiler is active\n");
		return VMM_EFAIL;
	}

	if (filter_mode != NULL) {
		cmp_function = NULL;
		while (filters[index].name) {
			if (strcmp(filter_mode, filters[index].name) == 0) {
				cmp_function = filters[index].function;
				break;
			}
			index++;
		}
	}

	if (cmp_function == NULL) {
		cmd_profile_usage(cdev);
		return VMM_EFAIL;
	}

	if (!cmd_profile_updated) {
		kallsyms_on_each_symbol(cmd_profile_stat_update, stat_array);
		cmd_profile_updated = TRUE;
	}

	libsort_smoothsort(stat_array, 0, kallsyms_num_syms, cmp_function,
			   cmd_profile_swap);

	for (index = 0; index < kallsyms_num_syms; index++) {
		int i;
		struct vmm_profiler_stat *ptr = &stat_array[index];
		u32 total_count = cmd_profile_compute_count(ptr);
		u64 time_per_call = cmd_profile_compute_time_per_call(ptr);
		u64 total_time = cmd_profile_compute_total_time(ptr);

		for (i = 0; i < VMM_PROFILE_ARRAY_SIZE; i++) {
			struct vmm_profiler_counter *cnt = &ptr->counter[i];
			u32 count = arch_atomic_read(&cnt->count);

			if (count) {
				char name[KSYM_NAME_LEN], parent[KSYM_NAME_LEN];

				name[0] = name[KSYM_NAME_LEN - 1] = 0;
				parent[0] = parent[KSYM_NAME_LEN - 1] = 0;

				kallsyms_expand_symbol
				    (kallsyms_get_symbol_offset(cnt->index),
				     name);

				if (stat_array[index].counter[i].parent_index !=
				    VMM_PROFILE_OTHER_PARENT) {
					kallsyms_expand_symbol
					    (kallsyms_get_symbol_offset
					     (cnt->parent_index), parent);
				} else {
					strcpy(parent, "[other]");
				}

				vmm_cprintf(cdev,
					    "%30s -> %-30s %8u/%-8u %10u/%-10u %10u/%-10u\n",
					    parent, name, count, total_count,
					    ns_to_micros(arch_atomic64_read(&cnt->total_time)),
					    ns_to_micros(total_time),
					    ns_to_micros(arch_atomic64_read(&cnt->time_per_call)),
					    ns_to_micros(time_per_call));
			}
		}
	}

	return VMM_OK;
}

static int cmd_profile_start(struct vmm_chardev *cdev, char *dummy)
{
	cmd_profile_updated = FALSE;
	return vmm_profiler_start();
}

static int cmd_profile_stop(struct vmm_chardev *cdev, char *dummy)
{
	return vmm_profiler_stop();
}

static const struct {
	char *name;
	int (*function) (struct vmm_chardev *, char *);
} const command[] = {
	{"help", cmd_profile_help},
	{"start", cmd_profile_start},
	{"stop", cmd_profile_stop},
	{"status", cmd_profile_status},
	{"dump", cmd_profile_dump},
	{NULL, NULL},
};

static int cmd_profile_exec(struct vmm_chardev *cdev, int argc, char **argv)
{
	char *param = NULL;
	int index = 0;

	if (argc > 3) {
		goto fail;
	}

	if (argc == 3) {
		param = argv[2];
	}

	while (command[index].name) {
		if (strcmp(argv[1], command[index].name) == 0) {
			return command[index].function(cdev, param);
		}
		index++;
	}

fail:
	cmd_profile_usage(cdev);
	return VMM_EFAIL;
}

static struct vmm_cmd cmd_profile = {
	.name = "profile",
	.desc = "profile related commands",
	.usage = cmd_profile_usage,
	.exec = cmd_profile_exec,
};

static int __init cmd_profile_init(void)
{
	return vmm_cmdmgr_register_cmd(&cmd_profile);
}

static void __exit cmd_profile_exit(void)
{
	vmm_cmdmgr_unregister_cmd(&cmd_profile);
}

VMM_DECLARE_MODULE(MODULE_DESC,
		   MODULE_AUTHOR,
		   MODULE_LICENSE,
		   MODULE_IPRIORITY,
		   MODULE_INIT,
		   MODULE_EXIT);
