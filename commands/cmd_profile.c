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
#include <vmm_string.h>
#include <vmm_modules.h>
#include <vmm_cmdmgr.h>
#include <vmm_heap.h>
#include <vmm_timer.h>
#include <vmm_profiler.h>
#include <mathlib.h>
#include <kallsyms.h>
#include <libsort.h>

#define MODULE_NAME			"Command profile"
#define MODULE_AUTHOR			"Jean-Christophe Dubois"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			cmd_profile_init
#define	MODULE_EXIT			cmd_profile_exit

static void cmd_profile_usage(struct vmm_chardev * cdev)
{
	vmm_cprintf(cdev, "Usage: \n");
	vmm_cprintf(cdev, "   profile help\n");
	vmm_cprintf(cdev, "   profile start\n");
	vmm_cprintf(cdev, "   profile stop\n");
	vmm_cprintf(cdev, "   profile status\n");
	vmm_cprintf(cdev, "   profile dump [name|count|total_time|single_time]\n");
}

static int cmd_profile_help(struct vmm_chardev * cdev, char *dummy)
{
	cmd_profile_usage(cdev);

	return VMM_OK;
}

static int cmd_profile_start(struct vmm_chardev * cdev, char *dummy)
{
	return vmm_profiler_start();
}

static int cmd_profile_stop(struct vmm_chardev * cdev, char *dummy)
{
	return vmm_profiler_stop();
}

static int cmd_profile_status(struct vmm_chardev * cdev, char *dummy)
{
	if (vmm_profiler_isactive()) {
		vmm_printf("profile function is running\n");
	} else {
		vmm_printf("profile function is not running\n");
	}

	return VMM_OK;
}

struct count_record {
	u64 total_time;
	u64 time_per_call;
	u32 count;
	char function_name[40];
};

static int cmd_profile_name_cmp(void *m, size_t a, size_t b)
{
	struct count_record *ptr = m;
	int result = vmm_strcmp(&ptr[a].function_name[0], &ptr[b].function_name[0]);

	if (result < 0) {
		return 1;
	} else {
		return 0;
	}
}

static int cmd_profile_count_cmp(void *m, size_t a, size_t b)
{
	struct count_record *ptr = m;

	if (ptr[a].count < ptr[b].count) {
		return 1;
	} else if (ptr[a].count == ptr[b].count) {
		if (vmm_strcmp
		    (&ptr[a].function_name[0], &ptr[b].function_name[0]) < 0) {
			return 1;
		}
	}

	return 0;
}

static int cmd_profile_total_time_cmp(void *m, size_t a, size_t b)
{
	struct count_record *ptr = m;

	if (ptr[a].total_time < ptr[b].total_time) {
		return 1;
	} else if (ptr[a].total_time == ptr[b].total_time) {
		if (vmm_strcmp
		    (&ptr[a].function_name[0], &ptr[b].function_name[0]) < 0) {
			return 1;
		}
	}

	return 0;
}

static int cmd_profile_time_per_call_cmp(void *m, size_t a, size_t b)
{
	struct count_record *ptr = m;

	if (ptr[a].time_per_call < ptr[b].time_per_call) {
		return 1;
	} else if (ptr[a].time_per_call == ptr[b].time_per_call) {
		if (vmm_strcmp
		    (&ptr[a].function_name[0], &ptr[b].function_name[0]) < 0) {
			return 1;
		}
	}

	return 0;
}

static void cmd_profile_swap(void *m, size_t a, size_t b)
{
	struct count_record tmp;
	struct count_record *ptr = m;

	tmp = ptr[a];
	ptr[a] = ptr[b];
	ptr[b] = tmp;
}

static int cmd_profile_count_iterator(void *data, const char *name,
				      unsigned long addr)
{
	struct count_record *ptr = data;
	u32 index = kallsyms_get_symbol_pos(addr, NULL, NULL);
	u32 count = vmm_profiler_get_function_count(addr);
	u64 time = vmm_profiler_get_function_total_time(addr);

	ptr += index;

	/* It would be nice to have the strncpy variant */
	vmm_strcpy(ptr->function_name, name);
	ptr->function_name[39] = 0;
	ptr->count = count;
	ptr->total_time = time;
	if (count) {
		ptr->time_per_call = udiv64(time, (u64)count);
	}

	return VMM_OK;
}

static struct count_record *count_array = NULL;

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
	if (count > ((u64)0xffffffff * 1000)) {
		count = (u64)0xffffffff * 1000;
	}

	return (u32)udiv64(count, 1000);
}

static int cmd_profile_dump(struct vmm_chardev * cdev, char *filter_mode)
{
	int index = 0;
	int (*cmp_function) (void *, size_t, size_t) = cmd_profile_count_cmp;

	if (filter_mode != NULL) {
		cmp_function = NULL;
		while (filters[index].name) {
			if (vmm_strcmp(filter_mode, filters[index].name) == 0) {
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

	kallsyms_on_each_symbol(cmd_profile_count_iterator, count_array);

	libsort_smoothsort(count_array, 0, kallsyms_num_syms, cmp_function,
			   cmd_profile_swap);

	for (index = 0; index < kallsyms_num_syms; index++) {
		if (count_array[index].count) {
			vmm_printf("%-30s %-10u %-10u %u\n",
				   count_array[index].function_name,
				   count_array[index].count,
				   ns_to_micros(count_array[index].total_time),
				   ns_to_micros(count_array[index].time_per_call));
		}
	}

	return VMM_OK;
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

static int cmd_profile_exec(struct vmm_chardev * cdev, int argc, char **argv)
{
	char *param = NULL;
	int index = 0;

	if (argc > 3) {
		cmd_profile_usage(cdev);
		return VMM_EFAIL;
	}

	if (argc == 3) {
		param = argv[2];
	}

	while (command[index].name) {
		if (vmm_strcmp(argv[1], command[index].name) == 0) {
			return command[index].function(cdev, param);
		}
		index++;
	}

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
	count_array =
	    vmm_malloc(sizeof(struct count_record) * kallsyms_num_syms);

	if (count_array == NULL) {
		return VMM_EFAIL;
	}

	return vmm_cmdmgr_register_cmd(&cmd_profile);
}

static void __exit cmd_profile_exit(void)
{
	vmm_cmdmgr_unregister_cmd(&cmd_profile);

	vmm_free(count_array);

	count_array = NULL;
}

VMM_DECLARE_MODULE(MODULE_NAME,
			MODULE_AUTHOR,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
