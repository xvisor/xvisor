/**
 * Copyright (c) 2010 Himanshu Chauhan.
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
 * @file vmm_wait.h
 * @version 0.01
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief Hyperthread wait functions.
 */

#ifndef __VMM_WAIT_H__
#define __VMM_WAIT_H__

#include <vmm_types.h>
#include <vmm_spinlocks.h>
#include <vmm_hyperthreads.h>

typedef struct __wait_head {
	vmm_spinlock_t lock;
	struct dlist wait_list_head;
} vmm_wait_head_t;

typedef struct __wait_element {
	struct dlist list_head;
	vmm_hyperthread_t *thread;
} vmm_wait_element_t;

#define DEFINE_WAIT_LIST(_name)	vmm_wait_head_t _name =			\
	{								\
		.lock = __INIT_SPIN_LOCK_UNLOCKED,			\
		.wait_list_head = { &_name.wait_list_head, &_name.wait_list_head } \
	}

/*
 * Definition shows that this macro can only be used
 * from inside a function call. Since we depend on
 * frame address for finding out the thread defining
 * the wait.
 */
#define DEFINE_WAIT_ELEMENT(_name)					\
	vmm_wait_element_t _name =					\
	{								\
		.thread = (vmm_hyperthread_t *)((u32)__builtin_frame_address(0) \
						& 0xFFFFF000),		\
		.list_head = { &_name.list_head, &_name.list_head } \
	}

extern vmm_wait_head_t global_wait_queue;
extern jiffies_t hcore_jiffies;

u32 add_to_wait_queue(vmm_wait_head_t * wait_list,
		      vmm_wait_element_t * wait_element);
u32 remove_from_wait_queue(vmm_wait_head_t * wait_list,
			   vmm_wait_element_t * wait_element);
u32 wake_up_on_queue(vmm_wait_head_t * wait_list);

#define wait_on_event_running(cond)				\
	do {							\
		while(1) {					\
			if (cond) break;			\
			vmm_hypercore_yield();			\
		};						\
	} while(0);

#define wait_on_event(wait_queue)				\
	do {							\
		DEFINE_WAIT_ELEMENT(wait);			\
		add_to_wait_queue(wait_queue, wait);		\
		vmm_hyperthread_set_state(CURRENT_THREAD,	\
					THREAD_STATE_SLEEP);	\
		vmm_hypercore_sched_dequeue_thread(CURRENT_THREAD);	\
		vmm_hypercore_yield();					\
	} while(0);

#define wait_on_event_running_timeout(next_jiffies)            \
       wait_on_event_running((hcore_jiffies >= next_jiffies))

#define loop_till_timeout(next_jiffies)		\
	while (hcore_jiffies < next_jiffies);

#endif /* __VMM_WAIT_H__ */
