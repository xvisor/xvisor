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
#include <vmm_spinlocks.h>

typedef struct vmm_timer_event vmm_timer_event_t;

typedef void (*vmm_timer_event_handler_t) (vmm_timer_event_t * event);

struct vmm_timer_event {
	struct dlist head;
	char name[32];
	bool active;
	bool on_cpu_list;
	struct dlist cpu_head;
	vmm_user_regs_t * cpu_regs;
	u64 pending_nsecs;
	u64 duration_nsecs;
	vmm_timer_event_handler_t handler;
	void * priv;
};

/** Control structure for Timer Subsystem */
struct vmm_timer_ctrl {
	u64 timestamp;
	u64 tick_nsecs;
	struct dlist cpu_event_list;
	struct dlist event_list;
};

typedef struct vmm_timer_ctrl vmm_timer_ctrl_t;

/** Process timer tick (Must be called from somewhere) */
void vmm_timer_tick_process(vmm_user_regs_t * regs);

/** Current global timestamp (nanoseconds elapsed) */
u64 vmm_timer_timestamp(void);

/** Adjust global timestamp (nanoseconds elapsed) 
  * Note: we can only increase timestamp since its a monotonic value.
  */
int vmm_timer_adjust_timestamp(u64 timestamp);

/** Get timer tick delay in microseconds */
u64 vmm_timer_tick_usecs(void);

/** Get timer tick delay in nanoseconds */
u64 vmm_timer_tick_nsecs(void);

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

/** Start timer */
void vmm_timer_start(void);

/** Stop timer */
void vmm_timer_stop(void);

/** Initialize timer subsystem */
int vmm_timer_init(void);

#endif
