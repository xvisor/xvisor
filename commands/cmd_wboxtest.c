/**
 * Copyright (c) 2016 Anup Patel.
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
 * @file cmd_wboxtest.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Command file for white-box testing.
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_version.h>
#include <vmm_threads.h>
#include <vmm_modules.h>
#include <vmm_cmdmgr.h>
#include <libs/stringlib.h>
#include <libs/wboxtest.h>

#define MODULE_DESC			"Command wboxtest"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			cmd_wboxtest_init
#define	MODULE_EXIT			cmd_wboxtest_exit

static void cmd_wboxtest_usage(struct vmm_chardev *cdev)
{
	vmm_cprintf(cdev, "Usage:\n");
	vmm_cprintf(cdev, "   wboxtest help\n");
	vmm_cprintf(cdev, "   wboxtest test_list\n");
	vmm_cprintf(cdev, "   wboxtest group_list\n");
	vmm_cprintf(cdev, "   wboxtest run_all <iterations>\n");
	vmm_cprintf(cdev, "   wboxtest run_tests <iterations> <test0_name>"
			  " <test1_name> ... <testN_name>\n");
	vmm_cprintf(cdev, "   wboxtest run_groups <iterations> <group0_name>"
			  " <group1_name> ... <groupN_name>\n");
}

struct cmd_wboxtest_test_list_args {
	u32 index;
	struct vmm_chardev *cdev;
};

static void cmd_wboxtest_test_list_iter(struct wboxtest *test, void *data)
{
	struct cmd_wboxtest_test_list_args *args = data;

	vmm_cprintf(args->cdev, " %-7d %-35s %-35s\n",
		    args->index++, test->group->name, test->name);
}

static void cmd_wboxtest_test_list(struct vmm_chardev *cdev)
{
	struct cmd_wboxtest_test_list_args args;

	args.index = 0;
	args.cdev = cdev;

	vmm_cprintf(cdev, "----------------------------------------"
		   	  "----------------------------------------\n");
	vmm_cprintf(cdev, " %-7s %-35s %-35s\n",
		   	  "#", "Group Name", "Test Name");
	vmm_cprintf(cdev, "----------------------------------------"
		   	  "----------------------------------------\n");
	wboxtest_iterate(cmd_wboxtest_test_list_iter, &args);
	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");
}

struct cmd_wboxtest_group_list_args {
	u32 index;
	struct vmm_chardev *cdev;
};

static void cmd_wboxtest_group_list_iter(struct wboxtest_group *group, void *data)
{
	struct cmd_wboxtest_group_list_args *args = data;

	vmm_cprintf(args->cdev, " %-7d %-35s %-35d\n",
		    args->index++, group->name, group->test_count);
}

static void cmd_wboxtest_group_list(struct vmm_chardev *cdev)
{
	struct cmd_wboxtest_group_list_args args;

	args.index = 0;
	args.cdev = cdev;

	vmm_cprintf(cdev, "----------------------------------------"
		   	  "----------------------------------------\n");
	vmm_cprintf(cdev, " %-7s %-35s %-35s\n",
		   	  "#", "Group Name", "Test Count");
	vmm_cprintf(cdev, "----------------------------------------"
		   	  "----------------------------------------\n");
	wboxtest_group_iterate(cmd_wboxtest_group_list_iter, &args);
	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");
}

static void cmd_wboxtest_run_all(struct vmm_chardev *cdev, u32 iterations)
{
	wboxtest_run_all(cdev, iterations);
}

static void cmd_wboxtest_run_tests(struct vmm_chardev *cdev, u32 iterations,
				   int test_count, char **test_names)
{
	wboxtest_run_tests(cdev, iterations, test_count, test_names);
}

static void cmd_wboxtest_run_groups(struct vmm_chardev *cdev, u32 iterations,
				    int group_count, char **group_names)
{
	wboxtest_run_groups(cdev, iterations, group_count, group_names);
}

static int cmd_wboxtest_exec(struct vmm_chardev *cdev, int argc, char **argv)
{
	u32 iterations;
	if (argc == 2) {
		if (strcmp(argv[1], "help") == 0) {
			cmd_wboxtest_usage(cdev);
			return VMM_OK;
		} else if (strcmp(argv[1], "test_list") == 0) {
			cmd_wboxtest_test_list(cdev);
			return VMM_OK;
		} else if (strcmp(argv[1], "group_list") == 0) {
			cmd_wboxtest_group_list(cdev);
			return VMM_OK;
		}
	} else if (argc == 3) {
		iterations = strtoul(argv[2], NULL, 10);
		if (strcmp(argv[1], "run_all") == 0) {
			cmd_wboxtest_run_all(cdev, iterations);
			return VMM_OK;
		}
	} else if (argc > 3) {
		iterations = strtoul(argv[2], NULL, 10);
		if (strcmp(argv[1], "run_tests") == 0) {
			cmd_wboxtest_run_tests(cdev, iterations,
						argc - 3, &argv[3]);
			return VMM_OK;
		} else if (strcmp(argv[1], "run_groups") == 0) {
			cmd_wboxtest_run_groups(cdev, iterations,
						argc - 3, &argv[3]);
			return VMM_OK;
		}
	}
	cmd_wboxtest_usage(cdev);
	return VMM_EFAIL;
}

static struct vmm_cmd cmd_wboxtest = {
	.name = "wboxtest",
	.desc = "commands for white-box testing",
	.usage = cmd_wboxtest_usage,
	.exec = cmd_wboxtest_exec,
};

static int __init cmd_wboxtest_init(void)
{
	return vmm_cmdmgr_register_cmd(&cmd_wboxtest);
}

static void __exit cmd_wboxtest_exit(void)
{
	vmm_cmdmgr_unregister_cmd(&cmd_wboxtest);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
