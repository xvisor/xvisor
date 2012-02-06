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
 * @file vmm_rtclib.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Real-Time Clock Library header
 */

#ifndef __VMM_RTCLIB_H_
#define __VMM_RTCLIB_H_

#include <vmm_types.h>

struct vmm_rtc_time {
	int tm_sec;
	int tm_min;
	int tm_hour;
	int tm_mday;
	int tm_mon;
	int tm_year;
	int tm_wday;
	int tm_yday;
	int tm_isdst;
};

/**
 * Check if given year is a leap year.
 */
bool vmm_rtc_is_leap_year(unsigned int year);

/**
 * The number of days in the month.
 */
unsigned int vmm_rtc_month_days(unsigned int month, unsigned int year);

/**
 * The number of days since January 1. (0 to 365)
 */
unsigned int vmm_rtc_year_days(unsigned int day, 
				unsigned int month, unsigned int year);

/**
 * Check whether vmm_rtc_time instance represents a valid date/time.
 */
bool vmm_rtc_valid_tm(struct vmm_rtc_time *tm);

/**
 * Convert Gregorian date to seconds since 01-01-1970 00:00:00.
 */
void vmm_rtc_tm_to_time(struct vmm_rtc_time *tm, unsigned long *time);

/**
 * Convert seconds since 01-01-1970 00:00:00 to Gregorian date.
 */
void vmm_rtc_time_to_tm(unsigned long time, struct vmm_rtc_time *tm);

#endif /* __VMM_RTCLIB_H_ */
