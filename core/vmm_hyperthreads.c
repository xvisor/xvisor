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
 * @file vmm_hyperthreads.c
 * @version 0.01
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @author Anup Patel (anup@brainfault.org)
 * @brief Source file for hyperthreads. These run on top of vcpus.
 */

#include <vmm_stdio.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_list.h>
#include <vmm_types.h>
#include <vmm_cpu.h>
#include <vmm_string.h>
#include <vmm_spinlocks.h>
#include <vmm_devtree.h>
#include <vmm_scheduler.h>
#include <vmm_wait.h>
#include <vmm_hyperthreads.h>

#define THREAD_MAX_TICKS		12

vmm_hypercore_info_t hcore_info;
vmm_hyperthread_t *hcore_init_thread = NULL;
jiffies_t hcore_jiffies;

vmm_hypercore_runqueue_t hcore_runqueue;

/* list of all threads running on hypercore */
struct global_htreads_list {
	struct dlist ht_list;
	vmm_spinlock_t ht_lock;
} ghthreads_list;

void vmm_hypercore_schedule(vmm_user_regs_t * tregs)
{
	vmm_hyperthread_t *next, *cthread;
	struct dlist *thead;

	if (hcore_info.started) {
		cthread = vmm_hyperthread_uregs2thread(tregs);

		vmm_spin_lock(&hcore_runqueue.rlock);
		vmm_spin_lock(&cthread->tlock);

		thead = cthread->rq_head.next;
		next = container_of(thead, vmm_hyperthread_t, rq_head);
		if ((u32) thead == (u32) & hcore_runqueue.thread_list) {
			thead = hcore_runqueue.thread_list.next;
			next = container_of(thead, vmm_hyperthread_t, rq_head);
		}

		vmm_spin_unlock(&cthread->tlock);
		vmm_spin_unlock(&hcore_runqueue.rlock);

		if (next && cthread != next) {
			/* Switch thread context */
			vmm_hyperthread_regs_switch(cthread, next, tregs);
		}
	} else {
		vmm_spin_lock(&hcore_runqueue.rlock);

		thead = hcore_runqueue.thread_list.next;
		if ((u32) thead == (u32) & hcore_runqueue.thread_list) {
			vmm_panic("Failed schedule next thread\n");
		}
		next = container_of(thead, vmm_hyperthread_t, rq_head);

		/* Restore next threads registers */
		vmm_hyperthread_regs_switch(NULL, next, tregs);

		hcore_info.started = 1;

		vmm_spin_unlock(&hcore_runqueue.rlock);
	}
}

void vmm_hypercore_yield(void)
{
	irq_flags_t flag;
	jiffies_t ctick;
	vmm_hyperthread_t *current = vmm_hyperthread_context2thread();

	flag = vmm_spin_lock_irqsave(&current->tlock);
	current->tjiffies = THREAD_MAX_TICKS;
	ctick = hcore_jiffies + 1;
	vmm_spin_unlock_irqrestore(&current->tlock, flag);
	loop_till_timeout(ctick);
}

s32 vmm_hypercore_sched_enqueue_thread(vmm_hyperthread_t * tinfo)
{
	BUG_ON(tinfo == NULL, "Null thread structure to sched enque!\n");

	vmm_spin_lock(&hcore_runqueue.rlock);
	list_add_tail(&hcore_runqueue.thread_list, &tinfo->rq_head);
	vmm_spin_unlock(&hcore_runqueue.rlock);

	return VMM_OK;
}

s32 vmm_hypercore_sched_dequeue_thread(vmm_hyperthread_t * tinfo)
{
	/*
	 * BIG NOTE: Thread specific lock should be taken by this caller since
	 * this doesn't set the thread state and nobody should get hold of the
	 * thread until the state is correctly set.
	 */
	vmm_spin_lock(&hcore_runqueue.rlock);
	list_del(&tinfo->rq_head);
	vmm_spin_unlock(&hcore_runqueue.rlock);

	return VMM_OK;
}

