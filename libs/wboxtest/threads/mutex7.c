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
 * @file mutex7.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief mutex7 test implementation
 *
 * This tests the ownership checks of the mutex library. Only threads
 * which own a mutex can release it. It should not be possible to
 * release a mutex if it is not owned by any thread, is owned by a
 * different thread. We test here that all two cases are handled.
 *
 * This source has been largely adapted from Atomthreads Sources:
 * <atomthreads_source>/tests/mutex7.c
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

#define MODULE_DESC			"mutex7 test"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(WBOXTEST_IPRIORITY+1)
#define MODULE_INIT			mutex7_init
#define MODULE_EXIT			mutex7_exit

/* Number of threads */
#define NUM_THREADS			1

/* Sleep delay in milliseconds */
#define SLEEP_MSECS			(VMM_THREAD_DEF_TIME_SLICE/1000000ULL)

/* Global data */
static struct vmm_thread *workers[NUM_THREADS];
static DEFINE_MUTEX(mutex1);
static volatile int shared_data;

static int mutex7_worker_thread_main(void *data)
{
	/* Acquire mutex */
	vmm_mutex_lock(&mutex1);

	/*
	 * Set shared_data to signify that
	 * we think we have the mutex
	 */
	shared_data = 1;

	/* Wait indefinetly here. */
	while (1) ;

	return 0;
}

static int mutex7_do_test(struct vmm_chardev *cdev)
{
	int rc, failures = 0;

	/* Initialise the shared_data to zero */
	shared_data = 0;

	/* Attempt to release the mutex when not owned by any thread */
	rc = vmm_mutex_unlock(&mutex1);
	if (rc == VMM_OK) {
		vmm_cprintf(cdev, "error: unlocking mutex worked\n");
		failures++;
	}

	/* Start worker */
	vmm_threads_start(workers[0]);

	/*
	 * The worker thread has now been started and should take ownership
	 * of the mutex. We wait a while and check that shared_data has been
	 * modified, which proves to us that the thread has taken the mutex.
	 */
	vmm_msleep(SLEEP_MSECS*10);

	/* Attempt to release the mutex when owned by worker thread */
	rc = vmm_mutex_unlock(&mutex1);
	if (rc == VMM_OK) {
		vmm_cprintf(cdev, "error: unlocking mutex worked\n");
		failures++;
	}

	/* Stop worker thread */
	vmm_threads_stop(workers[0]);

	return (failures) ? VMM_EFAIL : 0;
}

static int mutex7_run(struct wboxtest *test, struct vmm_chardev *cdev,
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
			     "mutex7_worker%d", i);
		workers[i] = vmm_threads_create(wname,
						mutex7_worker_thread_main,
						(void *)(unsigned long)i,
						current_priority,
						VMM_THREAD_DEF_TIME_SLICE);
		if (workers[i] == NULL) {
			ret = VMM_EFAIL;
			goto destroy_workers;
		}
	}

	/* Do the test */
	ret = mutex7_do_test(cdev);

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

static struct wboxtest mutex7 = {
	.name = "mutex7",
	.run = mutex7_run,
};

static int __init mutex7_init(void)
{
	return wboxtest_register("threads", &mutex7);
}

static void __exit mutex7_exit(void)
{
	wboxtest_unregister(&mutex7);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
