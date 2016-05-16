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
 * @file waitqueue2.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief waitqueue2 test implementation
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
 * <atomthreads_source>/tests/waitqueue2.c
 *
 * For more info visit: http://atomthreads.com
 */

#include <vmm_error.h>
#include <vmm_delay.h>
#include <vmm_waitqueue.h>
#include <vmm_timer.h>
#include <vmm_stdio.h>
#include <vmm_scheduler.h>
#include <vmm_threads.h>
#include <vmm_modules.h>
#include <libs/stringlib.h>
#include <libs/wboxtest.h>

#define MODULE_DESC			"waitqueue2 test"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(WBOXTEST_IPRIORITY+1)
#define MODULE_INIT			waitqueue2_init
#define MODULE_EXIT			waitqueue2_exit

/* Sleep delay in milliseconds */
#define SLEEP_MSECS			(VMM_THREAD_DEF_TIME_SLICE/1000000ULL)

/* Global data */
static DECLARE_WAITQUEUE(wq0, NULL);

static int waitqueue2_do_test(struct vmm_chardev *cdev)
{
	u64 i, timeout, etimeout, tstamp;
	int rc, failures = 0;

	/* Try waitqueue with timeout few times */
	for (i = 1; i <= 10; i++) {
		/* Save current timestamp */
		tstamp = vmm_timer_timestamp();

		/* Lock mutex with timeout */
		etimeout = i * SLEEP_MSECS * 1000000ULL;
		timeout = etimeout;
		rc = vmm_waitqueue_sleep_timeout(&wq0, &timeout);
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

	return (failures) ? VMM_EFAIL : 0;
}

static int waitqueue2_run(struct wboxtest *test, struct vmm_chardev *cdev,
			  u32 test_hcpu)
{
	/* Do the test */
	return waitqueue2_do_test(cdev);
}

static struct wboxtest waitqueue2 = {
	.name = "waitqueue2",
	.run = waitqueue2_run,
};

static int __init waitqueue2_init(void)
{
	return wboxtest_register("threads", &waitqueue2);
}

static void __exit waitqueue2_exit(void)
{
	wboxtest_unregister(&waitqueue2);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