void vmm_hypercore_ticks(vmm_user_regs_t * regs, u32 ticks_left)
{
	vmm_hyperthread_t *cthread;

	if (hcore_info.started) {
		cthread = vmm_hyperthread_uregs2thread(regs);

		hcore_jiffies++;

		if (++cthread->tjiffies > THREAD_MAX_TICKS) {
			cthread->tjiffies = 0;
			vmm_hypercore_schedule(regs);
		}
	} else {
		vmm_hypercore_schedule(regs);
	}
}

static void vmm_hypercore_main(void)
{
	/* We are done. */
	/* Wait for hypercore scheduler to get invoked. */
	while (1) ;
}

s32 vmm_hypercore_init(void)
{
	const char *attrval;
	vmm_devtree_node_t *vnode;

	INIT_LIST_HEAD(&hcore_runqueue.thread_list);
	INIT_SPIN_LOCK(&hcore_runqueue.rlock);
	vmm_memset(&hcore_info, 0, sizeof(hcore_info));

	/* create a new orphaned vcpu as hypercore */
	vnode = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPRATOR_STRING
				    VMM_DEVTREE_VMMINFO_NODE_NAME);
	if (!vnode) {
		goto vcpu_init_fail;
	}
	attrval = vmm_devtree_attrval(vnode,
				      VMM_DEVTREE_HCORE_TICK_COUNT_ATTR_NAME);
	if (!attrval) {
		goto vcpu_init_fail;
	}

	/* start hypercore vcpu with schedule and let it decide which thread to run */
	hcore_info.vcpu = vmm_scheduler_vcpu_orphan_create("hypercore",
							(virtual_addr_t) &
							vmm_hypercore_main,
							*(u32 *) attrval,
							&vmm_hypercore_ticks);
	if (!hcore_info.vcpu) {
		goto vcpu_init_fail;
	}

	/* Initialize hypercore jiffies */
	hcore_jiffies = 0;

	return VMM_OK;

vcpu_init_fail:
	vmm_hyperthread_kill(hcore_init_thread);
	return VMM_EFAIL;
}

static s32 vmm_hyperthread_add_thread_to_global_list(vmm_hyperthread_t * tinfo)
{
	vmm_spin_lock(&ghthreads_list.ht_lock);
	list_add_tail(&ghthreads_list.ht_list, &tinfo->glist_head);
	vmm_spin_unlock(&ghthreads_list.ht_lock);

	return VMM_OK;
}

vmm_hyperthread_t *vmm_hyperthread_create(char *tname, void *fn, void *udata)
{
	irq_flags_t flags;
	void *tmem = vmm_malloc(sizeof(vmm_hyperthread_info_t) * 2);
	vmm_hyperthread_info_t *tinfo;

	if (!tmem) {
		return NULL;
	}
	tmem = (void *)((u32) tmem + sizeof(vmm_hyperthread_info_t));
	tmem =
	    (void *)((u32) tmem -
		     ((u32) tmem) % sizeof(vmm_hyperthread_info_t));
	tinfo = tmem;

	INIT_SPIN_LOCK(&tinfo->thread_info.tlock);
	tinfo->thread_info.tfn = fn;
	tinfo->thread_info.tdata = udata;
	INIT_LIST_HEAD(&tinfo->thread_info.glist_head);
	INIT_LIST_HEAD(&tinfo->thread_info.rq_head);

	flags = vmm_spin_lock_irqsave(&tinfo->thread_info.tlock);

	/* threads don't run immediately. call vmm_hyperthread_run */
	vmm_hyperthread_set_state((vmm_hyperthread_t *) tinfo,
				  THREAD_STATE_STOPPED);

	vmm_strcpy(tinfo->thread_info.tname, tname);
	tinfo->thread_info.preempted = 0;

	/* Architecture specific init */
	vmm_hyperthread_regs_init((vmm_hyperthread_t *) tinfo, udata);

	/* add newly created thread to global thread list */
	vmm_hyperthread_add_thread_to_global_list((vmm_hyperthread_t *) tinfo);

	vmm_spin_unlock_irqrestore(&tinfo->thread_info.tlock, flags);

	return (vmm_hyperthread_t *) tinfo;
}

