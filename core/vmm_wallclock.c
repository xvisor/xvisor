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
 */

#include <vmm_error.h>
#include <vmm_math.h>
#include <vmm_string.h>
#include <vmm_timer.h>
#include <vmm_spinlocks.h>
#include <vmm_wallclock.h>

struct vmm_wallclock_ctrl {
	vmm_spinlock_t lock;
	struct vmm_timeval tv;
	struct vmm_timezone tz;
	u64 last_modify_tstamp;
};

struct vmm_wallclock_ctrl wclk;

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

	tv.tv_sec = vmm_sdiv64(nsec, NSEC_PER_SEC);
	tv.tv_nsec = nsec - tv.tv_sec * NSEC_PER_SEC;
	if (tv.tv_nsec < 0) {
		tv.tv_sec--;
		tv.tv_nsec += NSEC_PER_SEC;
	}

	return tv;
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
 * care of this issue we return u64 try to avoid overflow.
 */
u64 vmm_wallclock_mktime(const unsigned int year0, 
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

	return ret;
}

int vmm_wallclock_set_local_time(struct vmm_timeval * tv)
{
	irq_flags_t flags;

	if (!tv) {
		return VMM_EFAIL;
	}

	flags = vmm_spin_lock_irqsave(&wclk.lock);

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

	flags = vmm_spin_lock_irqsave(&wclk.lock);

	tv->tv_sec = wclk.tv.tv_sec;
	tv->tv_nsec = wclk.tv.tv_nsec;
	tdiff = vmm_timer_timestamp() - wclk.last_modify_tstamp;
	tdiv = vmm_udiv64(tdiff, NSEC_PER_SEC);
	tmod = tdiff - tdiv * NSEC_PER_SEC;
	tv->tv_nsec += tmod;
	while (NSEC_PER_SEC <= tv->tv_nsec) {
		tv->tv_sec++;
		tv->tv_nsec -= NSEC_PER_SEC;
	}
	tv->tv_sec += tdiv;

	vmm_spin_unlock_irqrestore(&wclk.lock, flags);

	return VMM_OK;
}

int vmm_wallclock_set_timezone(struct vmm_timezone * tz)
{
	int minuteswest;
	irq_flags_t flags;

	if (!tz) {
		return VMM_EFAIL;
	}

	flags = vmm_spin_lock_irqsave(&wclk.lock);

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

	flags = vmm_spin_lock_irqsave(&wclk.lock);

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

