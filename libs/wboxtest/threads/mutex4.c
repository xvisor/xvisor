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
 * @file mutex4.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief mutex4 test implementation
 *
 * Stress-tests mutex lock and unlock operations. Four threads are created
 * which are continually lock and unlock the same mutex, with no time delays
 * between each lock/unlock.
 *
 * This source has been largely adapted from Atomthreads Sources:
 * <atomthreads_source>/tests/mutex4.c
 *
 * For more info visit: http://atomthreads.com
 */

#include <vmm_error.h>
#include <vmm_delay.h>
#include <vmm_mutex.h>
#include <vmm_completion.h>
#include <vmm_smp.h>
#include <vmm_stdio.h>
#include <vmm_scheduler.h>
#include <vmm_threads.h>
#include <vmm_modules.h>
#include <libs/stringlib.h>
#include <libs/wboxtest.h>

#define MODULE_DESC			"mutex4 test"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(WBOXTEST_IPRIORITY+1)
#define MODULE_INIT			mutex4_init
#define MODULE_EXIT			mutex4_exit

/* Number of threads */
#define NUM_THREADS			4

/* Number of loops for stress-test */
#define NUM_LOOPS			10000

/* Sleep delay in milliseconds */
#define SLEEP_MSECS			(VMM_THREAD_DEF_TIME_SLICE/1000000ULL)

/* Global data */
static struct vmm_thread *workers[NUM_THREADS];
static DEFINE_MUTEX(mutex1);
static struct vmm_completion work_done;

static int mutex4_worker_thread_main(void *data)
{
	int i;

	for (i = 0; i < NUM_LOOPS; i++) {
		/* Acquire wake mutex */
		vmm_mutex_lock(&mutex1);

		/* Release wake mutex */
		vmm_mutex_unlock(&mutex1);
	}

	/* Signal work done completion */
	vmm_completion_complete(&work_done);

	return 0;
}

static int mutex4_do_test(struct vmm_chardev *cdev)
{
	int done_count = 0;

	/* Initialize work done completion */
	INIT_COMPLETION(&work_done);

	/* Acquire mutex1 */
	vmm_mutex_lock(&mutex1);

	/* Start workers */
	vmm_threads_start(workers[0]);
	vmm_threads_start(workers[1]);
	vmm_threads_start(workers[2]);
	vmm_threads_start(workers[3]);
	vmm_msleep(SLEEP_MSECS*40);

	/* Release mutex1 */
	vmm_mutex_unlock(&mutex1);

	/* Wait for workers to complete */
	do {
		if (done_count == NUM_THREADS) {
			break;
		}

		vmm_completion_wait(&work_done);
		done_count++;
	} while (1);

	/* Stop workers */
	vmm_threads_stop(workers[3]);
	vmm_threads_stop(workers[2]);
	vmm_threads_stop(workers[1]);
	vmm_threads_stop(workers[0]);

	return 0;
}

static int mutex4_run(struct wboxtest *test, struct vmm_chardev *cdev,
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
			     "mutex4_worker%d", i);
		workers[i] = vmm_threads_create(wname,
						mutex4_worker_thread_main,
						(void *)(unsigned long)i,
						current_priority,
						VMM_THREAD_DEF_TIME_SLICE);
		if (workers[i] == NULL) {
			ret = VMM_EFAIL;
			goto destroy_workers;
		}
	}

	/* Do the test */
	ret = mutex4_do_test(cdev);

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

static struct wboxtest mutex4 = {
	.name = "mutex4",
	.run = mutex4_run,
};

static int __init mutex4_init(void)
{
	return wboxtest_register("threads", &mutex4);
}

static void __exit mutex4_exit(void)
{
	wboxtest_unregister(&mutex4);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
