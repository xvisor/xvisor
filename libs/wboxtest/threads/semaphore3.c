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
 * @file semaphore3.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief semaphore3 test implementation
 *
 * This test exercises automatic wakeup of threads sleeping on a
 * semaphore when any of the thread holding the semaphore is destroyed.
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

#define MODULE_DESC			"semaphore3 test"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(WBOXTEST_IPRIORITY+1)
#define MODULE_INIT			semaphore3_init
#define MODULE_EXIT			semaphore3_exit

/* Number of threads */
#define NUM_THREADS			2

/* Sleep delay in milliseconds */
#define SLEEP_MSECS			(VMM_THREAD_DEF_TIME_SLICE/1000000ULL)

/* Global data */
static struct vmm_thread *workers[NUM_THREADS];
static DEFINE_SEMAPHORE(s1, 3, 3);
static int shared_data[NUM_THREADS];

static int semaphore3_worker_thread_main(void *data)
{
	int rc;
	int i, thread_id = (int)(unsigned long)data;

	for (i = 0; i < 3; i++) {
		rc = vmm_semaphore_down(&s1);
		if (rc) {
			return rc;
		}
	}

	shared_data[thread_id] = 1;

	while (1) {
		vmm_msleep(SLEEP_MSECS);
	}

	return 0;
}

static int semaphore3_do_test(struct vmm_chardev *cdev)
{
	int i, failures = 0;

	/* Clear shared data */
	for (i = 0; i < NUM_THREADS; i++) {
		shared_data[i] = 0;
	}

	/* s1 semaphore should be available */
	if (vmm_semaphore_avail(&s1) != 3) {
		vmm_cprintf(cdev, "error: initial semaphore not available\n");
		failures++;
	}

	/* Start worker0 */
	vmm_threads_start(workers[0]);

	/* Wait for worker0 to acquire s1 semaphore */
	vmm_msleep(SLEEP_MSECS * 10);

	/* Check worker0 shared data */
	if (shared_data[0] != 1) {
		vmm_cprintf(cdev, "error: worker0 shared data not updated\n");
		failures++;
	}

	/* Start worker1 */
	vmm_threads_start(workers[1]);

	/* Wait for worker1 to acquire s1 semaphore */
	vmm_msleep(SLEEP_MSECS * 10);

	/* Check worker1 shared data */
	if (shared_data[1] != 0) {
		vmm_cprintf(cdev, "error: worker1 shared data updated\n");
		failures++;
	}

	/* Destroy worker0 */
	vmm_threads_destroy(workers[0]);
	workers[0] = NULL;

	/* Wait for worker0 to be scheduled out */
	vmm_msleep(SLEEP_MSECS * 10);

	/* Check worker1 shared data */
	if (shared_data[1] != 1) {
		vmm_cprintf(cdev, "error: worker1 shared data not updated\n");
		failures++;
	}

	return (failures) ? VMM_EFAIL : 0;
}

static int semaphore3_run(struct wboxtest *test, struct vmm_chardev *cdev,
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
			     "semaphore3_worker%d", i);
		workers[i] = vmm_threads_create(wname,
						semaphore3_worker_thread_main,
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
	ret = semaphore3_do_test(cdev);

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

static struct wboxtest semaphore3 = {
	.name = "semaphore3",
	.run = semaphore3_run,
};

static int __init semaphore3_init(void)
{
	return wboxtest_register("threads", &semaphore3);
}

static void __exit semaphore3_exit(void)
{
	wboxtest_unregister(&semaphore3);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
