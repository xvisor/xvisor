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
 * @file mutex9.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief mutex9 test implementation
 *
 * This tests timeouts on a mutex. We make a thread block with timeout
 * on a mutex, and test that sufficient time has actually passed as
 * was requested by the timeout parameter.
 *
 * The main thread creates a worker thread which will immediately take
 * ownership of the mutex. The test checks that the correct timeout
 * occurs when the main thread blocks on the mutex which is already
 * owned (by the worker thread).
 *
 * This source has been largely adapted from Atomthreads Sources:
 * <atomthreads_source>/tests/mutex9.c
 *
 * For more info visit: http://atomthreads.com
 */

#include <vmm_error.h>
#include <vmm_delay.h>
#include <vmm_mutex.h>
#include <vmm_timer.h>
#include <vmm_stdio.h>
#include <vmm_scheduler.h>
#include <vmm_threads.h>
#include <vmm_modules.h>
#include <libs/stringlib.h>
#include <libs/wboxtest.h>

#define MODULE_DESC			"mutex9 test"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(WBOXTEST_IPRIORITY+1)
#define MODULE_INIT			mutex9_init
#define MODULE_EXIT			mutex9_exit

/* Number of threads */
#define NUM_THREADS			1

/* Sleep delay in milliseconds */
#define SLEEP_MSECS			(VMM_THREAD_DEF_TIME_SLICE/1000000ULL)

/* Global data */
static struct vmm_thread *workers[NUM_THREADS];
static DEFINE_MUTEX(mutex1);
static volatile int shared_data;

static int mutex9_worker_thread_main(void *data)
{
	/* Acquire mutex */
	vmm_mutex_lock(&mutex1);

	/*
	 * Set shared_data to signify that
	 * we think we have the mutex
	 */
	shared_data = 1;

	/* Wait indefinetly here. */
	while (1) {
		vmm_msleep(SLEEP_MSECS);
	}

	return 0;
}

static int mutex9_do_test(struct vmm_chardev *cdev)
{
	u64 i, timeout, etimeout, tstamp;
	int rc, failures = 0;

	/* Initialise the shared_data to zero */
	shared_data = 0;

	/* Start worker */
	vmm_threads_start(workers[0]);

	/*
	 * The worker thread has now been started and should take ownership
	 * of the mutex. We wait a while and check that shared_data has been
	 * modified, which proves to us that the thread has taken the mutex.
	 */
	vmm_msleep(SLEEP_MSECS*10);

	/* Check shared data. It should be one. */
	if (shared_data != 1) {
		vmm_cprintf(cdev, "error: shared data unmodified\n");
		failures++;
	}

	/* Try mutex lock with timeout few times */
	for (i = 1; i <= 10; i++) {
		/* Save current timestamp */
		tstamp = vmm_timer_timestamp();

		/* Lock mutex with timeout */
		etimeout = i * SLEEP_MSECS * 1000000ULL;
		timeout = etimeout;
		rc = vmm_mutex_lock_timeout(&mutex1, &timeout);
		if (rc != VMM_ETIMEDOUT) {
			vmm_cprintf(cdev, "error: did not timeout\n");
			failures++;
		}

		/* Check elapsed time */
		tstamp = vmm_timer_timestamp() - tstamp;
		if (tstamp < etimeout) {
			vmm_cprintf(cdev, "error: time elapsed %"PRIu64
				    " nanosecs instead of %"PRIu64" nanosecs",
				    tstamp, etimeout);
			failures++;
		}
	}

	/* Stop worker thread. */
	vmm_threads_stop(workers[0]);

	return (failures) ? VMM_EFAIL : 0;
}

static int mutex9_run(struct wboxtest *test, struct vmm_chardev *cdev,
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
			     "mutex9_worker%d", i);
		workers[i] = vmm_threads_create(wname,
						mutex9_worker_thread_main,
						(void *)(unsigned long)i,
						current_priority,
						VMM_THREAD_DEF_TIME_SLICE);
		if (workers[i] == NULL) {
			ret = VMM_EFAIL;
			goto destroy_workers;
		}
	}

	/* Do the test */
	ret = mutex9_do_test(cdev);

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

static struct wboxtest mutex9 = {
	.name = "mutex9",
	.run = mutex9_run,
};

static int __init mutex9_init(void)
{
	return wboxtest_register("threads", &mutex9);
}

static void __exit mutex9_exit(void)
{
	wboxtest_unregister(&mutex9);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
