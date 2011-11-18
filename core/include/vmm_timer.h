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
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for timer subsystem
 */
#ifndef _VMM_TIMER_H__
#define _VMM_TIMER_H__

#include <vmm_types.h>
#include <vmm_list.h>
#include <vmm_regs.h>
#include <vmm_math.h>
#include <vmm_spinlocks.h>

typedef struct vmm_timer_event vmm_timer_event_t;

typedef void (*vmm_timer_event_handler_t) (vmm_timer_event_t * event);

struct vmm_timer_event {
	struct dlist cpu_head;
	struct dlist head;
	vmm_user_regs_t * cpu_regs;
	char name[32];
	bool active;
	u64 expiry_tstamp;
	u64 duration_nsecs;
	vmm_timer_event_handler_t handler;
	void * priv;
};

/** Convert kHz clocksource to clocksource mult */
static inline u32 vmm_timer_clocksource_khz2mult(u32 khz, u32 shift)
{
	u64 tmp = ((u64)1000000) << shift;
	tmp += khz >> 1;
	tmp = vmm_udiv64(tmp, khz);
	return (u32)tmp;
}

/** Convert Hz clocksource to clocksource mult */
static inline u32 vmm_timer_clocksource_hz2mult(u32 hz, u32 shift)
{
	u64 tmp = ((u64)1000000000) << shift;
	tmp += hz >> 1;
	tmp = vmm_udiv64(tmp, hz);
	return (u32)tmp;
}

/** Process timer event (Must be called from somewhere) */
void vmm_timer_clockevent_process(vmm_user_regs_t * regs);

/** Start a timer event */
int vmm_timer_event_start(vmm_timer_event_t * ev, u64 duration_nsecs);

/** Restart a timer event */
int vmm_timer_event_restart(vmm_timer_event_t * ev);

/** Stop a timer event */
int vmm_timer_event_stop(vmm_timer_event_t * ev);

/** Create a timer event */
vmm_timer_event_t * vmm_timer_event_create(const char *name,
					   vmm_timer_event_handler_t handler,
					   void * priv);

/** Destroy a timer event */
int vmm_timer_event_destroy(vmm_timer_event_t * ev);

/** Find a timer event */
vmm_timer_event_t *vmm_timer_event_find(const char *name);

/** Retrive timer event with given index */
vmm_timer_event_t *vmm_timer_event_get(int index);

/** Count number of timer events */
u32 vmm_timer_event_count(void);

/** Current global timestamp (nanoseconds elapsed) */
u64 vmm_timer_timestamp(void);

/** Start all timer events */
void vmm_timer_start(void);

/** Stop all timer events */
void vmm_timer_stop(void);

/** Initialize timer subsystem */
int vmm_timer_init(void);

#endif
