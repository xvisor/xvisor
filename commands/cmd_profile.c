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
 * @version 0.01
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

#include <kallsyms.h>

#define MODULE_VARID			cmd_profile_module
#define MODULE_NAME			"Command profile"
#define MODULE_AUTHOR			"Jean-Christophe Dubois"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			cmd_profile_init
#define	MODULE_EXIT			cmd_profile_exit

/* This function is architecture specific */
extern void vmm_ftrace_stub(unsigned long ip, unsigned long parent_ip);

/* prototype for mcount callback functions */
typedef void (*ftrace_func_t) (unsigned long ip, unsigned long parent_ip);

/* This is used from the __gnu_mcount_nc function in cpu_entry.S */
ftrace_func_t vmm_ftrace_trace_function = vmm_ftrace_stub;

static int dont_trace = 1;

static unsigned long *counter = NULL;

static __notrace void vmm_ftrace_count(unsigned long ip,
				       unsigned long parent_ip)
{
	if (dont_trace)
		return;

	if (counter) {
		unsigned long symbolsize;
		unsigned long offset;
		counter[kallsyms_get_symbol_pos(ip, &symbolsize, &offset)]++;
	}
}

void cmd_profile_usage(vmm_chardev_t * cdev)
{
	vmm_cprintf(cdev, "Usage: \n");
	vmm_cprintf(cdev, "   profile help\n");
	vmm_cprintf(cdev, "   profile count\n");
}

static int cmd_profile_help(vmm_chardev_t * cdev, int dummy)
{
	cmd_profile_usage(cdev);
	return VMM_OK;
}

static int cmd_profile_count_iterator(void *data, const char *name,
				      unsigned long addr)
{
	int i = kallsyms_get_symbol_pos(addr, NULL, NULL);

	if (counter[i]) {
		vmm_printf("%s %d\n", name, counter[i]);
	}

	return VMM_OK;
}

static int cmd_profile_count(vmm_chardev_t * cdev, int dummy)
{
	static u64 start = 0;
	static u64 stop = 0;

	if (dont_trace == 1) {
		counter = vmm_malloc(sizeof(unsigned int) * kallsyms_num_syms);
		if (counter == NULL) {
			return VMM_EFAIL;
		}
		vmm_memset(counter, 0,
			   sizeof(unsigned int) * kallsyms_num_syms);
		vmm_printf("Start tracing\n");
		start = vmm_timer_timestamp();
		vmm_ftrace_trace_function = vmm_ftrace_count;
		dont_trace = 0;
	} else {
		vmm_ftrace_trace_function = vmm_ftrace_stub;
		dont_trace = 1;
		stop = vmm_timer_timestamp();
		vmm_printf("Stop tracing\n");
		kallsyms_on_each_symbol(cmd_profile_count_iterator, NULL);
		vmm_printf("=========================================\n");
		vmm_printf("trace lasted %u ms\n", (unsigned int)vmm_udiv64(stop-start, 1000*1000));
		vmm_free(counter);
		counter = NULL;
		start = stop = 0;
	}

	return VMM_OK;
}

static const struct {
	char *name;
	int (*function) (vmm_chardev_t *, int);
} const command[] = {
	{"help", cmd_profile_help},
	{"count", cmd_profile_count},
	{NULL, NULL},
};

static int cmd_profile_exec(vmm_chardev_t * cdev, int argc, char **argv)
{
	int id = -1;
	int index = 0;

	if (argc > 3) {
		cmd_profile_usage(cdev);
		return VMM_EFAIL;
	}

	if (argc == 3) {
		id = vmm_str2int(argv[2], 10);
	}

	while (command[index].name) {
		if (vmm_strcmp(argv[1], command[index].name) == 0) {
			return command[index].function(cdev, id);
		}
		index++;
	}

	cmd_profile_usage(cdev);

	return VMM_EFAIL;
}

static vmm_cmd_t cmd_profile = {
	.name = "profile",
	.desc = "profile related commands",
	.usage = cmd_profile_usage,
	.exec = cmd_profile_exec,
};

static int __init cmd_profile_init(void)
{
	return vmm_cmdmgr_register_cmd(&cmd_profile);
}

static void cmd_profile_exit(void)
{
	vmm_cmdmgr_unregister_cmd(&cmd_profile);
}

VMM_DECLARE_MODULE(MODULE_VARID,
		   MODULE_NAME,
		   MODULE_AUTHOR, 
		   MODULE_IPRIORITY, 
		   MODULE_INIT, 
		   MODULE_EXIT);
