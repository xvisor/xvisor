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
 * @file kern1.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief kern1 test implementation
 */

#include <vmm_error.h>
#include <vmm_delay.h>
#include <vmm_stdio.h>
#include <vmm_scheduler.h>
#include <vmm_threads.h>
#include <vmm_modules.h>
#include <libs/stringlib.h>
#include <libs/wboxtest.h>

#define MODULE_DESC			"kern1 test"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(WBOXTEST_IPRIORITY+1)
#define	MODULE_INIT			kern1_init
#define	MODULE_EXIT			kern1_exit

static int dummy_thread_main(void *data)
{
	/* Nothing to do here. */
	return 0;
}

static int kern1_run(struct wboxtest *test, struct vmm_chardev *cdev)
{
	struct vmm_thread *ti;

	ti = vmm_threads_create(NULL, dummy_thread_main, NULL,
				VMM_THREAD_DEF_PRIORITY,
				VMM_THREAD_DEF_TIME_SLICE);
	if (ti != NULL) {
		return VMM_EFAIL;
	}

	ti = vmm_threads_create("dummy", NULL, NULL,
				VMM_THREAD_DEF_PRIORITY,
				VMM_THREAD_DEF_TIME_SLICE);
	if (ti != NULL) {
		return VMM_EFAIL;
	}

	ti = vmm_threads_create("dummy", dummy_thread_main, NULL,
				VMM_VCPU_MAX_PRIORITY+1,
				VMM_THREAD_DEF_TIME_SLICE);
	if (ti != NULL) {
		return VMM_EFAIL;
	}

	ti = vmm_threads_create("dummy", dummy_thread_main, NULL,
				VMM_THREAD_DEF_PRIORITY,
				VMM_THREAD_DEF_TIME_SLICE);
	if (ti == NULL) {
		return VMM_EFAIL;
	}
	vmm_threads_destroy(ti);

	return 0;
}

static struct wboxtest kern1 = {
	.name = "kern1",
	.run = kern1_run,
};

static int __init kern1_init(void)
{
	return wboxtest_register("threads", &kern1);
}

static void __exit kern1_exit(void)
{
	wboxtest_unregister(&kern1);
}

VMM_DECLARE_MODULE(MODULE_DESC, 
			MODULE_AUTHOR, 
			MODULE_LICENSE, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
