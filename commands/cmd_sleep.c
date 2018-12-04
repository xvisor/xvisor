/**
 * Copyright (c) 2015 Himanshu Chauhan
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
 * @file cmd_sleep.c
 * @author Himanshu Chauhan <hchauhan@xvisor-x86.org>
 * @brief Implementation of sleep command
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_version.h>
#include <vmm_modules.h>
#include <vmm_cmdmgr.h>
#include <vmm_timer.h>
#include <vmm_delay.h>
#include <libs/mathlib.h>

#define MODULE_DESC			"Command sleep"
#define MODULE_AUTHOR			"Himanshu Chauhan"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			cmd_sleep_init
#define	MODULE_EXIT			cmd_sleep_exit

static void cmd_sleep_usage(struct vmm_chardev *cdev)
{
	vmm_cprintf(cdev, "Usage:\n");
	vmm_cprintf(cdev, "   sleep help\n");
	vmm_cprintf(cdev, "   sleep secs <number_of_seconds>\n");
	vmm_cprintf(cdev, "   sleep msecs <number_of_milliseconds>\n");
	vmm_cprintf(cdev, "   sleep usecs <number_of_microseconds>\n");
	vmm_cprintf(cdev, "   sleep test_secs <number_of_iterations> "
			  "<seconds_per_iteration>\n");
	vmm_cprintf(cdev, "   sleep test_msecs <number_of_iterations> "
			  "<milliseconds_per_iteration>\n");
	vmm_cprintf(cdev, "   sleep test_usecs <number_of_iterations> "
			  "<microseconds_per_iteration>\n");
}

enum cmd_sleep_type {
	CMD_SLEEP_TYPE_SECS=0,
	CMD_SLEEP_TYPE_MSECS=1,
	CMD_SLEEP_TYPE_USECS=2,
};

static int cmd_sleep_normal(struct vmm_chardev *cdev,
			    enum cmd_sleep_type type, int val)
{
	if (val <= 0) {
		return VMM_EINVALID;
	}

	switch (type) {
	case CMD_SLEEP_TYPE_SECS:
		vmm_ssleep(val);
		break;
	case CMD_SLEEP_TYPE_MSECS:
		vmm_msleep(val);
		break;
	case CMD_SLEEP_TYPE_USECS:
		vmm_usleep(val);
		break;
	default:
		return VMM_EINVALID;
	};

	return VMM_OK;
}

static int cmd_sleep_test(struct vmm_chardev *cdev,
			  enum cmd_sleep_type type, int iter, int val)
{
	int i;
	u64 delta, avgdelta = 0;
	u64 mult, start_tstamp, end_tstamp;

	if ((iter <= 0) || (val <= 0)) {
		return VMM_EINVALID;
	}

	for (i = 0; i < iter; i++) {
		vmm_cprintf(cdev, "iter=%d ", i);
		switch (type) {
		case CMD_SLEEP_TYPE_SECS:
			start_tstamp = vmm_timer_timestamp();
			vmm_ssleep(val);
			end_tstamp = vmm_timer_timestamp();
			mult = 1000000000;
			break;
		case CMD_SLEEP_TYPE_MSECS:
			start_tstamp = vmm_timer_timestamp();
			vmm_msleep(val);
			end_tstamp = vmm_timer_timestamp();
			mult = 1000000;
			break;
		case CMD_SLEEP_TYPE_USECS:
			start_tstamp = vmm_timer_timestamp();
			vmm_usleep(val);
			end_tstamp = vmm_timer_timestamp();
			mult = 1000;
			break;
		default:
			return VMM_EINVALID;
		};
		delta = (end_tstamp - start_tstamp) - (val * mult);
		avgdelta += delta;
		vmm_cprintf(cdev, "delta %"PRId64" nanoseconds\n", delta);
	}

	avgdelta = udiv64(avgdelta, (u64)iter);
	vmm_cprintf(cdev, "average delta %"PRId64" nanoseconds\n", avgdelta);

	return VMM_OK;
}

static int cmd_sleep_exec(struct vmm_chardev *cdev, int argc, char **argv)
{
	int rc = VMM_EINVALID, iter, val;

	if (argc == 2) {
		if (strcmp(argv[1], "help") == 0) {
			cmd_sleep_usage(cdev);
			rc = VMM_OK;
		}
	} else if (argc == 3) {
		val = atoi(argv[2]);
		if (strcmp(argv[1], "secs") == 0) {
			rc = cmd_sleep_normal(cdev,
					CMD_SLEEP_TYPE_SECS, val);
		} else if (strcmp(argv[1], "msecs") == 0) {
			rc = cmd_sleep_normal(cdev,
					CMD_SLEEP_TYPE_MSECS, val);
		} else if (strcmp(argv[1], "usecs") == 0) {
			rc = cmd_sleep_normal(cdev,
					CMD_SLEEP_TYPE_USECS, val);
		}
	} else if (argc == 4) {
		iter = atoi(argv[2]);
		val = atoi(argv[3]);
		if (strcmp(argv[1], "test_secs") == 0) {
			rc = cmd_sleep_test(cdev,
					CMD_SLEEP_TYPE_SECS, iter, val);
		} else if (strcmp(argv[1], "test_msecs") == 0) {
			rc = cmd_sleep_test(cdev,
					CMD_SLEEP_TYPE_MSECS, iter, val);
		} else if (strcmp(argv[1], "test_usecs") == 0) {
			rc = cmd_sleep_test(cdev,
					CMD_SLEEP_TYPE_USECS, iter, val);
		}
	}

	if (rc) {
		cmd_sleep_usage(cdev);
	}

	return rc;
}

static struct vmm_cmd cmd_sleep = {
	.name = "sleep",
	.desc = "Make the terminal thread sleep for given time",
	.usage = cmd_sleep_usage,
	.exec = cmd_sleep_exec,
};

static int __init cmd_sleep_init(void)
{
	return vmm_cmdmgr_register_cmd(&cmd_sleep);
}

static void __exit cmd_sleep_exit(void)
{
	vmm_cmdmgr_unregister_cmd(&cmd_sleep);
}

VMM_DECLARE_MODULE(MODULE_DESC,
		   MODULE_AUTHOR,
		   MODULE_LICENSE,
		   MODULE_IPRIORITY,
		   MODULE_INIT,
		   MODULE_EXIT);
