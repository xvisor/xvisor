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
 * @version 0.01
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @author Anup Patel (anup@brainfault.org)
 * @brief Source file for hypervisor threads. These run on top of vcpus.
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_string.h>
#include <vmm_stdio.h>
#include <vmm_spinlocks.h>
#include <vmm_scheduler.h>
#include <vmm_threads.h>

vmm_threads_ctrl_t thctrl;

static void vmm_threads_entry(void)
{
	vmm_vcpu_t * vcpu = vmm_scheduler_current_vcpu();
	vmm_thread_t * tinfo = NULL;

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
	while (1);
}

vmm_thread_t *vmm_threads_create(const char *thread_name, 
				 vmm_thread_func_t thread_fn,
				 void *thread_data,
				 u64 thread_nsecs)
{
	irq_flags_t flags;
	vmm_thread_t * tinfo;

	/* Create thread structure instance */
	tinfo = vmm_malloc(sizeof(vmm_thread_t));
	if (!tinfo) {
		return NULL;
	}
	tinfo->tfn = thread_fn;
	tinfo->tdata = thread_data;
	tinfo->tnsecs = thread_nsecs;
	vmm_memset(&tinfo->tstack, 0, VMM_THREAD_STACK_SZ);

	/* Create an orphan vcpu for this thread */
	tinfo->tvcpu = vmm_manager_vcpu_orphan_create(thread_name,
			(virtual_addr_t)&vmm_threads_entry,
			(virtual_addr_t)&tinfo->tstack[VMM_THREAD_STACK_SZ - 4],
			thread_nsecs);
	if (!tinfo->tvcpu) {
		vmm_free(tinfo);
		return NULL;
	}

	/* Lock threads control */
	flags = vmm_spin_lock_irqsave(&thctrl.lock);

	list_add_tail(&thctrl.thread_list, &tinfo->head);
	thctrl.thread_count++;

	/* Unlock threads control */
	vmm_spin_unlock_irqrestore(&thctrl.lock, flags);

	return tinfo;
}

int vmm_threads_destroy(vmm_thread_t * tinfo)
{
	/* FIXME: TBD */
	return VMM_OK;
}

int vmm_threads_start(vmm_thread_t * tinfo)
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

int vmm_threads_stop(vmm_thread_t * tinfo)
{
	/* FIXME: TBD */
	return VMM_OK;
}

u32 vmm_threads_get_id(vmm_thread_t * tinfo)
{
	if (!tinfo) {
		return 0;
	}

	return tinfo->tvcpu->id;
}

int vmm_threads_get_name(char * dst, vmm_thread_t * tinfo)
{
	if (!tinfo || !dst) {
		return VMM_EFAIL;
	}

	vmm_strcpy(dst, tinfo->tvcpu->name);

	return VMM_OK;
}

int vmm_threads_get_state(vmm_thread_t * tinfo)
{
	int rc = -1;

	if (!tinfo) {
		rc =  -1;
	} else {
		if (tinfo->tvcpu->state & VMM_VCPU_STATE_RESET) { 
			rc = VMM_THREAD_STATE_CREATED;
		} else if (tinfo->tvcpu->state & 
			  (VMM_VCPU_STATE_READY | VMM_VCPU_STATE_RUNNING)) {
			rc = VMM_THREAD_STATE_RUNNING;
		} else if (tinfo->tvcpu->state & 
			  (VMM_VCPU_STATE_PAUSED | VMM_VCPU_STATE_HALTED)) {
			rc = VMM_THREAD_STATE_STOPPED;
		} else {
			rc = -1;
		}
	}

	return rc;
}

vmm_thread_t *vmm_threads_id2thread(u32 tid)
{
	bool found;
	struct dlist *l;
	vmm_thread_t *ret;

	ret = NULL;
	found = FALSE;

	list_for_each(l, &thctrl.thread_list) {
		ret = list_entry(l, vmm_thread_t, head);
		if (ret->tvcpu->id == tid) {
			found = TRUE;
			break;
		}
	}

	if (!found) {
		return NULL;
	}

	return ret;
}

vmm_thread_t *vmm_threads_index2thread(int index)
{
	bool found;
	struct dlist *l;
	vmm_thread_t *ret;

	if (index < 0) {
		return NULL;
	}

	ret = NULL;
	found = FALSE;

	list_for_each(l, &thctrl.thread_list) {
		ret = list_entry(l, vmm_thread_t, head);
		if (!index) {
			found = TRUE;
			break;
		}
		index--;
	}

	if (!found) {
		return NULL;
	}

	return ret;
}

u32 vmm_threads_count(void)
{
	return thctrl.thread_count;
}

int vmm_threads_init(void)
{
	vmm_memset(&thctrl, 0, sizeof(vmm_threads_ctrl_t));

	INIT_SPIN_LOCK(&thctrl.lock);
	INIT_LIST_HEAD(&thctrl.thread_list);

	return VMM_OK;
}

