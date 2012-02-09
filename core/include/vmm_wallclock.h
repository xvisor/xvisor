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
 * @file vmm_wallclock.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for wall-clock subsystem
 */
#ifndef _VMM_WALLCLOCK_H__
#define _VMM_WALLCLOCK_H__

#include <vmm_types.h>

struct vmm_timeval {
	s64	tv_sec;		/* seconds */
	s64	tv_nsec;	/* nanoseconds */
};

struct vmm_timezone {
	int	tz_minuteswest;	/* minutes west of Greenwich */
	int	tz_dsttime;	/* type of dst correction */
};

struct vmm_timeinfo {
	/*
	 * the number of seconds after the minute, normally in the range
	 * 0 to 59, but can be up to 60 to allow for leap seconds
	 */
	int tm_sec;
	/* the number of minutes after the hour, in the range 0 to 59*/
	int tm_min;
	/* the number of hours past midnight, in the range 0 to 23 */
	int tm_hour;
	/* the day of the month, in the range 1 to 31 */
	int tm_mday;
	/* the number of months since January, in the range 0 to 11 */
	int tm_mon;
	/* the number of years since 1900 */
	long tm_year;
	/* the number of days since Sunday, in the range 0 to 6 */
	int tm_wday;
	/* the number of days since January 1, in the range 0 to 365 */
	int tm_yday;
};

/** Parameters used to convert the timeval values: */
#define MSEC_PER_SEC	1000L
#define USEC_PER_MSEC	1000L
#define NSEC_PER_USEC	1000L
#define NSEC_PER_MSEC	1000000L
#define USEC_PER_SEC	1000000L
#define NSEC_PER_SEC	1000000000L
#define FSEC_PER_SEC	1000000000000000LL

#define VMM_TIMEVAL_SEC_MAX	((1ULL << ((sizeof(s64) << 3) - 1)) - 1)
#define VMM_TIMEVAL_NSEC_MAX	NSEC_PER_SEC

/** Compare two vmm_timeval instances */
static inline int vmm_timeval_compare(const struct vmm_timeval *lhs, 
					const struct vmm_timeval *rhs)
{
	if (lhs->tv_sec < rhs->tv_sec)
		return -1;
	if (lhs->tv_sec > rhs->tv_sec)
		return 1;
	return lhs->tv_nsec - rhs->tv_nsec;
}

/** Check if the vmm_timeval is normalized or denormalized */
#define vmm_timeval_valid(tv) \
(((tv)->tv_sec >= 0) && (((unsigned long) (tv)->tv_nsec) < NSEC_PER_SEC))

/** Set normalized values to vmm_timeval instance */
void vmm_timeval_set_normalized(struct vmm_timeval *tv, s64 sec, s64 nsec);

/** Add vmm_timeval intances. add = lhs - rhs, in normalized form */
struct vmm_timeval vmm_timeval_add(struct vmm_timeval lhs,
				   struct vmm_timeval rhs);

/** Substract vmm_timeval intances. sub = lhs - rhs, in normalized form */
struct vmm_timeval vmm_timeval_sub(struct vmm_timeval lhs,
				   struct vmm_timeval rhs);

/** Convert vmm_timeval to nanoseconds */
static inline s64 vmm_timeval_to_ns(const struct vmm_timeval *tv)
{
	return ((s64) tv->tv_sec * NSEC_PER_SEC) + tv->tv_nsec;
}

/** Convert nanoseconds to vmm_timeval */
struct vmm_timeval vmm_ns_to_timeval(const s64 nsec);

/** Convert seconds elapsed Gregorian date to vmm_timeinfo 
 *  @totalsecs	number of seconds elapsed since 00:00:00 on January 1, 1970,
 *		Coordinated Universal Time (UTC).
 *  @offset	offset seconds adding to totalsecs.
 *  @result	pointer to struct vmm_timeinfo variable for broken-down time
 */
void vmm_wallclock_mkinfo(s64 totalsecs, int offset, 
			  struct vmm_timeinfo * result);

/** Converts Gregorian date to seconds since 1970-01-01 00:00:00.
 *  Assumes input in normal date format, i.e. 1980-12-31 23:59:59
 *  => year=1980, mon=12, day=31, hour=23, min=59, sec=59.
 */
s64 vmm_wallclock_mktime(const unsigned int year0, 
			 const unsigned int mon0,
			 const unsigned int day, 
			 const unsigned int hour,
			 const unsigned int min, 
			 const unsigned int sec);

/** Set local time */
int vmm_wallclock_set_local_time(struct vmm_timeval * tv);

/** Get local time */
int vmm_wallclock_get_local_time(struct vmm_timeval * tv);

/** Set current timezone */
int vmm_wallclock_set_timezone(struct vmm_timezone * tz);

/** Get current timezone */
int vmm_wallclock_get_timezone(struct vmm_timezone * tz);

/** Set current time and timezone */
int vmm_wallclock_set_timeofday(struct vmm_timeval * tv, 
				struct vmm_timezone * tz);

/** Get current time and timezone */
int vmm_wallclock_get_timeofday(struct vmm_timeval * tv, 
				struct vmm_timezone * tz);

/** Initialize wall-clock subsystem */
int vmm_wallclock_init(void);

#endif
