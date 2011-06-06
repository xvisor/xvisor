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
 * @file vmm_wait.c
 * @version 0.01
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief Wait functions.
 */

#include <vmm_stdio.h>
#include <vmm_types.h>
#include <vmm_error.h>
#include <vmm_cpu.h>
#include <vmm_hyperthreads.h>
#include <vmm_wait.h>

DEFINE_WAIT_LIST(global_wait_queue);

u32 add_to_wait_queue(vmm_wait_head_t * wait_list,
		      vmm_wait_element_t * wait_element)
{

	vmm_spin_lock(&wait_list->lock);
	list_add_tail(&wait_list->wait_list_head, &wait_element->list_head);

	vmm_spin_lock(&wait_element->thread->tlock);
	wait_element->thread->tstate = THREAD_STATE_RUNNING;
	vmm_spin_unlock(&wait_element->thread->tlock);

	vmm_spin_unlock(&wait_list->lock);

	return VMM_OK;
}

u32 remove_from_wait_queue(vmm_wait_head_t * wait_list,
			   vmm_wait_element_t * wait_element)
{
	vmm_spin_lock(&wait_list->lock);

	vmm_spin_lock(&wait_element->thread->tlock);
	wait_element->thread->tstate = THREAD_STATE_RUNNING;
	vmm_spin_unlock(&wait_element->thread->tlock);

	list_del(&wait_element->list_head);
	vmm_spin_unlock(&wait_list->lock);

	return VMM_OK;
}

u32 wake_up_on_queue(vmm_wait_head_t * wait_list)
{
	struct dlist *thead;
	vmm_hyperthread_t *tinfo;
	vmm_wait_element_t *welement;

	vmm_spin_lock(&wait_list->lock);
	list_for_each(thead, &wait_list->wait_list_head) {
		welement = container_of(thead, vmm_wait_element_t, list_head);

		if (welement) {
			tinfo = welement->thread;

			/* Set state to running */
			vmm_hyperthread_set_state(tinfo, THREAD_STATE_RUNNING);

			/* enqueue in scheduler's list */
			vmm_hypercore_sched_enqueue_thread(tinfo);
		}
	}
	vmm_spin_unlock(&wait_list->lock);

	return VMM_OK;
}
