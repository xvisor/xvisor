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
 * @file waitqueue3.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief waitqueue3 test implementation
 *
 * This test verifies waitqueue sleep(), wakefirst(), wakeall() APIs
 * by creating four worker threads sleeping on separate waitqueues
 * and only wakeing up when requested.
 */

#include <vmm_error.h>
#include <vmm_delay.h>
#include <vmm_waitqueue.h>
#include <vmm_stdio.h>
#include <vmm_scheduler.h>
#include <vmm_threads.h>
#include <vmm_modules.h>
#include <libs/stringlib.h>
#include <libs/wboxtest.h>

#define MODULE_DESC			"waitqueue3 test"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(WBOXTEST_IPRIORITY+1)
#define MODULE_INIT			waitqueue3_init
#define MODULE_EXIT			waitqueue3_exit

/* Number of threads */
#define NUM_THREADS			4

/* Sleep delay in milliseconds */
#define SLEEP_MSECS			(VMM_THREAD_DEF_TIME_SLICE/1000000ULL)

/* Global data */
static struct vmm_thread *workers[NUM_THREADS];
static DECLARE_WAITQUEUE(wq0, NULL);
static volatile int wake_count;
static volatile int wake_order[NUM_THREADS];

static int waitqueue3_worker_thread_main(void *data)
{
	/* Pull out thread ID */
	int thread_id = (int)(unsigned long)data;

	while (1) {
		/* Sleep on waitqueue */
		vmm_waitqueue_sleep(&wq0);

		/* Update wakeup order */
		wake_order[wake_count++] = thread_id;
	}

	return 0;
}

static int waitqueue3_do_test(struct vmm_chardev *cdev)
{
	int i, w, failures = 0;

	/* Start workers */
	for (w = 0; w < NUM_THREADS; w++) {
		vmm_threads_start(workers[w]);

		/* Wait for worker to sleep on waitqueue */
		vmm_msleep(SLEEP_MSECS*NUM_THREADS);
	}

	/* Try this few times */
	for (i = 0; i < 10; i++) {
		/* Clear wakeup order */
		wake_count = 0;
		for (w = 0; w < NUM_THREADS; w++) {
			wake_order[w] = 0;
		}

		for (w = 0; w < NUM_THREADS; w++) {
			/* Wakeup worker using wakefirst */
			vmm_waitqueue_wakefirst(&wq0);

			/* Wait for worker to update wake order */
			vmm_msleep(SLEEP_MSECS*NUM_THREADS);
		}

		/* Check wakeup order */
		if ((wake_order[0] != 0) ||
		    (wake_order[1] != 1) ||
		    (wake_order[2] != 2) ||
		    (wake_order[3] != 3)) {
			vmm_cprintf(cdev, "error: wake order %d %d %d %d\n",
				    wake_order[0], wake_order[1],
				    wake_order[2], wake_order[3]);
			failures++;
		}
	}

	return (failures) ? VMM_EFAIL : 0;
}

static int waitqueue3_run(struct wboxtest *test, struct vmm_chardev *cdev,
			  u32 test_hcpu)
{
	int i, ret = VMM_OK;
	char wname[VMM_FIELD_NAME_SIZE];
	u8 current_priority = vmm_scheduler_current_priority();

	/* Initialise global data */
	memset(workers, 0, sizeof(workers));

	/* Create worker threads */
	for (i = 0; i < NUM_THREADS; i++) {
		vmm_snprintf(wname, VMM_FIELD_NAME_SIZE,
			     "waitqueue3_worker%d", i);
		workers[i] = vmm_threads_create(wname,
						waitqueue3_worker_thread_main,
						(void *)(unsigned long)i,
						current_priority,
						VMM_THREAD_DEF_TIME_SLICE);
		if (workers[i] == NULL) {
			ret = VMM_EFAIL;
			goto destroy_workers;
		}
	}

	/* Do the test */
	ret = waitqueue3_do_test(cdev);

	/* Destroy worker threads */
destroy_workers:
	for (i = 0; i < NUM_THREADS; i++) {
		if (workers[i]) {
			vmm_threads_destroy(workers[i]);
			workers[i] = NULL;
		}
	}

	return ret;
}

static struct wboxtest waitqueue3 = {
	.name = "waitqueue3",
	.run = waitqueue3_run,
};

static int __init waitqueue3_init(void)
{
	return wboxtest_register("threads", &waitqueue3);
}

static void __exit waitqueue3_exit(void)
{
	wboxtest_unregister(&waitqueue3);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
