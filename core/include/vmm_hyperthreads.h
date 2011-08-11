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
 * @file vmm_hyperthreads.h
 * @version 0.01
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief Hyperthreads structures.
 */
#ifndef __VMM_HYPERTHREADS_H__
#define __VMM_HYPERTHREADS_H__

#include <vmm_types.h>
#include <vmm_manager.h>
#include <vmm_spinlocks.h>

typedef enum hyperthread_state {
	THREAD_STATE_RUNNING,
	THREAD_STATE_SLEEP,
	THREAD_STATE_STOPPED,
	THREAD_STATE_DEAD,
} vmm_hyperthread_state_t;

typedef struct vmm_hypercore_runqueue {
	struct dlist thread_list;
	vmm_spinlock_t rlock;
} vmm_hypercore_runqueue_t;

typedef struct vmm_hypercore_info {
	/* XXX: What when mcore spans multiple VCPUs */
	vmm_vcpu_t * vcpu; /* pointer to vcpu on which hypercore is running */
	u32 started;
} vmm_hypercore_info_t;

typedef void (*vmm_hyperthread_func_t) (void *udata);

typedef struct hyperthread {
	vmm_user_regs_t tregs;	/* thread registers saved across scheduling */
	void *tfn;		/* thread functions */
	void *tdata;		/* data passed to thread function on execution */
	vmm_spinlock_t tlock;
	vmm_hyperthread_state_t tstate;	/* current state of the thread. */
	jiffies_t tjiffies;	/* time that thread has run */
	int preempted;		/* Mark that the thread didn't yield but was forced */
	struct dlist glist_head;	/* for entry in global thread list */
	struct dlist rq_head;	/* for entry in hypercore scheduler's runqueue */
	char tname[32];
} vmm_hyperthread_t;

typedef union hyperthread_info {
	vmm_hyperthread_t thread_info;
	u32 tstack[1024];	/* 4K stack shared with thread data */
} vmm_hyperthread_info_t;

void vmm_hypercore_yield(void);
s32 vmm_hypercore_sched_enqueue_thread(vmm_hyperthread_t * tinfo);
s32 vmm_hypercore_sched_dequeue_thread(vmm_hyperthread_t * tinfo);
s32 vmm_hypercore_init(void);

vmm_hyperthread_t *vmm_hyperthread_create(char *thread_name, void *fn,
					  void *udata);
s32 vmm_hyperthread_run(vmm_hyperthread_t * tinfo);
s32 vmm_hyperthread_stop(vmm_hyperthread_t * tinfo);
s32 vmm_hyperthread_kill(vmm_hyperthread_t * tinfo);
s32 vmm_hyperthread_set_state(vmm_hyperthread_t * tinfo,
			      vmm_hyperthread_state_t state);
void vmm_hyperthreads_print_all_info(void);
s32 vmm_hyperthreading_init(void);

#endif /* __VMM_HYPERTHREADS_H__ */
