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
 * @file vmm_threads.h
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @author Anup Patel (anup@brainfault.org)
 * @brief Header file of hypervisor threads.
 */
#ifndef __VMM_THREADS_H__
#define __VMM_THREADS_H__

#include <vmm_types.h>
#include <vmm_manager.h>
#include <vmm_spinlocks.h>

#define VMM_THREAD_MAX_PRIORITY	VMM_VCPU_MAX_PRIORITY
#define VMM_THREAD_MIN_PRIORITY VMM_VCPU_MIN_PRIORITY
#define VMM_THREAD_DEF_PRIORITY	VMM_VCPU_DEF_PRIORITY
#define VMM_THREAD_DEF_TIME_SLICE VMM_VCPU_DEF_TIME_SLICE
#define VMM_THREAD_DEF_DEADLINE VMM_VCPU_DEF_DEADLINE
#define VMM_THREAD_DEF_PERIODICITY VMM_VCPU_DEF_PERIODICITY

enum vmm_thread_states {
	VMM_THREAD_STATE_CREATED=0,
	VMM_THREAD_STATE_RUNNING=1,
	VMM_THREAD_STATE_SLEEPING=2,
	VMM_THREAD_STATE_STOPPED=3
};

struct vmm_thread {
	struct dlist head;		/* thread list head */
	struct vmm_vcpu *tvcpu;	/* vcpu on which thread runs */
	int (*tfn) (void *udata);	/* thread functions */
	void *tdata;			/* data passed to thread
					 * function on execution */
	int tretval;			/* thread return value */
	u64 tnsecs;			/* thread time slice in nanoseconds */
	u64 tdeadline;			/* thread deadline in nanoseconds */
	u64 tperiodicity;		/* thread periodicity in nanoseconds */
};

/** Start a thread */
int vmm_threads_start(struct vmm_thread *tinfo);

/** Stop a thread */
int vmm_threads_stop(struct vmm_thread *tinfo);

/** Put a thread to sleep */
int vmm_threads_sleep(struct vmm_thread *tinfo);

/** Wakeup a thread */
int vmm_threads_wakeup(struct vmm_thread *tinfo);

/** Retrive thread id */
u32 vmm_threads_get_id(struct vmm_thread *tinfo);

/** Retrive thread priority */
u8 vmm_threads_get_priority(struct vmm_thread *tinfo);

/** Retrive thread name */
int vmm_threads_get_name(char *dst, struct vmm_thread *tinfo);

/** Retrive thread state */
int vmm_threads_get_state(struct vmm_thread *tinfo);

/** Retrive host CPU assigned to given thread */
int vmm_threads_get_hcpu(struct vmm_thread *tinfo, u32 *hcpu);

/** Update host CPU assigned to given thread */
int vmm_thread_set_hcpu(struct vmm_thread *tinfo, u32 hcpu);

/** Retrive host CPU affinity of given thread */
const struct vmm_cpumask *vmm_threads_get_affinity(struct vmm_thread *tinfo);

/** Update host CPU affinity of given thread */
int vmm_threads_set_affinity(struct vmm_thread *tinfo,
			     const struct vmm_cpumask *cpu_mask);

/** Retrive thread instance from thread id */
struct vmm_thread *vmm_threads_id2thread(u32 tid);

/** Retrive thread instance from thread index */
struct vmm_thread *vmm_threads_index2thread(int index);

/** Count number of threads */
u32 vmm_threads_count(void);

/** Create a new thread with explicitly specified deadline and periodicity.
 *  This is more real-time friendly API so that users can specify deadline
 *  and periodicity for a VCPU.
 */
struct vmm_thread *vmm_threads_create_rt(const char *thread_name,
					 int (*thread_fn) (void *udata),
					 void *thread_data,
					 u8 thread_priority,
				         u64 thread_nsecs,
					 u64 thread_deadline,
					 u64 thread_periodicity);

/** Create a new thread */
static inline struct vmm_thread *vmm_threads_create(
					const char *thread_name,
					int (*thread_fn) (void *udata),
					void *thread_data,
					u8 thread_priority,
					u64 thread_nsecs)
{
	return vmm_threads_create_rt(thread_name, thread_fn, thread_data,
				     thread_priority, thread_nsecs,
				     thread_nsecs * 10, thread_nsecs * 100);
}

/** Destroy a thread */
int vmm_threads_destroy(struct vmm_thread *tinfo);

/** Intialize hypervisor threads */
int vmm_threads_init(void);

#endif /* __VMM_THREADS_H__ */
