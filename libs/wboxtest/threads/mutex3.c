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
 * @file mutex3.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief mutex3 test implementation
 *
 * This source has been largely adapted from Atomthreads Sources:
 * <atomthreads_source>/tests/mutex3.c
 *
 * For more info visit: http://atomthreads.com
 */

#include <vmm_error.h>
#include <vmm_delay.h>
#include <vmm_mutex.h>
#include <vmm_smp.h>
#include <vmm_stdio.h>
#include <vmm_scheduler.h>
#include <vmm_threads.h>
#include <vmm_modules.h>
#include <libs/stringlib.h>
#include <libs/wboxtest.h>

#define MODULE_DESC			"mutex3 test"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(WBOXTEST_IPRIORITY+1)
#define MODULE_INIT			mutex3_init
#define MODULE_EXIT			mutex3_exit

/* Number of threads */
#define NUM_THREADS			4

/* Sleep delay in milliseconds */
#define SLEEP_MSECS			(VMM_THREAD_DEF_TIME_SLICE/1000000ULL)

/* Global data */
static struct vmm_thread *workers[NUM_THREADS];
static DEFINE_MUTEX(wake_mutex);
static volatile int wake_count;
static volatile int wake_order[NUM_THREADS];

static int mutex3_worker_thread_main(void *data)
{
	/* Pull out thread ID */
	int thread_id = (int)(unsigned long)data;

	/* Acquire wake mutex */
	vmm_mutex_lock(&wake_mutex);

        /*
         * Store our thread ID in the array using the current
         * wake_count order. The threads are holding ownership
         * of a mutex here, which provides protection for this
         * global data.
         */
        wake_order[wake_count++] = thread_id;

	/* Release wake mutex */
	vmm_mutex_unlock(&wake_mutex);

	return 0;
}

static int mutex3_do_test(struct vmm_chardev *cdev)
{
	int failures = 0;

	/* Acquire wake mutex */
	vmm_mutex_lock(&wake_mutex);

	/* Initialize wake order and wake count */
	wake_count = 0;
	wake_order[0] = 0;
	wake_order[1] = 0;
	wake_order[2] = 0;
	wake_order[3] = 0;

	/* Start workers */
	vmm_threads_start(workers[0]);
	vmm_threads_start(workers[1]);
	vmm_threads_start(workers[2]);
	vmm_threads_start(workers[3]);
	vmm_msleep(SLEEP_MSECS*40);

	/* Release wake mutex */
	vmm_mutex_unlock(&wake_mutex);
	vmm_msleep(SLEEP_MSECS*40);

	/* Check wakeup order */
	if ((wake_order[0] != 0) ||
	    (wake_order[1] != 1) ||
	    (wake_order[2] != 2) ||
	    (wake_order[3] != 3)) {
		failures++;
	}

	/* Stop workers */
	vmm_threads_stop(workers[3]);
	vmm_threads_stop(workers[2]);
	vmm_threads_stop(workers[1]);
	vmm_threads_stop(workers[0]);

	return (failures) ? VMM_EFAIL : 0;
}

static int mutex3_run(struct wboxtest *test, struct vmm_chardev *cdev,
		      u32 test_hcpu)
{
	int i, ret = VMM_OK;
	char wname[VMM_FIELD_NAME_SIZE];
	u8 current_priority = vmm_scheduler_current_priority();
	const struct vmm_cpumask *cpu_mask = vmm_cpumask_of(test_hcpu);

	/* Ensure we have sufficiently higher priority */
	if ((current_priority - VMM_THREAD_MIN_PRIORITY + 1) < NUM_THREADS) {
		vmm_cprintf(cdev, "Current priority %d non-sufficient to "
			    "create %d threads of lower priority\n",
			    (unsigned int)current_priority, NUM_THREADS);
		return VMM_EINVALID;
	}

	/* Initialise global data */
	memset(workers, 0, sizeof(workers));

	/* Create worker threads */
	for (i = 0; i < NUM_THREADS; i++) {
		vmm_snprintf(wname, VMM_FIELD_NAME_SIZE,
			     "mutex3_worker%d", i);
		workers[i] = vmm_threads_create(wname,
						mutex3_worker_thread_main,
						(void *)(unsigned long)i,
						VMM_THREAD_DEF_PRIORITY - i,
						VMM_THREAD_DEF_TIME_SLICE);
		if (workers[i] == NULL) {
			ret = VMM_EFAIL;
			goto destroy_workers;
		}
		vmm_threads_set_affinity(workers[i], cpu_mask);
	}

	/* Do the test */
	ret = mutex3_do_test(cdev);

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

static struct wboxtest mutex3 = {
	.name = "mutex3",
	.run = mutex3_run,
};

static int __init mutex3_init(void)
{
	return wboxtest_register("threads", &mutex3);
}

static void __exit mutex3_exit(void)
{
	wboxtest_unregister(&mutex3);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
