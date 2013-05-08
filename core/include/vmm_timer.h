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
	struct dlist head;
	bool active;
	u64 expiry_tstamp;
	u64 duration_nsecs;
	void (*handler) (struct vmm_timer_event *);
	void *priv;
};

#define INIT_TIMER_EVENT(ev, _hndl, _priv)	do { \
						INIT_LIST_HEAD(&(ev)->head); \
						(ev)->active = FALSE; \
						(ev)->expiry_tstamp = 0; \
						(ev)->duration_nsecs = 0; \
						(ev)->handler = _hndl; \
						(ev)->priv = _priv; \
						} while (0)

#define __TIMER_EVENT_INITIALIZER(ev, _hndl, _priv)	{	\
	.head	= { &(ev).head, &(ev).head },			\
	.active = FALSE,					\
	.expiry_tstamp = 0,					\
	.duration_nsecs = 0,					\
	.handler = _hndl,					\
	.priv = _priv,						\
	}

#define DECLARE_TIMER_EVENT(ev, _hndl, _priv)		\
	struct vmm_timer_event ev = __TIMER_EVENT_INITIALIZER(ev, _hndl, _priv)

/** Start a timer event */
int vmm_timer_event_start(struct vmm_timer_event *ev, u64 duration_nsecs);

/** Restart a timer event */
int vmm_timer_event_restart(struct vmm_timer_event *ev);

/** Stop a timer event */
int vmm_timer_event_stop(struct vmm_timer_event *ev);

/** Current global timestamp (nanoseconds elapsed) */
u64 vmm_timer_timestamp(void);

#if defined(CONFIG_PROFILE)
/** Special version for profile */
u64 vmm_timer_timestamp_for_profile(void);
#endif

/** Start all timer events */
void vmm_timer_start(void);

/** Stop all timer events */
void vmm_timer_stop(void);

/** Initialize timer subsystem */
int vmm_timer_init(void);

#endif
