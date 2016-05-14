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
 * @file mutex8.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief mutex8 test implementation
 *
 * This test verifies the mutex release on thread destroy, by destroying
 * a thread having a mutex on which multiple threads are blocking, and
 * checking that all three are woken up.
 *
 * This source has been largely adapted from Atomthreads Sources:
 * <atomthreads_source>/tests/mutex8.c
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

#define MODULE_DESC			"mutex8 test"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(WBOXTEST_IPRIORITY+1)
#define MODULE_INIT			mutex8_init
#define MODULE_EXIT			mutex8_exit

/* Number of threads */
#define NUM_THREADS			4

/* Sleep delay in milliseconds */
#define SLEEP_MSECS			(VMM_THREAD_DEF_TIME_SLICE/1000000ULL)

/* Global data */
static struct vmm_thread *workers[NUM_THREADS];
static DEFINE_MUTEX(mutex1);
static volatile int shared_data[NUM_THREADS];

static int mutex8_worker_thread_main(void *data)
{
	/* Pull out thread ID */
	int thread_id = (int)(unsigned long)data;

	/* Acquire mutex */
	vmm_mutex_lock(&mutex1);

	/*
	 * Set shared_data to signify that
	 * we think we have the mutex
	 */
	shared_data[thread_id] = 1;

	/* Release mutex */
	if (thread_id == 0) {
		goto skip_unlock;
	}
	vmm_mutex_unlock(&mutex1);

skip_unlock:
	/* Wait indefinetly here. */
	while (1) ;

	return 0;
}

static int mutex8_do_test(struct vmm_chardev *cdev)
{
	int failures = 0;

	/* Initialise the shared_data to zero */
	shared_data[0] = 0;
	shared_data[1] = 0;
	shared_data[2] = 0;
	shared_data[3] = 0;

	/* Start worker0 */
	vmm_threads_start(workers[0]);

	/*
	 * The worker0 thread has now been started and should take ownership
	 * of the mutex. We wait a while and check that shared_data has been
	 * modified, which proves to us that the thread has taken the mutex.
	 */
	vmm_msleep(SLEEP_MSECS*10);

	/* Check shared data for worker0. It should be one. */
	if (shared_data[0] != 1) {
		vmm_cprintf(cdev, "error: worker0 shared data unmodified\n");
		failures++;
	}

	/* Start worker1, worker2, and worker3 */
	vmm_threads_start(workers[1]);
	vmm_threads_start(workers[2]);
	vmm_threads_start(workers[3]);

	/* Wait for worker1, worker2, and worker3 to block */
	vmm_msleep(SLEEP_MSECS*10);

	/* Destroy worker0 thread so that mutex is automatically released */
	vmm_threads_destroy(workers[0]);
	workers[0] = NULL;

	/* Wait for worker1, worker2, and worker3 to update shared data */
	vmm_msleep(SLEEP_MSECS*10);

	/* Check shared data for worker1. It should be one. */
	if (shared_data[1] != 1) {
		vmm_cprintf(cdev, "error: worker1 shared data unmodified\n");
		failures++;
	}

	/* Check shared data for worker2. It should be one. */
	if (shared_data[2] != 1) {
		vmm_cprintf(cdev, "error: worker2 shared data unmodified\n");
		failures++;
	}

	/* Check shared data for worker3. It should be one. */
	if (shared_data[3] != 1) {
		vmm_cprintf(cdev, "error: worker3 shared data unmodified\n");
		failures++;
	}

	/* Stop worker1, worker2, and worker3 */
	vmm_threads_stop(workers[1]);
	vmm_threads_stop(workers[2]);
	vmm_threads_stop(workers[3]);

	return (failures) ? VMM_EFAIL : 0;
}

static int mutex8_run(struct wboxtest *test, struct vmm_chardev *cdev,
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
			     "mutex8_worker%d", i);
		workers[i] = vmm_threads_create(wname,
						mutex8_worker_thread_main,
						(void *)(unsigned long)i,
						current_priority,
						VMM_THREAD_DEF_TIME_SLICE);
		if (workers[i] == NULL) {
			ret = VMM_EFAIL;
			goto destroy_workers;
		}
	}

	/* Do the test */
	ret = mutex8_do_test(cdev);

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

static struct wboxtest mutex8 = {
	.name = "mutex8",
	.run = mutex8_run,
};

static int __init mutex8_init(void)
{
	return wboxtest_register("threads", &mutex8);
}

static void __exit mutex8_exit(void)
{
	wboxtest_unregister(&mutex8);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
