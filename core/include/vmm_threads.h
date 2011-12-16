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
 * @version 0.01
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

enum vmm_thread_states {
	VMM_THREAD_STATE_CREATED=0,
	VMM_THREAD_STATE_RUNNING=1,
	VMM_THREAD_STATE_STOPPED=2
};

typedef int (*vmm_thread_func_t) (void *udata);

struct vmm_thread {
	struct dlist head;		/* thread list head */
	vmm_vcpu_t * tvcpu;		/* vcpu on which thread runs */
	vmm_thread_func_t tfn;		/* thread functions */
	void *tdata;			/* data passed to thread 
					 * function on execution */
	int tretval;			/* thread return value */
	u64 tnsecs;			/* thread time slice in nanoseconds */
	u8 tstack[CONFIG_THREAD_STACK_SIZE]; /* thread stack */
};

typedef struct vmm_thread vmm_thread_t;

vmm_thread_t *vmm_threads_create(const char *thread_name, 
				 vmm_thread_func_t thread_fn,
				 void *thread_data,
				 u8 thread_priority,
				 u64 thread_nsecs);
int vmm_threads_destroy(vmm_thread_t * tinfo);
int vmm_threads_start(vmm_thread_t * tinfo);
int vmm_threads_stop(vmm_thread_t * tinfo);
u32 vmm_threads_get_id(vmm_thread_t * tinfo);
int vmm_threads_get_name(char * dst, vmm_thread_t * tinfo);
int vmm_threads_get_state(vmm_thread_t * tinfo);
vmm_thread_t *vmm_threads_id2thread(u32 tid);
vmm_thread_t *vmm_threads_index2thread(int index);
u32 vmm_threads_count(void);
int vmm_threads_init(void);

#endif /* __VMM_THREADS_H__ */
