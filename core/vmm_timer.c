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
 * @file vmm_timer.c
 * @version 0.1
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of timer subsystem
 */

#include <vmm_cpu.h>
#include <vmm_error.h>
#include <vmm_string.h>
#include <vmm_heap.h>
#include <vmm_devtree.h>
#include <vmm_timer.h>

vmm_timer_ctrl_t tctrl;

void vmm_timer_tick_process(vmm_user_regs_t * regs)
{
	struct dlist *l;
	vmm_timer_event_t *e, *de;

	/* Increment timestamp */
	tctrl.timestamp += tctrl.tick_nsecs;

	/* Update active events */
	e = NULL;
	de = NULL;
	list_for_each(l, &tctrl.cpu_event_list) {
		if (de) {
			list_del(&de->cpu_head);
			de->on_cpu_list = FALSE;
			de = NULL;
		}
		e = list_entry(l, vmm_timer_event_t, cpu_head);
		if (!e->active) {
			de = e;
			continue;
		}
		if (tctrl.tick_nsecs < e->pending_nsecs) {
			e->pending_nsecs -= tctrl.tick_nsecs;
		} else {
			e->pending_nsecs = 0;
			e->active = FALSE;
			e->cpu_regs = regs;
			e->handler(e);
			e->cpu_regs = NULL;
			if (!e->active) {
				de = e;
			}
		}
	}
	if (de) {
		list_del(&de->cpu_head);
		de->on_cpu_list = FALSE;
	}
}

u64 vmm_timer_timestamp(void)
{
	return tctrl.timestamp;
}

int vmm_timer_adjust_timestamp(u64 timestamp)
{
	if (timestamp < tctrl.timestamp) {
		return VMM_EFAIL;
	}

	tctrl.timestamp = timestamp;

	return VMM_OK;
}

int vmm_timer_event_start(vmm_timer_event_t * ev, u64 duration_nsecs)
{
	if (!ev) {
		return VMM_EFAIL;
	}

	ev->active = TRUE;
	ev->pending_nsecs = duration_nsecs;
	ev->duration_nsecs = duration_nsecs;
	if (!ev->on_cpu_list) {
		list_add_tail(&tctrl.cpu_event_list, &ev->cpu_head);
		ev->on_cpu_list = TRUE;
	}

	return VMM_OK;
}

int vmm_timer_event_restart(vmm_timer_event_t * ev)
{
	if (!ev) {
		return VMM_EFAIL;
	}

	ev->active = TRUE;
	ev->pending_nsecs = ev->duration_nsecs;
	if (!ev->on_cpu_list) {
		list_add_tail(&tctrl.cpu_event_list, &ev->cpu_head);
		ev->on_cpu_list = TRUE;
	}

	return VMM_OK;
}

int vmm_timer_event_stop(vmm_timer_event_t * ev)
{
	if (!ev) {
		return VMM_EFAIL;
	}

	ev->active = FALSE;
	ev->pending_nsecs = 0;
	if (ev->on_cpu_list) {
		list_del(&ev->cpu_head);
		ev->on_cpu_list = FALSE;
	}

	return VMM_OK;
}

vmm_timer_event_t * vmm_timer_event_create(const char *name,
					   vmm_timer_event_handler_t handler,
					   void * priv)
{
	bool found;
	struct dlist *l;
	vmm_timer_event_t *e;

	e = NULL;
	found = FALSE;
	list_for_each(l, &tctrl.event_list) {
		e = list_entry(l, vmm_timer_event_t, head);
		if (vmm_strcmp(name, e->name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (found) {
		return NULL;
	}

	e = vmm_malloc(sizeof(vmm_timer_event_t));
	if (!e) {
		return NULL;
	}

	INIT_LIST_HEAD(&e->head);
	vmm_strcpy(e->name, name);
	e->active = FALSE;
	e->on_cpu_list = FALSE;
	INIT_LIST_HEAD(&e->cpu_head);
	e->cpu_regs = NULL;
	e->pending_nsecs = 0;
	e->duration_nsecs = 0;
	e->handler = handler;
	e->priv = priv;

	list_add_tail(&tctrl.event_list, &e->head);

	return e;
}

int vmm_timer_event_destroy(vmm_timer_event_t * ev)
{
	bool found;
	struct dlist *l;
	vmm_timer_event_t *e;

	if (!ev) {
		return VMM_EFAIL;
	}

	if (list_empty(&tctrl.event_list)) {
		return VMM_EFAIL;
	}

	e = NULL;
	found = FALSE;
	list_for_each(l, &tctrl.event_list) {
		e = list_entry(l, vmm_timer_event_t, head);
		if (vmm_strcmp(e->name, ev->name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (!found) {
		return VMM_ENOTAVAIL;
	}

	list_del(&e->head);

	vmm_free(e);

	return VMM_OK;
}

vmm_timer_event_t *vmm_timer_event_find(const char *name)
{
	bool found;
	struct dlist *l;
	vmm_timer_event_t *e;

	if (!name) {
		return NULL;
	}

	found = FALSE;
	e = NULL;

	list_for_each(l, &tctrl.event_list) {
		e = list_entry(l, vmm_timer_event_t, head);
		if (vmm_strcmp(e->name, name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (!found) {
		return NULL;
	}

	return e;
}

vmm_timer_event_t *vmm_timer_event_get(int index)
{
	bool found;
	struct dlist *l;
	vmm_timer_event_t *ret;

	if (index < 0) {
		return NULL;
	}

	ret = NULL;
	found = FALSE;

	list_for_each(l, &tctrl.event_list) {
		ret = list_entry(l, vmm_timer_event_t, head);
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

u32 vmm_timer_event_count(void)
{
	u32 retval = 0;
	struct dlist *l;

	list_for_each(l, &tctrl.event_list) {
		retval++;
	}

	return retval;
}

u64 vmm_timer_tick_usecs(void)
{
	return tctrl.tick_nsecs / (u64)1000;
}

u64 vmm_timer_tick_nsecs(void)
{
	return tctrl.tick_nsecs;
}

void vmm_timer_start(void)
{
	/** Setup timer */
	vmm_cpu_timer_setup(tctrl.tick_nsecs);

	/** Enable timer */
	vmm_cpu_timer_enable();
}

void vmm_timer_stop(void)
{
	/** Disable timer */
	vmm_cpu_timer_disable();
}

int vmm_timer_init(void)
{
	const char *attrval;
	vmm_devtree_node_t *vnode;

	/* Set timestamp to zero */
	tctrl.timestamp = 0;

	/* Find out tick delay in nanoseconds */
	vnode = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPRATOR_STRING
				   VMM_DEVTREE_VMMINFO_NODE_NAME);
	if (!vnode) {
		return VMM_EFAIL;
	}
	attrval = vmm_devtree_attrval(vnode,
				      VMM_DEVTREE_TICK_DELAY_NSECS_ATTR_NAME);
	if (!attrval) {
		return VMM_EFAIL;
	}
	tctrl.tick_nsecs = *((u32 *) attrval);

	/* Initialize Per CPU event list */
	INIT_LIST_HEAD(&tctrl.cpu_event_list);

	/* Initialize event list */
	INIT_LIST_HEAD(&tctrl.event_list);

	return VMM_OK;
}



