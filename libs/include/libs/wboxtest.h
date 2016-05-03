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
 * @file wboxtest.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief white-box testing library interface
 */

#ifndef __WBOXTEST_H__
#define __WBOXTEST_H__

#include <vmm_limits.h>
#include <vmm_types.h>
#include <libs/list.h>

#define WBOXTEST_IPRIORITY			(1)

struct vmm_chardev;

struct wboxtest_group {
	/* list head */
	struct dlist head;

	/* group name */
	char name[VMM_FIELD_NAME_SIZE];

	/* number of test under this group */
	u32 test_count;

	/* list of tests under this group */
	struct dlist test_list;
};

struct wboxtest {
	/* list head */
	struct dlist head;

	/* group pointer */
	struct wboxtest_group *group;

	/* test name */
	char name[VMM_FIELD_NAME_SIZE];

	/* operations */
	int (*setup) (struct wboxtest *test, struct vmm_chardev *cdev,
		      u32 test_hcpu);
	int (*run) (struct wboxtest *test, struct vmm_chardev *cdev,
		    u32 test_hcpu);
	void (*cleanup) (struct wboxtest *test, struct vmm_chardev *cdev);
};

/** Iterate over each wboxtest group */
void wboxtest_group_iterate(
	void (*iter)(struct wboxtest_group *group, void *data), void *data);

/** Iterate over each wboxtest */
void wboxtest_iterate(
	void (*iter)(struct wboxtest *test, void *data), void *data);

/** Run one or more wboxtest groups */
void wboxtest_run_groups(struct vmm_chardev *cdev, u32 iterations,
			 int group_count, char **group_names);

/** Run one or more wboxtests */
void wboxtest_run_tests(struct vmm_chardev *cdev, u32 iterations,
			int test_count, char **test_names);

/** Run all wboxtests */
void wboxtest_run_all(struct vmm_chardev *cdev, u32 iterations);

/** Register wboxtest */
int wboxtest_register(const char *group_name, struct wboxtest *test);

/** Unregister wboxtest */
void wboxtest_unregister(struct wboxtest *test);

#endif /* __WBOXTEST_H__ */
