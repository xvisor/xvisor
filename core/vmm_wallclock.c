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
 * @file vmm_wallclock.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief source file for wall-clock subsystem
 *
 *  This source has been adapted from linux/kernel/time.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *
 *  This file contains the interface functions for the various
 *  time related system calls: time, stime, gettimeofday, settimeofday,
 *			       adjtime
 *
 *  The original code is licensed under the GPL.
 */

#include <vmm_error.h>
#include <vmm_string.h>
#include <vmm_timer.h>
#include <vmm_spinlocks.h>
#include <vmm_wallclock.h>
#include <mathlib.h>

struct vmm_wallclock_ctrl {
	vmm_spinlock_t lock;
	struct vmm_timeval tv;
	struct vmm_timezone tz;
	u64 last_modify_tstamp;
};

static struct vmm_wallclock_ctrl wclk;

void vmm_timeval_set_normalized(struct vmm_timeval *tv, s64 sec, s64 nsec)
{
	while (nsec >= NSEC_PER_SEC) {
		/*
		 * The following asm() prevents the compiler from
		 * optimising this loop into a modulo operation. See
		 * also __iter_div_u64_rem() in include/linux/time.h
		 */
		asm("" : "+rm"(nsec));
		nsec -= NSEC_PER_SEC;
		++sec;
	}
	while (nsec < 0) {
		asm("" : "+rm"(nsec));
		nsec += NSEC_PER_SEC;
		--sec;
	}
	tv->tv_sec = sec;
	tv->tv_nsec = nsec;
}

struct vmm_timeval vmm_timeval_add(struct vmm_timeval lhs,
				   struct vmm_timeval rhs)
{
	struct vmm_timeval tv_delta;
	vmm_timeval_set_normalized(&tv_delta, lhs.tv_sec + rhs.tv_sec,
				   lhs.tv_nsec + rhs.tv_nsec);
	if (tv_delta.tv_sec < lhs.tv_sec || tv_delta.tv_sec < rhs.tv_sec)
		tv_delta.tv_sec = VMM_TIMEVAL_SEC_MAX;
	return tv_delta;
}

struct vmm_timeval vmm_timeval_sub(struct vmm_timeval lhs,
				   struct vmm_timeval rhs)
{
	struct vmm_timeval tv_delta;
	vmm_timeval_set_normalized(&tv_delta, lhs.tv_sec - rhs.tv_sec,
				   lhs.tv_nsec - rhs.tv_nsec);
	if (tv_delta.tv_sec < lhs.tv_sec || tv_delta.tv_sec < rhs.tv_sec)
		tv_delta.tv_sec = VMM_TIMEVAL_SEC_MAX;
	return tv_delta;
}

struct vmm_timeval vmm_ns_to_timeval(const s64 nsec)
{
	struct vmm_timeval tv;

	if (!nsec) {
		tv.tv_sec = 0;
		tv.tv_nsec = 0;
		return tv;
	}

	tv.tv_sec = sdiv64(nsec, NSEC_PER_SEC);
	tv.tv_nsec = nsec - tv.tv_sec * NSEC_PER_SEC;
	if (tv.tv_nsec < 0) {
		tv.tv_sec--;
		tv.tv_nsec += NSEC_PER_SEC;
	}

	return tv;
}

/*
 * Nonzero if YEAR is a leap year (every 4 years,
 * except every 100th isn't, and every 400th is).
 */
static int __isleap(long year)
{
	return (year) % 4 == 0 && ((year) % 100 != 0 || (year) % 400 == 0);
}

/* do a mathdiv for long type */
static long math_div(long a, long b)
{
	return sdiv64(a, b) - (smod64(a, b) < 0);
}

/* How many leap years between y1 and y2, y1 must less or equal to y2 */
static long leaps_between(long y1, long y2)
{
	long leaps1 = math_div(y1 - 1, 4) - math_div(y1 - 1, 100)
		+ math_div(y1 - 1, 400);
	long leaps2 = math_div(y2 - 1, 4) - math_div(y2 - 1, 100)
		+ math_div(y2 - 1, 400);
	return leaps2 - leaps1;
}

/* How many days come before each month (0-12). */
static const unsigned short __mon_yday[2][13] = {
	/* Normal years. */
	{0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365},
	/* Leap years. */
	{0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366}
};

#define SECS_PER_HOUR	(60 * 60)
#define SECS_PER_DAY	(SECS_PER_HOUR * 24)

void vmm_wallclock_mkinfo(s64 totalsecs, int offset, 
			  struct vmm_timeinfo * result)
{
	long days, rem, y;
	const unsigned short *ip;

	days = sdiv64(totalsecs, SECS_PER_DAY);
	rem = totalsecs - days * SECS_PER_DAY;
	rem += offset;
	while (rem < 0) {
		rem += SECS_PER_DAY;
		--days;
	}
	while (rem >= SECS_PER_DAY) {
		rem -= SECS_PER_DAY;
		++days;
	}

	result->tm_hour = rem / SECS_PER_HOUR;
	rem %= SECS_PER_HOUR;
	result->tm_min = rem / 60;
	result->tm_sec = rem % 60;

	/* January 1, 1970 was a Thursday. */
	result->tm_wday = (4 + days) % 7;
	if (result->tm_wday < 0)
		result->tm_wday += 7;

	y = 1970;

	while (days < 0 || days >= (__isleap(y) ? 366 : 365)) {
		/* Guess a corrected year, assuming 365 days per year. */
		long yg = y + math_div(days, 365);

		/* Adjust DAYS and Y to match the guessed year. */
		days -= (yg - y) * 365 + leaps_between(y, yg);
		y = yg;
	}

