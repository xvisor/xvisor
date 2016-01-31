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
 * @file vmm_threads.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @author Anup Patel (anup@brainfault.org)
 * @brief Source file for hypervisor threads. These run on top of vcpus.
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_heap.h>
#include <vmm_spinlocks.h>
#include <vmm_scheduler.h>
#include <vmm_threads.h>
#include <vmm_main.h>
#include <libs/stringlib.h>

struct vmm_threads_ctrl {
        vmm_spinlock_t lock;
        u32 thread_count;
        struct dlist thread_list;
};

static struct vmm_threads_ctrl thctrl;

int vmm_threads_start(struct vmm_thread *tinfo)
{
	int rc;

	if (!tinfo) {
		return VMM_EFAIL;
	}

	if ((rc = vmm_manager_vcpu_kick(tinfo->tvcpu))) {
		return rc;
	}

	return VMM_OK;
}

int vmm_threads_stop(struct vmm_thread *tinfo)
{
	int rc;

	if (!tinfo) {
		return VMM_EFAIL;
	}

	if ((rc = vmm_manager_vcpu_reset(tinfo->tvcpu))) {
		return rc;
	}

	return VMM_OK;
}

int vmm_threads_sleep(struct vmm_thread *tinfo)
{
	int rc;

	if (!tinfo) {
		return VMM_EFAIL;
	}

	if ((rc = vmm_manager_vcpu_pause(tinfo->tvcpu))) {
		return rc;
	}

	return VMM_OK;
}

int vmm_threads_wakeup(struct vmm_thread *tinfo)
{
	int rc;

	if (!tinfo) {
		return VMM_EFAIL;
	}

	if ((rc = vmm_manager_vcpu_resume(tinfo->tvcpu))) {
		return rc;
	}

	return VMM_OK;
}

u32 vmm_threads_get_id(struct vmm_thread *tinfo)
{
	if (!tinfo) {
		return 0;
	}

	return tinfo->tvcpu->id;
}

u8 vmm_threads_get_priority(struct vmm_thread *tinfo)
{
	if (!tinfo) {
		return 0;
	}

	return tinfo->tvcpu->priority;
}

int vmm_threads_get_name(char *dst, struct vmm_thread *tinfo)
{
	if (!tinfo || !dst) {
		return VMM_EFAIL;
	}

	strcpy(dst, tinfo->tvcpu->name);

	return VMM_OK;
}

int vmm_threads_get_state(struct vmm_thread *tinfo)
{
	int rc = -1;
	u32 state;

	if (!tinfo) {
		rc =  -1;
	} else {
		state = vmm_manager_vcpu_get_state(tinfo->tvcpu);
		if (state & VMM_VCPU_STATE_RESET) {
			rc = VMM_THREAD_STATE_CREATED;
		} else if (state &
			  (VMM_VCPU_STATE_READY | VMM_VCPU_STATE_RUNNING)) {
			rc = VMM_THREAD_STATE_RUNNING;
		} else if (state & VMM_VCPU_STATE_PAUSED) {
			rc = VMM_THREAD_STATE_SLEEPING;
		} else if (state & VMM_VCPU_STATE_HALTED) {
			rc = VMM_THREAD_STATE_STOPPED;
		} else {
			rc = -1;
		}
	}

	return rc;
}

int vmm_threads_get_hcpu(struct vmm_thread *tinfo, u32 *hcpu)
{
	if (!tinfo || !hcpu) {
		return VMM_EFAIL;
	}

	return vmm_manager_vcpu_get_hcpu(tinfo->tvcpu, hcpu);
}

int vmm_thread_set_hcpu(struct vmm_thread *tinfo, u32 hcpu)
{
	if (!tinfo) {
		return VMM_EFAIL;
	}

	return vmm_manager_vcpu_set_hcpu(tinfo->tvcpu, hcpu);
}

const struct vmm_cpumask *vmm_threads_get_affinity(struct vmm_thread *tinfo)
{
	if (!tinfo) {
		return NULL;
	}

	return vmm_manager_vcpu_get_affinity(tinfo->tvcpu);
}

int vmm_threads_set_affinity(struct vmm_thread *tinfo,
			     const struct vmm_cpumask *cpu_mask)
{
	if (!tinfo || !cpu_mask) {
		return VMM_EFAIL;
	}

	return vmm_manager_vcpu_set_affinity(tinfo->tvcpu, cpu_mask);
}

struct vmm_thread *vmm_threads_id2thread(u32 tid)
{
	bool found;
	irq_flags_t flags;
	struct vmm_thread *tinfo;

	tinfo = NULL;
	found = FALSE;

	/* Lock threads control */
	vmm_spin_lock_irqsave(&thctrl.lock, flags);

	list_for_each_entry(tinfo, &thctrl.thread_list, head) {
		if (tinfo->tvcpu->id == tid) {
			found = TRUE;
			break;
		}
	}

