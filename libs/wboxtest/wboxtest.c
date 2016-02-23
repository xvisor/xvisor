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
 * @file wboxtest.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief white-box testing library implementation
 */

#include <vmm_error.h>
#include <vmm_macros.h>
#include <vmm_heap.h>
#include <vmm_host_vapool.h>
#include <vmm_mutex.h>
#include <vmm_modules.h>
#include <vmm_timer.h>
#include <libs/stringlib.h>
#include <libs/wboxtest.h>

#define MODULE_DESC			"white-box testing library"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		WBOXTEST_IPRIORITY
#define	MODULE_INIT			wboxtest_init
#define	MODULE_EXIT			wboxtest_exit

struct wboxtest_control {
	struct vmm_mutex lock;
	struct dlist group_list;
};

static struct wboxtest_control wtc;

/* Note: Must be called with wtc.lock held */
static struct wboxtest_group *__wboxtest_group_find(const char *group_name)
{
	bool found;
	struct wboxtest_group *group = NULL;

	if (!group_name) {
		return NULL;
	}

	found = FALSE;
	list_for_each_entry(group, &wtc.group_list, head) {
		if (!strncmp(group->name, group_name, sizeof(group->name))) {
			found = TRUE;
			break;
		}
	}

	return (found) ? group : NULL;
}

/* Note: Must be called with wtc.lock held */
static struct wboxtest *__wboxtest_find(struct wboxtest_group *group,
				        const char *name)
{
	bool found;
	struct wboxtest *test = NULL;

	if (!group || !name) {
		return NULL;
	}

	found = FALSE;
	list_for_each_entry(test, &group->test_list, head) {
		if (!strncmp(test->name, name, sizeof(test->name))) {
			found = TRUE;
			break;
		}
	}

	return (found) ? test : NULL;
}

/* Note: Must be called with wtc.lock held */
int __wboxtest_group_add_test(const char *group_name, struct wboxtest *test)
{
	struct wboxtest *t;
	struct wboxtest_group *group;

	if (!group_name || !test || !test->run) {
		return VMM_EINVALID;
	}

	group = __wboxtest_group_find(group_name);
	if (!group) {
		group = vmm_zalloc(sizeof(*group));
		if (!group) {
			return VMM_ENOMEM;
		}

		INIT_LIST_HEAD(&group->head);
		strlcpy(group->name, group_name, sizeof(group->name));
		group->test_count = 0;
		INIT_LIST_HEAD(&group->test_list);

		list_add_tail(&group->head, &wtc.group_list);
	}

	t = __wboxtest_find(group, test->name);
	if (t) {
		return VMM_EEXIST;
	}

	INIT_LIST_HEAD(&test->head);
	test->group = group;

	list_add_tail(&test->head, &group->test_list);
	group->test_count++;

	return VMM_OK;
}

/* Note: Must be called with wtc.lock held */
void __wboxtest_group_del_test(struct wboxtest *test)
{
	struct wboxtest *t;
	struct wboxtest_group *group;

	if (!test || !test->group) {
		return;
	}
	group = test->group;

	t = __wboxtest_find(group, test->name);
	if (!t) {
		return;
	}

	test->group = NULL;

	list_del(&test->head);
	group->test_count--;
	if (!group->test_count) {
		list_del(&group->head);
		vmm_free(group);
	}
}

/* Note: Must be called with wtc.lock held */
int __wboxtest_run_test(struct wboxtest *test,
			struct vmm_chardev *cdev, u32 iterations)
{
	int rc;
	u32 iter, fail_count = 0;
	u64 tstamp = vmm_timer_timestamp();
	u32 vp = vmm_host_vapool_free_page_count();
	virtual_size_t nh = vmm_normal_heap_free_size();
	virtual_size_t dh = vmm_dma_heap_free_size();

	vmm_cprintf(cdev, "wboxtest: test=%s start\n", test->name);

	if (test->setup) {
		rc = test->setup(test, cdev);
		if (rc) {
			vmm_cprintf(cdev, "wboxtest: test=%s setup failed "
				    "(error %d)\n", test->name, rc);
		}
	}

	for (iter = 0; iter < iterations; iter++) {
		rc = test->run(test, cdev);
		if (rc) {
			fail_count++;
		}
		vmm_cprintf(cdev, "wboxtest: test=%s iteration=%d %s"
			    " (error %d)\n", test->name, iter,
			    (rc) ? "failed" : "passed", rc);
	}

	if (test->cleanup)
		test->cleanup(test, cdev);

	vmm_cprintf(cdev, "wboxtest: test=%s vapool leakage %d pages\n",
		    test->name,
		    (unsigned long)(vp - vmm_host_vapool_free_page_count()));
	vmm_cprintf(cdev, "wboxtest: test=%s normal heap leakage %d bytes\n",
		    test->name,
		    (unsigned long)(nh - vmm_normal_heap_free_size()));
	vmm_cprintf(cdev, "wboxtest: test=%s dma heap leakage %d bytes\n",
		    test->name,
		    (unsigned long)(dh - vmm_dma_heap_free_size()));
	vmm_cprintf(cdev, "wboxtest: test=%s time taken %ll nanoseconds\n",
		    test->name, vmm_timer_timestamp() - tstamp);
	vmm_cprintf(cdev, "wboxtest: test=%s failures %d out of %d\n",
		    test->name, fail_count, iterations);
	vmm_cprintf(cdev, "wboxtest: test=%s %s\n",
		    test->name, (fail_count) ? "failed" : "passed");