	result->tm_year = y - 1900;

	result->tm_yday = days;

	ip = __mon_yday[__isleap(y)];
	for (y = 11; days < ip[y]; y--)
		continue;
	days -= ip[y];

	result->tm_mon = y;
	result->tm_mday = days + 1;
}

/* [For the Julian calendar (which was used in Russia before 1917,
 * Britain & colonies before 1752, anywhere else before 1582,
 * and is still in use by some communities) leave out the
 * -year/100+year/400 terms, and add 10.]
 *
 * This algorithm was first published by Gauss (I think).
 *
 * NOTE: the original function mktime() in linux will overflow on 
 * 2106-02-07 06:28:16 on machines where long is 32-bit! To take 
 * care of this issue we return s64 try to avoid overflow.
 */
s64 vmm_wallclock_mktime(const unsigned int year0, 
			 const unsigned int mon0,
			 const unsigned int day, 
			 const unsigned int hour,
			 const unsigned int min, 
			 const unsigned int sec)
{
	unsigned int year = year0, mon = mon0;
	u64 ret;

	/* 1..12 -> 11,12,1..10 */
	if (0 >= (int) (mon -= 2)) {
		mon += 12; /* Puts Feb last since it has leap day */
		year -= 1;
	}

	/* no. of days */
	ret = (u64)(year/4 - year/100 + year/400 + 367*mon/12 + day);
	ret += (u64)(year)*365 - 719499;

	/* no. of hours */
	ret *= (u64)24;
	ret += hour;
	
	/* no. of mins */
	ret *= (u64)60;
	ret += min;

	/* no. of secs */
	ret *= (u64)60;
	ret += sec;

	return (s64)ret;
}

int vmm_wallclock_set_local_time(struct vmm_timeval * tv)
{
	irq_flags_t flags;

	if (!tv) {
		return VMM_EFAIL;
	}

	vmm_spin_lock_irqsave(&wclk.lock, flags);

	wclk.tv.tv_sec = tv->tv_sec;
	wclk.tv.tv_nsec = tv->tv_nsec;
	wclk.last_modify_tstamp = vmm_timer_timestamp();

	vmm_spin_unlock_irqrestore(&wclk.lock, flags);

	return VMM_OK;
}

int vmm_wallclock_get_local_time(struct vmm_timeval * tv)
{
	irq_flags_t flags;
	u64 tdiff, tdiv, tmod;

	if (!tv) {
		return VMM_EFAIL;
	}

	vmm_spin_lock_irqsave(&wclk.lock, flags);

	tv->tv_sec = wclk.tv.tv_sec;
	tv->tv_nsec = wclk.tv.tv_nsec;
	tdiff = vmm_timer_timestamp() - wclk.last_modify_tstamp;

	vmm_spin_unlock_irqrestore(&wclk.lock, flags);

	tdiv = udiv64(tdiff, NSEC_PER_SEC);
	tmod = tdiff - tdiv * NSEC_PER_SEC;
	tv->tv_nsec += tmod;
	while (NSEC_PER_SEC <= tv->tv_nsec) {
		tv->tv_sec++;
		tv->tv_nsec -= NSEC_PER_SEC;
	}
	tv->tv_sec += tdiv;

	return VMM_OK;
}

int vmm_wallclock_set_timezone(struct vmm_timezone * tz)
{
	int minuteswest;
	irq_flags_t flags;

	if (!tz) {
		return VMM_EFAIL;
	}

	vmm_spin_lock_irqsave(&wclk.lock, flags);

	minuteswest = tz->tz_minuteswest - wclk.tz.tz_minuteswest;
	wclk.tv.tv_sec += minuteswest * 60;
	wclk.tz.tz_minuteswest = tz->tz_minuteswest;
	wclk.tz.tz_dsttime = tz->tz_dsttime;

	vmm_spin_unlock_irqrestore(&wclk.lock, flags);

	return VMM_OK;
}

int vmm_wallclock_get_timezone(struct vmm_timezone * tz)
{
	irq_flags_t flags;

	if (!tz) {
		return VMM_EFAIL;
	}

	vmm_spin_lock_irqsave(&wclk.lock, flags);

	tz->tz_minuteswest = wclk.tz.tz_minuteswest;
	tz->tz_dsttime = wclk.tz.tz_dsttime;

	vmm_spin_unlock_irqrestore(&wclk.lock, flags);

	return VMM_OK;
}

int vmm_wallclock_set_timeofday(struct vmm_timeval * tv, 
				struct vmm_timezone * tz)
{
	int rc;

	if (tz) {
		if ((rc = vmm_wallclock_set_timezone(tz))) {
			return rc;
		}
	}

	if (tv) {
		if ((rc = vmm_wallclock_set_local_time(tv))) {
			return rc;
		}
	}

	return VMM_OK;
}

int vmm_wallclock_get_timeofday(struct vmm_timeval * tv, 
				struct vmm_timezone * tz)
{
	int rc;

	if (tz) {
		if ((rc = vmm_wallclock_get_timezone(tz))) {
			return rc;
		}
	}

	if (tv) {
		if ((rc = vmm_wallclock_get_local_time(tv))) {
			return rc;
		}
	}

	return VMM_OK;
}

int vmm_wallclock_init(void)
{
	vmm_memset(&wclk, 0, sizeof(wclk));

	INIT_SPIN_LOCK(&wclk.lock);

	wclk.tv.tv_sec = 0;
	wclk.tv.tv_nsec = 0;

	wclk.tz.tz_minuteswest = 0;
	wclk.tz.tz_dsttime = 0;

	wclk.last_modify_tstamp = vmm_timer_timestamp();

	return VMM_OK;
}

