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
 * @file semaphore4.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief semaphore4 test implementation
 *
 * This test exercises the fact that semaphore acquired by one thread
 * can be released by another thread because semaphore does not honour
 * ownership like mutex does.
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

#define MODULE_DESC			"semaphore4 test"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(WBOXTEST_IPRIORITY+1)
#define MODULE_INIT			semaphore4_init
#define MODULE_EXIT			semaphore4_exit

/* Number of threads */
#define NUM_THREADS			1

/* Sleep delay in milliseconds */
#define SLEEP_MSECS			(VMM_THREAD_DEF_TIME_SLICE/1000000ULL)

/* Global data */
static struct vmm_thread *workers[NUM_THREADS];
static DEFINE_SEMAPHORE(s1, 3, 3);
static int shared_data[NUM_THREADS];

static int semaphore4_worker_thread_main(void *data)
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

static int semaphore4_do_test(struct vmm_chardev *cdev)
{
	int i, rc, failures = 0;

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

	/* s1 semaphore should not be available */
	if (vmm_semaphore_avail(&s1) != 0) {
		vmm_cprintf(cdev, "error: semaphore available\n");
		failures++;
	}

	/* Release s1 acquired by worker0 */
	for (i = 0; i < 3; i++) {
		rc = vmm_semaphore_up(&s1);
		if (rc) {
			vmm_cprintf(cdev, "error: semaphore not released\n");
			failures++;
		}
	}

	/* s1 semaphore should be available */
	if (vmm_semaphore_avail(&s1) != 3) {
		vmm_cprintf(cdev, "error: semaphore not available\n");
		failures++;
	}

	return (failures) ? VMM_EFAIL : 0;
}

static int semaphore4_run(struct wboxtest *test, struct vmm_chardev *cdev,
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
			     "semaphore4_worker%d", i);
		workers[i] = vmm_threads_create(wname,
						semaphore4_worker_thread_main,
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
	ret = semaphore4_do_test(cdev);

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

static struct wboxtest semaphore4 = {
	.name = "semaphore4",
	.run = semaphore4_run,
};

static int __init semaphore4_init(void)
{
	return wboxtest_register("threads", &semaphore4);
}

static void __exit semaphore4_exit(void)
{
	wboxtest_unregister(&semaphore4);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
