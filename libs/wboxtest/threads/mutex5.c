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
 * @file mutex5.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief mutex5 test implementation
 *
 * This tests basic usage of a mutex. Whichever thread holds the
 * mutex can modify the global variable "shared_data".
 *
 * The main thread first takes the mutex, then creates a worker
 * thread. The workder thread should block on the mutex until the
 * main thread releases it. The test checks that the global
 * "shared_data" is not modified by the worker thread until the
 * main thread releases the mutex.
 *
 * This source has been largely adapted from Atomthreads Sources:
 * <atomthreads_source>/tests/mutex5.c
 *
 * For more info visit: http://atomthreads.com
 */

#include <vmm_error.h>
#include <vmm_delay.h>
#include <vmm_mutex.h>
#include <vmm_stdio.h>
#include <vmm_scheduler.h>
#include <vmm_threads.h>
#include <vmm_modules.h>
#include <libs/stringlib.h>
#include <libs/wboxtest.h>

#define MODULE_DESC			"mutex5 test"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(WBOXTEST_IPRIORITY+1)
#define MODULE_INIT			mutex5_init
#define MODULE_EXIT			mutex5_exit

/* Number of threads */
#define NUM_THREADS			1

/* Sleep delay in milliseconds */
#define SLEEP_MSECS			(VMM_THREAD_DEF_TIME_SLICE/1000000ULL)

/* Global data */
static struct vmm_thread *workers[NUM_THREADS];
static DEFINE_MUTEX(mutex1);
static volatile int shared_data;

static int mutex5_worker_thread_main(void *data)
{
	while (1) {
		/* Acquire mutex */
		vmm_mutex_lock(&mutex1);

		/*
		 * Set shared_data to signify that
		 * we think we have the mutex
		 */
		shared_data = 1;

		/* Release mutex */
		vmm_mutex_unlock(&mutex1);
	}

	return 0;
}

static int mutex5_do_test(struct vmm_chardev *cdev)
{
	int i, failures = 0;

	/* Initialise the shared_data to zero */
	shared_data = 0;

	/* Acquire mutex */
	vmm_mutex_lock(&mutex1);

	/* Start worker */
	vmm_threads_start(workers[0]);

	/*
	 * The worker thread has now been thread and should block
	 * on the mutex until we release it. We wait a while and
	 * check that shared_data has not been modified.
	 */
	for (i = 0; i < 4; i++) {
		/*
		 * Sleep for a while to give the worker thread a
		 * chance to modify shared_data, thought it shouldn't
		 * until we release the mutex.
		 */
		vmm_msleep(SLEEP_MSECS*10);

		/*
		 * Check shared data. The worker thread always
		 * sets it to one.
		 */
		if (shared_data != 0) {
			vmm_cprintf(cdev, "error: shared data modified\n");
			failures++;
		}
	}

	/* Release mutex */
	vmm_mutex_unlock(&mutex1);

	/* Check successful so far */
	if (failures == 0) {
		/*
		 * Wait a little while then check that shared_data has
		 * been modified.
		 */
		vmm_msleep(SLEEP_MSECS*10);

		/*
		 * Check shared data. The worker thread always
		 * sets it to one.
		 */
		if (shared_data != 1) {
			vmm_cprintf(cdev, "error: shared data unmodified\n");
			failures++;
		}

		/*
		 * Release and take the mutex again a few times to ensure
		 * that the mutex continues to protect shared_data.
		 */
		for (i = 0; i < 4; i++) {
			/* Acquire mutex */
			vmm_mutex_lock(&mutex1);

			/*
			 * Set shared_data to 0 and wait to ensure that the
			 * worker thread doesn't modify it while we have the
			 * mutex again.
			 */
			shared_data = 0;

			/*
			 * Wait a while to give worker thread potential
			 * to run.
			 */
			vmm_msleep(SLEEP_MSECS*10);

			/*
			 * Check shared data. The worker thread always
			 * sets it to one.
			 */
			if (shared_data != 0) {
				vmm_cprintf(cdev,
					    "error: worker modifying\n");
				failures++;
			}

			/*
			 * Release the mutex, which will allow the worker
			 * thread to wake and start modifying shared_data
			 * again.
			 */
			vmm_mutex_unlock(&mutex1);
		}
	}

	/* Stop worker thread */
	vmm_threads_stop(workers[0]);

	return (failures) ? VMM_EFAIL : 0;
}

static int mutex5_run(struct wboxtest *test, struct vmm_chardev *cdev,
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
			     "mutex5_worker%d", i);
		workers[i] = vmm_threads_create(wname,
						mutex5_worker_thread_main,
						(void *)(unsigned long)i,
						current_priority,
						VMM_THREAD_DEF_TIME_SLICE);
		if (workers[i] == NULL) {
			ret = VMM_EFAIL;
			goto destroy_workers;
		}
	}

	/* Do the test */
	ret = mutex5_do_test(cdev);

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

static struct wboxtest mutex5 = {
	.name = "mutex5",
	.run = mutex5_run,
};

static int __init mutex5_init(void)
{
	return wboxtest_register("threads", &mutex5);
}

static void __exit mutex5_exit(void)
{
	wboxtest_unregister(&mutex5);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
