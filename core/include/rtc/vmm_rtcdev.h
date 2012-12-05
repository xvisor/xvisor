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
 * @file vmm_rtcdev.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Real-Time Clock Device framework header
 */

#ifndef __VMM_RTCDEV_H_
#define __VMM_RTCDEV_H_

#include <vmm_types.h>
#include <vmm_spinlocks.h>
#include <vmm_devdrv.h>
#include <rtc/vmm_rtclib.h>

#define VMM_RTCDEV_CLASS_NAME				"rtc"
#define VMM_RTCDEV_CLASS_IPRIORITY			1

/*
 * This data structure is inspired by the EFI (v0.92) wakeup
 * alarm API.
 */
struct vmm_rtc_wkalrm {
	unsigned char enabled;	/* 0 = alarm disabled, 1 = alarm enabled */
	unsigned char pending;  /* 0 = alarm not pending, 1 = alarm pending */
	struct vmm_rtc_time time;	/* time the alarm is set to */
};

struct vmm_rtcdev;

#define VMM_RTCDEV_NAME_SIZE				32

struct vmm_rtcdev {
	char name[VMM_RTCDEV_NAME_SIZE];
	struct vmm_device *dev;
	int (*set_time)(struct vmm_rtcdev *rdev, struct vmm_rtc_time *tm);
	int (*get_time)(struct vmm_rtcdev *rdev, struct vmm_rtc_time *tm);
	int (*set_alarm)(struct vmm_rtcdev *rdev, struct vmm_rtc_wkalrm *alrm);
	int (*get_alarm)(struct vmm_rtcdev *rdev, struct vmm_rtc_wkalrm *alrm);
	int (*alarm_irq_enable)(struct vmm_rtcdev *rdev, unsigned int enabled);
	void *priv;
};

/** Set time in a rtc device */
int vmm_rtcdev_set_time(struct vmm_rtcdev *rdev,
			struct vmm_rtc_time *tm);

/** Get time from a rtc device */
int vmm_rtcdev_get_time(struct vmm_rtcdev *rdev,
			struct vmm_rtc_time *tm);

/** Sync wall-clock time using given rtc device */
int vmm_rtcdev_sync_wallclock(struct vmm_rtcdev *rdev);

/** Sync rtc device time from current wall-clock time */
int vmm_rtcdev_sync_device(struct vmm_rtcdev *rdev);

/** Register rtc device to device driver framework */
int vmm_rtcdev_register(struct vmm_rtcdev *rdev);

/** Unregister rtc device from device driver framework */
int vmm_rtcdev_unregister(struct vmm_rtcdev *rdev);

/** Find a rtc device in device driver framework */
struct vmm_rtcdev *vmm_rtcdev_find(const char *name);

/** Get rtc device with given number */
struct vmm_rtcdev *vmm_rtcdev_get(int num);

/** Count number of rtc devices */
u32 vmm_rtcdev_count(void);

#endif /* __VMM_RTCDEV_H_ */
