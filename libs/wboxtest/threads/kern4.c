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
 * @file kern4.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief kern4 test implementation
 *
 * This test verifies vmm_threads_sleep() and vmm_threads_wakeup() APIs
 * by creating four worker threads which sleep using sleep() API and only
 * wakeing up when requested using wakeup() API.
 */

#include <vmm_error.h>
#include <vmm_delay.h>
#include <vmm_stdio.h>
#include <vmm_scheduler.h>
#include <vmm_threads.h>
#include <vmm_modules.h>
#include <libs/stringlib.h>
#include <libs/wboxtest.h>

#define MODULE_DESC			"kern4 test"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(WBOXTEST_IPRIORITY+1)
#define MODULE_INIT			kern4_init
#define MODULE_EXIT			kern4_exit

/* Number of threads */
#define NUM_THREADS			4

/* Sleep delay in milliseconds */
#define SLEEP_MSECS			(VMM_THREAD_DEF_TIME_SLICE/1000000ULL)

/* Global data */
static struct vmm_thread *workers[NUM_THREADS];
static volatile int shared_data[NUM_THREADS];

static int kern4_worker_thread_main(void *data)
{
	/* Pull out thread ID */
	int thread_id = (int)(unsigned long)data;

	while (1) {
		/* Sleep using vmm_threads API */
		vmm_threads_sleep(workers[thread_id]);

		/*
		 * Set shared_data to signify that
		 * we have woke-up.
		 */
		shared_data[thread_id] = 1;
	}

	return 0;
}

static int kern4_do_test(struct vmm_chardev *cdev)
{
	int i, w, failures = 0;

	/* Start workers */
	for (w = 0; w < NUM_THREADS; w++) {
		vmm_threads_start(workers[w]);
	}

	/* Wait for workers to sleep */
	vmm_msleep(SLEEP_MSECS*NUM_THREADS);

	/* Do this few times */
	for (i = 0; i < 10; i++) {
		/* Try wakeup API */
		for (w = 0; w < NUM_THREADS; w++) {
			/* Reset shared_data to zero */
			shared_data[w] = 0;

			/* Wakeup worker using wakefirst */
			vmm_threads_wakeup(workers[w]);

			/* Wait for worker to update shared data */
			vmm_msleep(SLEEP_MSECS*NUM_THREADS);

			/* Check shared data for worker. It should be one. */
			if (shared_data[0] != 1) {
				vmm_cprintf(cdev, "error: i=%d w=%d wakeup"
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

static int kern4_run(struct wboxtest *test, struct vmm_chardev *cdev,
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
			     "kern4_worker%d", i);
		workers[i] = vmm_threads_create(wname,
						kern4_worker_thread_main,
						(void *)(unsigned long)i,
						current_priority,
						VMM_THREAD_DEF_TIME_SLICE);
		if (workers[i] == NULL) {
			ret = VMM_EFAIL;
			goto destroy_workers;
		}
	}

	/* Do the test */
	ret = kern4_do_test(cdev);

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

static struct wboxtest kern4 = {
	.name = "kern4",
	.run = kern4_run,
};

static int __init kern4_init(void)
{
	return wboxtest_register("threads", &kern4);
}

static void __exit kern4_exit(void)
{
	wboxtest_unregister(&kern4);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