	/* Unlock threads control */
	vmm_spin_unlock_irqrestore(&thctrl.lock, flags);

	if (!found) {
		return NULL;
	}

	return tinfo;
}

struct vmm_thread *vmm_threads_index2thread(int index)
{
	bool found;
	irq_flags_t flags;
	struct vmm_thread *tinfo;

	if (index < 0) {
		return NULL;
	}

	tinfo = NULL;
	found = FALSE;

	/* Lock threads control */
	vmm_spin_lock_irqsave(&thctrl.lock, flags);

	list_for_each_entry(tinfo, &thctrl.thread_list, head) {
		if (!index) {
			found = TRUE;
			break;
		}
		index--;
	}

	/* Unlock threads control */
	vmm_spin_unlock_irqrestore(&thctrl.lock, flags);

	if (!found) {
		return NULL;
	}

	return tinfo;
}

u32 vmm_threads_count(void)
{
	return thctrl.thread_count;
}

static void vmm_threads_entry(void)
{
	struct vmm_vcpu *vcpu = vmm_scheduler_current_vcpu();
	struct vmm_thread *tinfo = NULL;

	/* Sanity check */
	if (!vcpu) {
		vmm_panic("Error: Null vcpu at thread entry.\n");
	}

	/* Sanity check */
	tinfo = vmm_threads_id2thread(vcpu->id);
	if (!tinfo) {
		vmm_panic("Error: Null thread at thread entry.\n");
	}

	/* Enter the thread function */
	tinfo->tretval = tinfo->tfn(tinfo->tdata);

	/* Thread finished so, stop it. */
	vmm_threads_stop(tinfo);
	
	/* Nothing else to do for this thread.
	 * Let us hope someone else will destroy it.
	 * For now just hang. :( :(
         */
	vmm_hang();
}

struct vmm_thread *vmm_threads_create_rt(const char *thread_name,
					 int (*thread_fn) (void *udata),
					 void *thread_data,
					 u8 thread_priority,
				         u64 thread_nsecs,
					 u64 thread_deadline,
					 u64 thread_periodicity)
{
	irq_flags_t flags;
	struct vmm_thread *tinfo;

	/* Create thread structure instance */
	tinfo = vmm_malloc(sizeof(struct vmm_thread));
	if (!tinfo) {
		return NULL;
	}
	tinfo->tfn = thread_fn;
	tinfo->tdata = thread_data;
	tinfo->tnsecs = thread_nsecs;
	if (tinfo->tnsecs == 0) {
		tinfo->tnsecs = VMM_THREAD_DEF_TIME_SLICE;
	}
	tinfo->tdeadline = thread_deadline;
	if (tinfo->tdeadline < tinfo->tnsecs) {
		tinfo->tdeadline = tinfo->tnsecs;
	}
	tinfo->tperiodicity = thread_periodicity;
	if (tinfo->tperiodicity < tinfo->tdeadline) {
		tinfo->tperiodicity = tinfo->tdeadline;
	}

	/* Create an orphan vcpu for this thread */
	tinfo->tvcpu = vmm_manager_vcpu_orphan_create(thread_name,
			(virtual_addr_t)&vmm_threads_entry,
			CONFIG_THREAD_STACK_SIZE, thread_priority,
			thread_nsecs, thread_deadline, thread_periodicity);
	if (!tinfo->tvcpu) {
		vmm_free(tinfo);
		return NULL;
	}

	/* Lock threads control */
	vmm_spin_lock_irqsave(&thctrl.lock, flags);

	list_add_tail(&tinfo->head, &thctrl.thread_list);
	thctrl.thread_count++;

	/* Unlock threads control */
	vmm_spin_unlock_irqrestore(&thctrl.lock, flags);

	return tinfo;
}

int vmm_threads_destroy(struct vmm_thread *tinfo)
{
	int rc = VMM_OK;
	irq_flags_t flags;

	/* Sanity Check */
	if (!tinfo) {
		return VMM_EFAIL;
	}

	/* Lock threads control */
	vmm_spin_lock_irqsave(&thctrl.lock, flags);

	list_del(&tinfo->head);
	thctrl.thread_count--;

	/* Unlock threads control */
	vmm_spin_unlock_irqrestore(&thctrl.lock, flags);

	/* Destroy the thread VCPU */
	if ((rc = vmm_manager_vcpu_orphan_destroy(tinfo->tvcpu))) {
		return rc;
	}

	/* Free thread memory */
	vmm_free(tinfo);

	return VMM_OK;
}

int __init vmm_threads_init(void)
{
	memset(&thctrl, 0, sizeof(thctrl));

	INIT_SPIN_LOCK(&thctrl.lock);
	INIT_LIST_HEAD(&thctrl.thread_list);

	return VMM_OK;
}

