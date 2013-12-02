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
 * @file rtc-dev.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Real-Time Clock Device framework source
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_stdio.h>
#include <vmm_modules.h>
#include <vmm_devdrv.h>
#include <vmm_wallclock.h>
#include <libs/list.h>
#include <libs/stringlib.h>
#include <drv/rtc.h>

#define MODULE_DESC			"RTC Device Framework"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		RTC_DEVICE_CLASS_IPRIORITY
#define	MODULE_INIT			rtc_device_init
#define	MODULE_EXIT			rtc_device_exit

int rtc_device_get_time(struct rtc_device *rdev, struct rtc_time *tm)
{
	if (rdev && tm && rdev->get_time) {
		return rdev->get_time(rdev, tm);
	}

	return VMM_EFAIL;
}
VMM_EXPORT_SYMBOL(rtc_device_get_time);

int rtc_device_set_time(struct rtc_device *rdev, struct rtc_time *tm)
{
	if (rdev && rdev->set_time) {
		return rdev->set_time(rdev, tm);
	}

	return VMM_EFAIL;
}
VMM_EXPORT_SYMBOL(rtc_device_set_time);

int rtc_device_sync_wallclock(struct rtc_device *rdev)
{
	int rc;
	struct vmm_timezone tz, utc_tz;
	struct vmm_timeval tv;
	struct rtc_time tm;

	if (!rdev) {
		return VMM_EFAIL;
	}

	if ((rc = rtc_device_get_time(rdev, &tm))) {
		return rc;
	}

	if ((rc = vmm_wallclock_get_timezone(&tz))) {
		return rc;
	}

	utc_tz.tz_minuteswest = 0;
	utc_tz.tz_dsttime = 0;

	tv.tv_sec = vmm_wallclock_mktime(tm.tm_year + 1900, 
					 tm.tm_mon + 1, 
					 tm.tm_mday, 
					 tm.tm_hour, 
					 tm.tm_min, 
					 tm.tm_sec);
	tv.tv_nsec = 0;

	if ((rc = vmm_wallclock_set_timeofday(&tv, &utc_tz))) {
		return rc;
	}

	if ((rc = vmm_wallclock_set_timezone(&tz))) {
		return rc;
	}

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(rtc_device_sync_wallclock);

int rtc_device_sync_device(struct rtc_device *rdev)
{
	int rc;
	struct vmm_timezone tz;
	struct vmm_timeval tv;
	struct rtc_time tm;

	if (!rdev) {
		return VMM_EFAIL;
	}

	if ((rc = vmm_wallclock_get_timeofday(&tv, &tz))) {
		return rc;
	}

	tv.tv_sec -= tz.tz_minuteswest * 60;
	rtc_time_to_tm((unsigned long)tv.tv_sec, &tm);

	if ((rc = rtc_device_set_time(rdev, &tm))) {
		return rc;
	}

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(rtc_device_sync_device);

int rtc_device_register(struct rtc_device *rdev)
{
	int rc;
	struct vmm_classdev *cd;

	if (!(rdev && rdev->set_time && rdev->get_time)) {
		return VMM_EFAIL;
	}

	cd = vmm_zalloc(sizeof(struct vmm_classdev));
	if (!cd) {
		return VMM_EFAIL;
	}

	INIT_LIST_HEAD(&cd->head);

	if (strlcpy(cd->name, rdev->name, sizeof(cd->name)) >=
	    sizeof(cd->name)) {
		rc = VMM_EOVERFLOW;
		goto free_classdev;
	}

	cd->dev = rdev->dev;
	cd->priv = rdev;

	rc = vmm_devdrv_register_classdev(RTC_DEVICE_CLASS_NAME, cd);
	if (rc) {
		goto free_classdev;
	}

	return rc;

free_classdev:
	vmm_free(cd);
	return rc;

}
VMM_EXPORT_SYMBOL(rtc_device_register);

int rtc_device_unregister(struct rtc_device *rdev)
{
	int rc;
	struct vmm_classdev *cd;

	if (!rdev) {
		return VMM_EFAIL;
	}

	cd = vmm_devdrv_find_classdev(RTC_DEVICE_CLASS_NAME, rdev->name);
	if (!cd) {
		return VMM_EFAIL;
	}

	rc = vmm_devdrv_unregister_classdev(RTC_DEVICE_CLASS_NAME, cd);
	if (rc == VMM_OK) {
		vmm_free(cd);
	}

	return rc;
}
VMM_EXPORT_SYMBOL(rtc_device_unregister);

struct rtc_device *rtc_device_find(const char *name)
{
	struct vmm_classdev *cd;

	cd = vmm_devdrv_find_classdev(RTC_DEVICE_CLASS_NAME, name);
	if (!cd) {
		return NULL;
	}

	return cd->priv;
}
VMM_EXPORT_SYMBOL(rtc_device_find);

struct rtc_device *rtc_device_get(int num)
{
	struct vmm_classdev *cd;

	cd = vmm_devdrv_classdev(RTC_DEVICE_CLASS_NAME, num);
	if (!cd) {
		return NULL;
	}

	return cd->priv;
}
VMM_EXPORT_SYMBOL(rtc_device_get);

u32 rtc_device_count(void)
{
	return vmm_devdrv_classdev_count(RTC_DEVICE_CLASS_NAME);
}
VMM_EXPORT_SYMBOL(rtc_device_count);

static int __init rtc_device_init(void)
{
	int rc;
	struct vmm_class *c;

	vmm_printf("Initialize RTC Device Framework\n");

	c = vmm_zalloc(sizeof(struct vmm_class));
	if (!c) {
		return VMM_EFAIL;
	}

	INIT_LIST_HEAD(&c->head);

	if (strlcpy(c->name, RTC_DEVICE_CLASS_NAME, sizeof(c->name)) >=
	    sizeof(c->name)) {
		rc = VMM_EOVERFLOW;
		goto free_class;
	}

	INIT_LIST_HEAD(&c->classdev_list);

	rc = vmm_devdrv_register_class(c);
	if (rc) {
		goto free_class;
	}

	return rc;

free_class:
	vmm_free(c);
	return rc;
}

static void __exit rtc_device_exit(void)
{
	int rc;
	struct vmm_class *c;

	c = vmm_devdrv_find_class(RTC_DEVICE_CLASS_NAME);
	if (!c) {
		return;
	}

	rc = vmm_devdrv_unregister_class(c);
	if (rc) {
		return;
	}

	vmm_free(c);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
