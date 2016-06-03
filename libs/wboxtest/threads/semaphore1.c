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
 * @file semaphore1.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief semaphore1 test implementation
 *
 * This test exercises blocking on a semaphore. We do this by creating
 * a worker thread that will down s1 semaphore and up s2 semaphore. The
 * worker will block on s1 and when woken-up will up s2.
 */

#include <vmm_error.h>
#include <vmm_delay.h>
#include <vmm_smp.h>
#include <vmm_stdio.h>
#include <vmm_scheduler.h>
#include <vmm_threads.h>
#include <vmm_semaphore.h>
#include <vmm_modules.h>
#include <libs/stringlib.h>
#include <libs/wboxtest.h>

#define MODULE_DESC			"semaphore1 test"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(WBOXTEST_IPRIORITY+1)
#define MODULE_INIT			semaphore1_init
#define MODULE_EXIT			semaphore1_exit

/* Number of threads */
#define NUM_THREADS			1

/* Sleep delay in milliseconds */
#define SLEEP_MSECS			(VMM_THREAD_DEF_TIME_SLICE/1000000ULL)

/* Global data */
static struct vmm_thread *workers[NUM_THREADS];
static DEFINE_SEMAPHORE(s1, 1, 1);
static DEFINE_SEMAPHORE(s2, 1, 0);

static int semaphore1_worker_thread_main(void *data)
{
	int rc;

	rc = vmm_semaphore_down(&s1);
	if (rc) {
		return rc;
	}

	rc = vmm_semaphore_up(&s2);
	if (rc) {
		return rc;
	}

	while (1) {
		vmm_msleep(SLEEP_MSECS);
	}

	return 0;
}

static int semaphore1_do_test(struct vmm_chardev *cdev)
{
	int rc, failures = 0;

	/* Initialize semaphores */
	INIT_SEMAPHORE(&s1, 1, 1);
	INIT_SEMAPHORE(&s2, 1, 0);

	/* s1 semaphore should be available */
	if (!vmm_semaphore_avail(&s1)) {
		failures++;
	}

	/* s2 semaphore should not be available */
	if (vmm_semaphore_avail(&s2)) {
		failures++;
	}

	/* Acquire s1 semaphore */
	rc = vmm_semaphore_down(&s1);
	if (rc) {
		return rc;
	}

	/* Start workers */
	vmm_threads_start(workers[0]);

	/* Wait for worker0 block on s1 semaphore */
	vmm_msleep(SLEEP_MSECS * 10);

	/* s2 semaphore should not be available */
	if (vmm_semaphore_avail(&s2)) {
		failures++;
	}

	/* Release s1 semaphore */
	rc = vmm_semaphore_up(&s1);
	if (rc) {
		return rc;
	}

	/* Wait for worker0 wakeup and release s2 semaphore */
	vmm_msleep(SLEEP_MSECS * 10);

	/* s2 semaphore should be available */
	if (!vmm_semaphore_avail(&s2)) {
		failures++;
	}

	/* Stop workers */
	vmm_threads_stop(workers[0]);

	return (failures) ? VMM_EFAIL : 0;
}

static int semaphore1_run(struct wboxtest *test, struct vmm_chardev *cdev,
		      u32 test_hcpu)
{
	int i, ret = VMM_OK;
	char wname[VMM_FIELD_NAME_SIZE];
	u8 current_priority = vmm_scheduler_current_priority();
	const struct vmm_cpumask *cpu_mask = vmm_cpumask_of(test_hcpu);

	/* Initialise global data */
	memset(workers, 0, sizeof(workers));

	/* Create worker threads */
	for (i = 0; i < NUM_THREADS; i++) {
		vmm_snprintf(wname, VMM_FIELD_NAME_SIZE,
			     "semaphore1_worker%d", i);
		workers[i] = vmm_threads_create(wname,
						semaphore1_worker_thread_main,
						(void *)(unsigned long)i,
						current_priority,
						VMM_THREAD_DEF_TIME_SLICE);
		if (workers[i] == NULL) {
			ret = VMM_EFAIL;
			goto destroy_workers;
		}
		vmm_threads_set_affinity(workers[i], cpu_mask);
	}

	/* Do the test */
	ret = semaphore1_do_test(cdev);

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

static struct wboxtest semaphore1 = {
	.name = "semaphore1",
	.run = semaphore1_run,
};

static int __init semaphore1_init(void)
{
	return wboxtest_register("threads", &semaphore1);
}

static void __exit semaphore1_exit(void)
{
	wboxtest_unregister(&semaphore1);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
