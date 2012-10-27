/*
 * Generic RTC interface.
 * This version contains the part of the user interface to the Real Time Clock
 * service. It is used with both the legacy mc146818 and also  EFI
 * Struct rtc_time and first 12 ioctl by Paul Gortmaker, 1996 - separated out
 * from <linux/mc146818rtc.h> to this file for 2.4 kernels.
 *
 * Copyright (C) 1999 Hewlett-Packard Co.
 * Copyright (C) 1999 Stephane Eranian <eranian@hpl.hp.com>
 */
#ifndef _LINUX_RTC_H_
#define _LINUX_RTC_H_

#include <rtc/vmm_rtclib.h>
#include <rtc/vmm_rtcdev.h>

/* FIXME: interrupt event flags */
#define RTC_IRQF 0x80	/* Any of the following is active */
#define RTC_PF 0x40	/* Periodic interrupt */
#define RTC_AF 0x20	/* Alarm interrupt */
#define RTC_UF 0x10	/* Update interrupt for 1Hz RTC */

/* FIXME: report interrupt event */
#define rtc_update_irq(rtc,count,events)

#define rtc_time		vmm_rtc_time
#define rtc_wkalrm		vmm_rtc_wkalrm
#define rtc_device		vmm_rtcdev

#define rtc_month_days		vmm_rtc_month_days
#define rtc_year_days		vmm_rtc_year_days
#define rtc_valid_tm		vmm_rtc_valid_tm
#define rtc_time_to_tm		vmm_rtc_time_to_tm
#define rtc_tm_to_time		vmm_rtc_tm_to_time

#endif
