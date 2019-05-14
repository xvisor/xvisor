/**
 * Copyright (c) 2010 Anup Patel.
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
 * @file vmm_timer.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for timer subsystem
 */
#ifndef _VMM_TIMER_H__
#define _VMM_TIMER_H__

#include <vmm_types.h>
#include <vmm_spinlocks.h>
#include <libs/list.h>

struct vmm_timer_event;

struct vmm_timer_event {
	/* Publically accessible info */
	u64 expiry_tstamp;
	u64 duration_nsecs;
	void (*handler) (struct vmm_timer_event *);
	void *priv;
	/* Internal house-keeping info */
	vmm_spinlock_t active_lock;
	bool active_state;
	struct dlist active_head;
	u32 active_hcpu;
};

#define INIT_TIMER_EVENT(ev, _hndl, _priv)	\
				do { \
					(ev)->expiry_tstamp = 0; \
					(ev)->duration_nsecs = 0; \
					(ev)->handler = _hndl; \
					(ev)->priv = _priv; \
					INIT_SPIN_LOCK(&(ev)->active_lock); \
					INIT_LIST_HEAD(&(ev)->active_head); \
					(ev)->active_state = FALSE; \
					(ev)->active_hcpu = 0; \
				} while (0)

#define __TIMER_EVENT_INITIALIZER(ev, _hndl, _priv)	\
	{ \
		.expiry_tstamp = 0,					\
		.duration_nsecs = 0,					\
		.handler = _hndl,					\
		.priv = _priv,						\
		.active_lock = __SPINLOCK_INITIALIZER((ev).active_lock),\
		.active_head = { &(ev).head, &(ev).head },		\
		.active_state = FALSE,					\
		.active_hcpu = 0,					\
	}

#define DECLARE_TIMER_EVENT(ev, _hndl, _priv)		\
	struct vmm_timer_event ev = __TIMER_EVENT_INITIALIZER(ev, _hndl, _priv)

/** Get timer clocksource frequency */
u32 vmm_timer_clocksource_frequency(void);

/** Get timer clockchip frequency */
u32 vmm_timer_clockchip_frequency(void);

/** Check if timer event is pending */
bool vmm_timer_event_pending(struct vmm_timer_event *ev);

/** Return the absolute timestamp at which timer event will expire */
u64 vmm_timer_event_expiry_time(struct vmm_timer_event *ev);

/** Start a timer event and return expiry time */
int vmm_timer_event_start2(struct vmm_timer_event *ev,
			   u64 duration_nsecs, u64 *ret_expiry_tstamp);

/** Start a timer event */
static inline int vmm_timer_event_start(struct vmm_timer_event *ev,
					u64 duration_nsecs)
{
	return vmm_timer_event_start2(ev, duration_nsecs, NULL);
}

/** Restart a timer event */
int vmm_timer_event_restart(struct vmm_timer_event *ev);

/** Stop a timer event */
int vmm_timer_event_stop(struct vmm_timer_event *ev);

/** Convert given cycles to nanoseconds */
u64 vmm_timer_cycles_to_ns(u64 cycles);

/** Compute delta of given cycles and current cycles in-terms of nanoseconds */
u64 vmm_timer_delta_cycles_to_ns(u64 cycles);

/** Current global timestamp (nanoseconds elapsed) */
u64 vmm_timer_timestamp(void);

#if defined(CONFIG_PROFILE)
/** Special version for profile */
u64 vmm_timer_timestamp_for_profile(void);
#endif

/** Check if timer subsystem is running on current host CPU */
bool vmm_timer_started(void);

/** Start timer subsystem on current host CPU */
void vmm_timer_start(void);

/** Stop timer subsystem on current host CPU */
void vmm_timer_stop(void);

/** Initialize timer subsystem */
int vmm_timer_init(void);

#endif
