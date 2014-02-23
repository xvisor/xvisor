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

static struct vmm_class rtc_class = {
	.name = RTC_DEVICE_CLASS_NAME,
};

int rtc_device_register(struct rtc_device *rdev)
{
	if (!(rdev && rdev->set_time && rdev->get_time)) {
		return VMM_EFAIL;
	}

	vmm_devdrv_initialize_device(&rdev->dev);
	if (strlcpy(rdev->dev.name, rdev->name, sizeof(rdev->dev.name)) >=
	    sizeof(rdev->dev.name)) {
		return VMM_EOVERFLOW;
	}
	rdev->dev.class = &rtc_class;
	vmm_devdrv_set_data(&rdev->dev, rdev);

	return vmm_devdrv_class_register_device(&rtc_class, &rdev->dev);

}
VMM_EXPORT_SYMBOL(rtc_device_register);

int rtc_device_unregister(struct rtc_device *rdev)
{
	if (!rdev) {
		return VMM_EFAIL;
	}

	return vmm_devdrv_class_unregister_device(&rtc_class, &rdev->dev);
}
VMM_EXPORT_SYMBOL(rtc_device_unregister);

struct rtc_device *rtc_device_find(const char *name)
{
	struct vmm_device *dev;

	dev = vmm_devdrv_class_find_device(&rtc_class, name);
	if (!dev) {
		return NULL;
	}

	return vmm_devdrv_get_data(dev);
}
VMM_EXPORT_SYMBOL(rtc_device_find);

struct rtc_device *rtc_device_get(int num)
{
	struct vmm_device *dev;

	dev = vmm_devdrv_class_device(&rtc_class, num);
	if (!dev) {
		return NULL;
	}

	return vmm_devdrv_get_data(dev);
}
VMM_EXPORT_SYMBOL(rtc_device_get);

u32 rtc_device_count(void)
{
	return vmm_devdrv_class_device_count(&rtc_class);
}
VMM_EXPORT_SYMBOL(rtc_device_count);

static int __init rtc_device_init(void)
{
	vmm_printf("Initialize RTC Device Framework\n");

	return vmm_devdrv_register_class(&rtc_class);
}

static void __exit rtc_device_exit(void)
{
	vmm_devdrv_unregister_class(&rtc_class);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
