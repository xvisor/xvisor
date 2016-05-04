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
 * @file mutex2.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief mutex2 test implementation
 *
 * This source has been largely adapted from Atomthreads Sources:
 * <atomthreads_source>/tests/mutex2.c
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

#define MODULE_DESC			"mutex2 test"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(WBOXTEST_IPRIORITY+1)
#define MODULE_INIT			mutex2_init
#define MODULE_EXIT			mutex2_exit

/* Number of threads */
#define NUM_THREADS			1

/* Sleep delay in milliseconds */
#define SLEEP_MSECS			10

/* Global data */
static struct vmm_thread *workers[NUM_THREADS];
static DEFINE_MUTEX(m1);
static DEFINE_MUTEX(m2);

static int mutex2_worker_thread_main(void *data)
{
	int rc;

	rc = vmm_mutex_lock(&m2);
	if (rc) {
		return rc;
	}

	while (1) {
		vmm_msleep(SLEEP_MSECS);
	}

	return 0;
}

static int mutex2_do_test(struct vmm_chardev *cdev)
{
	int i, rc, failures = 0;
	u64 timeout;

	/* Start workers */
	vmm_threads_start(workers[0]);

	/* Wait till worker0 acquired m2 */
	while (vmm_mutex_owner(&m2) != workers[0]->tvcpu) {
		vmm_msleep(SLEEP_MSECS);
	}

	/* Try to lock m2 with timeout (This should timeout) */
	timeout = SLEEP_MSECS * 1000000LL;
	rc = vmm_mutex_lock_timeout(&m2, &timeout);
	if (rc != VMM_ETIMEDOUT) {
		vmm_cprintf(cdev, "error: did not get mutex lock timeout\n");
		failures++;
	}

	/* Try to lock m2 using non-blocking API (This should fail) */
	rc = vmm_mutex_trylock(&m2);
	if (rc != 0) {
		vmm_cprintf(cdev, "error: mutex trylock should fail\n");
		failures++;
	}

	/* Try to unlock m2 (This should fail) */
	rc = vmm_mutex_unlock(&m1);
	if (rc == VMM_OK) {
		vmm_cprintf(cdev,
			    "error: mutex unlock on unowned mutex passed\n");
		failures++;
	}

	/* Lock m1 multiple times using blocking API (This should pass) */
	for (i = 0; i < 10; i++) {
		rc = vmm_mutex_lock(&m1);
		if (rc != VMM_OK) {
			vmm_cprintf(cdev,
				    "error: mutex lock failed i=%d\n", i);
			failures++;
		}
	}

	/* Unlock m1 multiple times using blocking API (This should pass) */
	for (i = 0; i < 10; i++) {
		rc = vmm_mutex_unlock(&m1);
		if (rc != VMM_OK) {
			vmm_cprintf(cdev,
				    "error: mutex unlock failed i=%d\n", i);
			failures++;
		}
	}

	/* Unlock m1 one more time (This should fail) */
	rc = vmm_mutex_unlock(&m1);
	if (rc == VMM_OK) {
		vmm_cprintf(cdev, "error: additional mutex unlock passed\n");
		failures++;
	}

	/* Stop workers */
	vmm_threads_stop(workers[0]);

	return (failures) ? VMM_EFAIL : 0;
}

static int mutex2_run(struct wboxtest *test, struct vmm_chardev *cdev,
		      u32 test_hcpu)
{
	int i, ret = VMM_OK;
	char wname[VMM_FIELD_NAME_SIZE];
	const struct vmm_cpumask *cpu_mask = vmm_cpumask_of(test_hcpu);

	/* Initialise global data */
	memset(workers, 0, sizeof(workers));

	/* Create worker threads */
	for (i = 0; i < NUM_THREADS; i++) {
		vmm_snprintf(wname, VMM_FIELD_NAME_SIZE,
			     "mutex2_worker%d", i);
		workers[i] = vmm_threads_create(wname,
						mutex2_worker_thread_main,
						(void *)(unsigned long)i,
						VMM_THREAD_DEF_PRIORITY,
						VMM_THREAD_DEF_TIME_SLICE);
		if (workers[i] == NULL) {
			ret = VMM_EFAIL;
			goto destroy_workers;
		}
		vmm_threads_set_affinity(workers[i], cpu_mask);
	}


	/* Do the test */
	ret = mutex2_do_test(cdev);

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

static struct wboxtest mutex2 = {
	.name = "mutex2",
	.run = mutex2_run,
};

static int __init mutex2_init(void)
{
	return wboxtest_register("threads", &mutex2);
}

static void __exit mutex2_exit(void)
{
	wboxtest_unregister(&mutex2);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
