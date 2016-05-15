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
 * @file waitqueue1.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief waitqueue1 test implementation
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

#define MODULE_DESC			"waitqueue1 test"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(WBOXTEST_IPRIORITY+1)
#define MODULE_INIT			waitqueue1_init
#define MODULE_EXIT			waitqueue1_exit

/* Number of threads */
#define NUM_THREADS			4

/* Sleep delay in milliseconds */
#define SLEEP_MSECS			(VMM_THREAD_DEF_TIME_SLICE/1000000ULL)

/* Global data */
static struct vmm_thread *workers[NUM_THREADS];
static DECLARE_WAITQUEUE(wq0, NULL);
static DECLARE_WAITQUEUE(wq1, NULL);
static DECLARE_WAITQUEUE(wq2, NULL);
static DECLARE_WAITQUEUE(wq3, NULL);
static volatile int shared_data[NUM_THREADS];

static struct vmm_waitqueue *waitqueue1_wq(int thread_id)
{
	switch (thread_id) {
	case 0:
		return &wq0;
	case 1:
		return &wq1;
	case 2:
		return &wq2;
	case 3:
		return &wq3;
	};

	return NULL;
}

static int waitqueue1_worker_thread_main(void *data)
{
	/* Pull out thread ID */
	int thread_id = (int)(unsigned long)data;
	struct vmm_waitqueue *wq = waitqueue1_wq(thread_id);

	if (!wq) {
		goto done;
	}

	while (1) {
		/* Sleep on waitqueue */
		vmm_waitqueue_sleep(wq);

		/*
		 * Set shared_data to signify that
		 * we have woke-up.
		 */
		shared_data[thread_id] = 1;
	}

done:
	return 0;
}

static int waitqueue1_do_test(struct vmm_chardev *cdev)
{
	int i, w, failures = 0;
	struct vmm_waitqueue *wq;

	/* Start workers */
	for (w = 0; w < NUM_THREADS; w++) {
		vmm_threads_start(workers[w]);
	}

	/* Wait for workers to sleep on waitqueue */
	vmm_msleep(SLEEP_MSECS*NUM_THREADS);

	/* Do this few times to stress waitqueue */
	for (i = 0; i < 10; i++) {
		/* Try wakefirst API */
		for (w = 0; w < NUM_THREADS; w++) {
			/* Get waitqueue pointer */
			wq = waitqueue1_wq(w);

			/* Reset shared_data to zero */
			shared_data[w] = 0;

			/* Wakeup worker using wakefirst */
			vmm_waitqueue_wakefirst(wq);

			/* Wait for worker to update shared data */
			vmm_msleep(SLEEP_MSECS*NUM_THREADS);

			/* Check shared data for worker. It should be one. */
			if (shared_data[0] != 1) {
				vmm_cprintf(cdev, "error: i=%d w=%d wakefirst"
					    "shared data unmodified\n", i, w);
				failures++;
			}
		}

		/* Try wakeall API */
		for (w = 0; w < NUM_THREADS; w++) {
			/* Get waitqueue pointer */
			wq = waitqueue1_wq(w);

			/* Reset shared_data to zero */
			shared_data[w] = 0;

			/* Wakeup worker using wakeall */
			vmm_waitqueue_wakeall(wq);

			/* Wait for worker to update shared data */
			vmm_msleep(SLEEP_MSECS*NUM_THREADS);

			/* Check shared data for worker. It should be one. */
			if (shared_data[0] != 1) {
				vmm_cprintf(cdev, "error: i=%d w=%d wakeall"
					    "shared data unmodified\n", i, w);
				failures++;
			}
		}
	}

	/*
	 * We don't stop workers here instead we let them block and
	 * destroyed later.
	 */
	vmm_msleep(SLEEP_MSECS*NUM_THREADS);

	return (failures) ? VMM_EFAIL : 0;
}

static int waitqueue1_run(struct wboxtest *test, struct vmm_chardev *cdev,
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
			     "waitqueue1_worker%d", i);
		workers[i] = vmm_threads_create(wname,
						waitqueue1_worker_thread_main,
						(void *)(unsigned long)i,
						current_priority,
						VMM_THREAD_DEF_TIME_SLICE);
		if (workers[i] == NULL) {
			ret = VMM_EFAIL;
			goto destroy_workers;
		}
	}

	/* Do the test */
	ret = waitqueue1_do_test(cdev);

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

static struct wboxtest waitqueue1 = {
	.name = "waitqueue1",
	.run = waitqueue1_run,
};

static int __init waitqueue1_init(void)
{
	return wboxtest_register("threads", &waitqueue1);
}

static void __exit waitqueue1_exit(void)
{
	wboxtest_unregister(&waitqueue1);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
