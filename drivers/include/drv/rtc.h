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
 * @file rtc.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Real-Time Clock Device framework header
 */

#ifndef __DRV_RTC_H_
#define __DRV_RTC_H_

#include <vmm_limits.h>
#include <vmm_types.h>
#include <vmm_devdrv.h>

#define RTC_DEVICE_CLASS_NAME				"rtc"
#define RTC_DEVICE_CLASS_IPRIORITY			1

struct rtc_time {
	int tm_sec;
	int tm_min;
	int tm_hour;
	int tm_mday;
	int tm_mon;
	int tm_year;
	int tm_wday;
	int tm_yday;
};

/**
 * Check if given year is a leap year.
 */
bool rtc_is_leap_year(unsigned int year);

/**
 * The number of days in the month.
 */
unsigned int rtc_month_days(unsigned int month, unsigned int year);

/**
 * The number of days since January 1. (0 to 365)
 */
unsigned int rtc_year_days(unsigned int day, 
				unsigned int month, unsigned int year);

/**
 * Check whether rtc_time instance represents a valid date/time.
 */
bool rtc_valid_tm(struct rtc_time *tm);

/**
 * Convert Gregorian date to seconds since 01-01-1970 00:00:00.
 */
int rtc_tm_to_time(struct rtc_time *tm, unsigned long *time);

/**
 * Convert seconds since 01-01-1970 00:00:00 to Gregorian date.
 */
void rtc_time_to_tm(unsigned long time, struct rtc_time *tm);

/*
 * This data structure is inspired by the EFI (v0.92) wakeup
 * alarm API.
 */
struct rtc_wkalrm {
	unsigned char enabled;	/* 0 = alarm disabled, 1 = alarm enabled */
	unsigned char pending;  /* 0 = alarm not pending, 1 = alarm pending */
	struct rtc_time time;	/* time the alarm is set to */
};

struct rtc_device {
	char name[VMM_FIELD_NAME_SIZE];
	struct vmm_device dev;
	int (*set_time)(struct rtc_device *rdev, struct rtc_time *tm);
	int (*get_time)(struct rtc_device *rdev, struct rtc_time *tm);
	int (*set_alarm)(struct rtc_device *rdev, struct rtc_wkalrm *alrm);
	int (*get_alarm)(struct rtc_device *rdev, struct rtc_wkalrm *alrm);
	int (*alarm_irq_enable)(struct rtc_device *rdev, unsigned int enabled);
	void *priv;
};

/** Set time in a rtc device */
int rtc_device_set_time(struct rtc_device *rdev,
			struct rtc_time *tm);

/** Get time from a rtc device */
int rtc_device_get_time(struct rtc_device *rdev, struct rtc_time *tm);

/** Sync wall-clock time using given rtc device */
int rtc_device_sync_wallclock(struct rtc_device *rdev);

/** Sync rtc device time from current wall-clock time */
int rtc_device_sync_device(struct rtc_device *rdev);

/** Register rtc device to device driver framework */
int rtc_device_register(struct rtc_device *rdev);

/** Unregister rtc device from device driver framework */
int rtc_device_unregister(struct rtc_device *rdev);

/** Find a rtc device in device driver framework */
struct rtc_device *rtc_device_find(const char *name);

/** Iterate over each rtc device */
int rtc_device_iterate(struct rtc_device *start, void *data,
			int (*fn)(struct rtc_device *rdev, void *data));

/** Count number of rtc devices */
u32 rtc_device_count(void);

#endif /* __DRV_RTC_H_ */