s32 vmm_hyperthread_run(vmm_hyperthread_t * tinfo)
{
	irq_flags_t flags;

	BUG_ON(tinfo == NULL, "Thread run: NULL thread\n");

	flags = vmm_spin_lock_irqsave(&tinfo->tlock);

	/* Set state to running */
	vmm_hyperthread_set_state(tinfo, THREAD_STATE_RUNNING);

	/* enqueue in scheduler's list */
	vmm_hypercore_sched_enqueue_thread(tinfo);

	vmm_spin_unlock_irqrestore(&tinfo->tlock, flags);

	return VMM_OK;
}

s32 vmm_hyperthread_stop(vmm_hyperthread_t * tinfo)
{
	return VMM_OK;
}

s32 vmm_hyperthread_kill(vmm_hyperthread_t * tinfo)
{
	irq_flags_t flags;

	BUG_ON(tinfo == NULL, "Thread kill: NULL thread\n");

	/* remove the thread from global list */
	flags = vmm_spin_lock_irqsave(&ghthreads_list.ht_lock);
	list_del(&tinfo->glist_head);
	vmm_spin_unlock_irqrestore(&ghthreads_list.ht_lock, flags);

	flags = vmm_spin_lock_irqsave(&tinfo->tlock);

	/* dequeue from scheduler's list if currently running */
	vmm_hypercore_sched_dequeue_thread(tinfo);

	/* change the thread state. */
	vmm_hyperthread_set_state(tinfo, THREAD_STATE_DEAD);

	vmm_spin_unlock_irqrestore(&tinfo->tlock, flags);

	/* Free thread info */
	vmm_free(tinfo);

	return VMM_OK;
}

s32 vmm_hyperthread_set_state(vmm_hyperthread_t * tinfo,
			      vmm_hyperthread_state_t state)
{
	BUG_ON(tinfo == NULL, "Thread set state: NULL thread\n");

	tinfo->tstate = state;

	return VMM_OK;
}

void vmm_hyperthreads_print_all_info(void)
{
	struct dlist *thead;
	vmm_hyperthread_t *curr;
	int nr_threads = 0;

	vmm_spin_lock(&ghthreads_list.ht_lock);

	list_for_each(thead, &ghthreads_list.ht_list) {
		curr = container_of(thead, vmm_hyperthread_t, glist_head);
		if (curr) {
			vmm_printf("Thread: %s\n", curr->tname);
			nr_threads++;
		}
	}

	vmm_printf("\nTotal %d hyperthreads running.\n", nr_threads);

	vmm_spin_unlock(&ghthreads_list.ht_lock);
}

s32 hthread_idle(void *udata)
{
	jiffies_t cjiffies;

	while (1) {
		cjiffies = hcore_jiffies;
		wait_on_event_running_timeout((cjiffies + 50));
	}

	return VMM_OK;
}

s32 vmm_hyperthreading_init(void)
{
	int ret;
	vmm_hyperthread_t *idle_thread;

	INIT_LIST_HEAD(&ghthreads_list.ht_list);
	INIT_SPIN_LOCK(&ghthreads_list.ht_lock);

	ret = vmm_hypercore_init();
	if (ret) {
		return ret;
	}

	idle_thread =
	    vmm_hyperthread_create("idle", (void *)&hthread_idle, NULL);
	if (!idle_thread) {
		return VMM_EFAIL;
	}

	vmm_hyperthread_run(idle_thread);

	return VMM_OK;
}
