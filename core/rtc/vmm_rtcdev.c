/**
 * Copyright (c) 2012 Anup Patel.
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
 * @file vmm_rtcdev.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Real-Time Clock Device framework source
 */

#include <vmm_error.h>
#include <vmm_list.h>
#include <vmm_string.h>
#include <vmm_heap.h>
#include <vmm_stdio.h>
#include <vmm_modules.h>
#include <vmm_devdrv.h>
#include <rtc/vmm_rtcdev.h>

#define MODULE_VARID			rtcdev_framework_module
#define MODULE_NAME			"RTC Device Framework"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_IPRIORITY		VMM_RTCDEV_CLASS_IPRIORITY
#define	MODULE_INIT			vmm_rtcdev_init
#define	MODULE_EXIT			vmm_rtcdev_exit

int vmm_rtcdev_get_time(struct vmm_rtcdev * rdev,
			struct vmm_rtc_time * tm)
{
	if (rdev && tm && rdev->get_time) {
		return rdev->get_time(rdev, tm);
	}

	return VMM_EFAIL;
}

int vmm_rtcdev_set_time(struct vmm_rtcdev * rdev,
			struct vmm_rtc_time * tm)
{
	if (rdev && rdev->set_time) {
		return rdev->set_time(rdev, tm);
	}

	return VMM_EFAIL;
}

int vmm_rtcdev_register(struct vmm_rtcdev * rdev)
{
	int rc;
	struct vmm_classdev *cd;

	if (!(rdev && rdev->set_time && rdev->get_time)) {
		return VMM_EFAIL;
	}

	cd = vmm_malloc(sizeof(struct vmm_classdev));
	if (!cd) {
		return VMM_EFAIL;
	}

	vmm_memset(cd, 0, sizeof(struct vmm_classdev));

	INIT_LIST_HEAD(&cd->head);
	vmm_strcpy(cd->name, rdev->name);
	cd->dev = rdev->dev;
	cd->priv = rdev;

	rc = vmm_devdrv_register_classdev(VMM_RTCDEV_CLASS_NAME, cd);
	if (rc != VMM_OK) {
		vmm_free(cd);
	}

	return rc;
}

int vmm_rtcdev_unregister(struct vmm_rtcdev * rdev)
{
	int rc;
	struct vmm_classdev *cd;

	if (!rdev) {
		return VMM_EFAIL;
	}

	cd = vmm_devdrv_find_classdev(VMM_RTCDEV_CLASS_NAME, rdev->name);
	if (!cd) {
		return VMM_EFAIL;
	}

	rc = vmm_devdrv_unregister_classdev(VMM_RTCDEV_CLASS_NAME, cd);
	if (rc == VMM_OK) {
		vmm_free(cd);
	}

	return rc;
}

struct vmm_rtcdev *vmm_rtcdev_find(const char *name)
{
	struct vmm_classdev *cd;

	cd = vmm_devdrv_find_classdev(VMM_RTCDEV_CLASS_NAME, name);
	if (!cd) {
		return NULL;
	}

	return cd->priv;
}

struct vmm_rtcdev *vmm_rtcdev_get(int num)
{
	struct vmm_classdev *cd;

	cd = vmm_devdrv_classdev(VMM_RTCDEV_CLASS_NAME, num);
	if (!cd) {
		return NULL;
	}

	return cd->priv;
}

u32 vmm_rtcdev_count(void)
{
	return vmm_devdrv_classdev_count(VMM_RTCDEV_CLASS_NAME);
}

static int __init vmm_rtcdev_init(void)
{
	int rc;
	struct vmm_class *c;

	vmm_printf("Initialize RTC Device Framework\n");

	c = vmm_malloc(sizeof(struct vmm_class));
	if (!c) {
		return VMM_EFAIL;
	}

	vmm_memset(c, 0, sizeof(struct vmm_class));

	INIT_LIST_HEAD(&c->head);
	vmm_strcpy(c->name, VMM_RTCDEV_CLASS_NAME);
	INIT_LIST_HEAD(&c->classdev_list);

	rc = vmm_devdrv_register_class(c);
	if (rc != VMM_OK) {
		vmm_free(c);
	}

	return rc;
}

static void vmm_rtcdev_exit(void)
{
	int rc;
	struct vmm_class *c;

	c = vmm_devdrv_find_class(VMM_RTCDEV_CLASS_NAME);
	if (!c) {
		return;
	}

	rc = vmm_devdrv_unregister_class(c);
	if (rc) {
		return;
	}

	vmm_free(c);
}

VMM_DECLARE_MODULE(MODULE_VARID,
		   MODULE_NAME,
		   MODULE_AUTHOR,
		   MODULE_IPRIORITY,
		   MODULE_INIT,
		   MODULE_EXIT);
