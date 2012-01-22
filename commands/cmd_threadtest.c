/**
 * Copyright (c) 2011 Sanjeev Pandita.
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
 * @file cmd_threadtest.c
 * @version 0.01
 * @author Sanjeev Pandita (san.pandita@gmail.com)
 * @author Anup Patel (anup@brainfault.org)
 * @brief Command for testing threading & locking related features
 */

#include <vmm_error.h>
#include <vmm_math.h>
#include <vmm_string.h>
#include <vmm_stdio.h>
#include <vmm_devtree.h>
#include <vmm_threads.h>
#include <vmm_completion.h>
#include <vmm_modules.h>
#include <vmm_cmdmgr.h>

#define MODULE_VARID			cmd_threadtest_module
#define MODULE_NAME			"Thread Test Command"
#define MODULE_AUTHOR			"Sanjeev Pandita"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			cmd_threadtest_init
#define	MODULE_EXIT			cmd_threadtest_exit

void cmd_threadtest_usage(struct vmm_chardev *cdev)
{
	vmm_cprintf(cdev,"Usage: \n");
	vmm_cprintf(cdev,"   threadtest help\n");
	vmm_cprintf(cdev,"   threadtest list\n");
	vmm_cprintf(cdev,"   threadtest exec <test_id>\n");
}

typedef int (*threadtest_testfunc_t) (struct vmm_chardev * cdev);

struct threadtest_testcase {
	threadtest_testfunc_t func;
	char desc[64];
};

struct test1_thread_data {
	struct vmm_completion cmpl;
	bool start;
	u32 counter;
	u32 limit;
};

static int test1_thread_main(void *data)
{
	struct test1_thread_data * d = data;

	while (!d->start) ;

	while (d->counter < d->limit) {
		d->counter++;
	}

	vmm_completion_complete_all(&d->cmpl);

	return VMM_OK;
}

static int threadtest_test1(struct vmm_chardev * cdev)
{
	int rc;
	struct vmm_thread * t = NULL;
	struct test1_thread_data d;

	INIT_COMPLETION(&d.cmpl);

	d.start = FALSE;
	d.counter = 0x0;
	d.limit = 0x10000;

	t = vmm_threads_create("test1_thread", 
				test1_thread_main,
				&d,
				VMM_THREAD_DEF_PRIORITY,
				VMM_THREAD_DEF_TIME_SLICE);
	if (!t) {
		return VMM_EFAIL;
	}

	if ((rc = vmm_threads_start(t))) {
		return rc;
	}

	d.start = TRUE;
	vmm_completion_wait(&d.cmpl);

	if ((rc = vmm_threads_destroy(t))) {
		return rc;
	}

	if (d.counter == d.limit) {
		return VMM_OK;
	}

	return VMM_EFAIL;
}

static int threadtest_test2(struct vmm_chardev * cdev)
{
	return VMM_OK;
}

static struct threadtest_testcase testcases[] = {
	{ threadtest_test1, "First Test"},
	{ threadtest_test2, "Second Test"}
};

void cmd_threadtest_list(struct vmm_chardev *cdev)
{
	int i, testcount = 0;

	testcount = vmm_udiv32(sizeof(testcases), 
				sizeof(struct threadtest_testcase));

	for (i = 0; i < testcount; i++) {
		vmm_cprintf(cdev, "%4d %-64s\n", i, testcases[i].desc);
	}
}

int cmd_threadtest_exec(struct vmm_chardev *cdev, int argc, char **argv)
{
	int i, rc, testid = 0, testcount = 0;	
	if ((argc > 1) && (argc < 4)) {
                if (vmm_strcmp(argv[1], "help") == 0) {
                        cmd_threadtest_usage(cdev);
                        return VMM_OK;
                } else if (vmm_strcmp(argv[1], "list") == 0) {
                        cmd_threadtest_list(cdev);
                        return VMM_OK;
		} else if ((vmm_strcmp(argv[1], "exec") == 0) && (3 <= argc)) {
			testid = vmm_str2int(argv[2], 10);
			testcount = vmm_udiv32(sizeof(testcases), 
					sizeof(struct threadtest_testcase));
			if (testid == -1) {
				for (i = 0; i < testcount; i++) {
					vmm_cprintf(cdev, 
						"=== Start Test %d ===\n", i);
					if ((rc = testcases[i].func(cdev))) {
						vmm_cprintf(cdev, "FAILED "
							"(Error %d)\n", rc);
					} else {
						vmm_cprintf(cdev, "SUCCESS\n");
					}
					vmm_cprintf(cdev, 
						"=== End Test %d ===\n", i);
				}
			} else if ((-1 < testid) && (testid < testcount)) {
				vmm_cprintf(cdev, 
					"=== Start Test %d ===\n", testid);
				if ((rc = testcases[testid].func(cdev))) {
					vmm_cprintf(cdev, "FAILED "
							"(Error %d)\n", rc);
				} else {
					vmm_cprintf(cdev, "SUCCESS\n");
				}
				vmm_cprintf(cdev, 
					"=== End Test %d ===\n", testid);
			}
			return VMM_OK;
                }
	}
	cmd_threadtest_usage(cdev);
	return VMM_EFAIL;
}

static struct vmm_cmd cmd_threadtest = {
	.name = "threadtest",
	.desc = "Thread Test Command",
	.usage = cmd_threadtest_usage,
	.exec = cmd_threadtest_exec,
};

static int __init cmd_threadtest_init(void)
{
	return vmm_cmdmgr_register_cmd(&cmd_threadtest);
}

static void cmd_threadtest_exit(void)
{
	vmm_cmdmgr_unregister_cmd(&cmd_threadtest);
}

VMM_DECLARE_MODULE(MODULE_VARID, 
		   MODULE_NAME, 
		   MODULE_AUTHOR, 
		   MODULE_IPRIORITY, 
		   MODULE_INIT, 
		   MODULE_EXIT);