	return (fail_count) ? VMM_EFAIL : VMM_OK;
}

void wboxtest_group_iterate(
	void (*iter)(struct wboxtest_group *group, void *data), void *data)
{
	struct wboxtest_group *group;

	if (!iter) {
		return;
	}

	vmm_mutex_lock(&wtc.lock);
	list_for_each_entry(group, &wtc.group_list, head) {
		iter(group, data);
	}
	vmm_mutex_unlock(&wtc.lock);
}
VMM_EXPORT_SYMBOL(wboxtest_group_iterate);

void wboxtest_iterate(
	void (*iter)(struct wboxtest *test, void *data), void *data)
{
	struct wboxtest *test;
	struct wboxtest_group *group;

	if (!iter) {
		return;
	}

	vmm_mutex_lock(&wtc.lock);
	list_for_each_entry(group, &wtc.group_list, head) {
		list_for_each_entry(test, &group->test_list, head) {
			iter(test, data);
		}
	}
	vmm_mutex_unlock(&wtc.lock);
}
VMM_EXPORT_SYMBOL(wboxtest_iterate);

struct wboxtest_run_groups_args {
	struct vmm_chardev *cdev;
	u32 iterations;
	const char *group_name;
};

static void wboxtest_run_groups_iter(struct wboxtest *test, void *data)
{
	struct wboxtest_run_groups_args *args = data;
	struct wboxtest_group *group = test->group;

	/* skip if group name does not match */
	if (strncmp(group->name, args->group_name, sizeof(group->name)))
		return;	

	__wboxtest_run_test(test, args->cdev, args->iterations);
}

void wboxtest_run_groups(struct vmm_chardev *cdev, u32 iterations,
			 int group_count, char **group_names)
{
	int g;
	struct wboxtest_run_groups_args args;

	if (!group_names || (group_count <= 0))
		return;

	for (g = 0 ; g < group_count; g++) {
		if (!group_names[g])
			continue;
		args.cdev = cdev;
		args.iterations = iterations;
		args.group_name = group_names[g];
		wboxtest_iterate(wboxtest_run_groups_iter, &args);
	}
}
VMM_EXPORT_SYMBOL(wboxtest_run_groups);

struct wboxtest_run_tests_args {
	struct vmm_chardev *cdev;
	u32 iterations;
	const char *test_name;
};

static void wboxtest_run_tests_iter(struct wboxtest *test, void *data)
{
	struct wboxtest_run_tests_args *args = data;

	/* skip if test name does not match */
	if (strncmp(test->name, args->test_name, sizeof(test->name)))
		return;	

	__wboxtest_run_test(test, args->cdev, args->iterations);
}

void wboxtest_run_tests(struct vmm_chardev *cdev, u32 iterations,
			int test_count, char **test_names)
{
	int t;
	struct wboxtest_run_tests_args args;

	if (!test_names || (test_count <= 0))
		return;

	for (t = 0 ; t < test_count; t++) {
		if (!test_names[t])
			continue;
		args.cdev = cdev;
		args.iterations = iterations;
		args.test_name = test_names[t];
		wboxtest_iterate(wboxtest_run_tests_iter, &args);
	}
}
VMM_EXPORT_SYMBOL(wboxtest_run_tests);

struct wboxtest_run_all_args {
	struct vmm_chardev *cdev;
	u32 iterations;
};

static void wboxtest_run_all_iter(struct wboxtest *test, void *data)
{
	struct wboxtest_run_all_args *args = data;

	__wboxtest_run_test(test, args->cdev, args->iterations);
}

void wboxtest_run_all(struct vmm_chardev *cdev, u32 iterations)
{
	struct wboxtest_run_all_args args;

	args.cdev = cdev;
	args.iterations = iterations;
	wboxtest_iterate(wboxtest_run_all_iter, &args);
}
VMM_EXPORT_SYMBOL(wboxtest_run_all);

int wboxtest_register(const char *group_name, struct wboxtest *test)
{
	int rc;

	vmm_mutex_lock(&wtc.lock);
	rc = __wboxtest_group_add_test(group_name, test);
	vmm_mutex_unlock(&wtc.lock);

	return rc;
}
VMM_EXPORT_SYMBOL(wboxtest_register);

void wboxtest_unregister(struct wboxtest *test)
{
	vmm_mutex_lock(&wtc.lock);
	__wboxtest_group_del_test(test);
	vmm_mutex_unlock(&wtc.lock);
}
VMM_EXPORT_SYMBOL(wboxtest_unregister);

static int __init wboxtest_init(void)
{
	memset(&wtc, 0, sizeof(wtc));

	INIT_MUTEX(&wtc.lock);
	INIT_LIST_HEAD(&wtc.group_list);

	return VMM_OK;
}

static void __exit wboxtest_exit(void)
{
	/* Nothing to do here. */
}

VMM_DECLARE_MODULE(MODULE_DESC, 
			MODULE_AUTHOR, 
			MODULE_LICENSE, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
