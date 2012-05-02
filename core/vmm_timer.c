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
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of timer subsystem
 */

#include <arch_cpu_irq.h>
#include <vmm_error.h>
#include <vmm_string.h>
#include <vmm_heap.h>
#include <vmm_stdio.h>
#include <vmm_clocksource.h>
#include <vmm_clockchip.h>
#include <vmm_timer.h>

/** Control structure for Timer Subsystem */
struct vmm_timer_ctrl {
	struct vmm_timecounter cpu_tc;
	struct vmm_clockchip *cpu_cc;
	bool cpu_started;
	bool cpu_inprocess;
	u64 cpu_next_event;
	struct vmm_timer_event *cpu_curr;
	struct dlist cpu_event_list;
	struct dlist event_list;
};

static struct vmm_timer_ctrl tctrl;

#ifdef CONFIG_PROFILE
u64 __notrace vmm_timer_timestamp_for_profile(void)
#else
u64 vmm_timer_timestamp(void)
#endif
{
	return vmm_timecounter_read(&tctrl.cpu_tc);
}

static void vmm_timer_schedule_next_event(void)
{
	u64 tstamp;
	struct vmm_timer_event *e;

	/* If not started yet or still processing events then we give up */
	if (!tctrl.cpu_started || tctrl.cpu_inprocess) {
		return;
	}

	/* If no events, we give up */
	if (list_empty(&tctrl.cpu_event_list)) {
		return;
	}

	/* Retrieve first event from list of active events */
	e = list_entry(list_first(&tctrl.cpu_event_list), 
		       struct vmm_timer_event,
		       cpu_head);

	/* Configure clockevent device for first event */
	tctrl.cpu_curr = e;
	tstamp = vmm_timer_timestamp();
	if (tstamp < e->expiry_tstamp) {
		tctrl.cpu_next_event = e->expiry_tstamp;
		vmm_clockchip_program_event(tctrl.cpu_cc, 
				    tstamp, e->expiry_tstamp);
	} else {
		tctrl.cpu_next_event = tstamp;
		vmm_clockchip_force_expiry(tctrl.cpu_cc, tstamp);
	}
}

/**
 * This is call from interrupt context. So we don't need to protect the list
 * when manipulating it.
 */
static void timer_clockchip_event_handler(struct vmm_clockchip *cc,
					  arch_regs_t * regs)
{
	struct vmm_timer_event *e;

	tctrl.cpu_inprocess = TRUE;

	/* process expired active events */
	while (!list_empty(&tctrl.cpu_event_list)) {
		e = list_entry(list_first(&tctrl.cpu_event_list),
			       struct vmm_timer_event, cpu_head);
		/* Current timestamp */
		if (e->expiry_tstamp <= vmm_timer_timestamp()) {
			/* Set current CPU event to NULL */
			tctrl.cpu_curr = NULL;
			/* consume expired active events */
			list_del(&e->cpu_head);
			e->expiry_tstamp = 0;
			e->active = FALSE;
			e->regs = regs;
			e->handler(e);
			e->regs = NULL;
		} else {
			/* no more expired events */
			break;
		}
	}

	tctrl.cpu_inprocess = FALSE;

	/* Schedule next timer event */
	vmm_timer_schedule_next_event();
}

int vmm_timer_event_start(struct vmm_timer_event * ev, u64 duration_nsecs)
{
	bool added;
	irq_flags_t flags;
	struct dlist *l;
	struct vmm_timer_event *e;
	u64 tstamp;

	if (!ev) {
		return VMM_EFAIL;
	}

	tstamp = vmm_timer_timestamp();

	flags = arch_cpu_irq_save();

	if (ev->active) {
		/*
		 * if the timer event is already started, we remove it from
		 * the active list because it has changed.
		 */
		list_del(&ev->cpu_head);
	}

	ev->expiry_tstamp = tstamp + duration_nsecs;
	ev->duration_nsecs = duration_nsecs;
	ev->active = TRUE;
	added = FALSE;
	e = NULL;
	list_for_each(l, &tctrl.cpu_event_list) {
		e = list_entry(l, struct vmm_timer_event, cpu_head);
		if (ev->expiry_tstamp < e->expiry_tstamp) {
			list_add_tail(&e->cpu_head, &ev->cpu_head);
			added = TRUE;
			break;
		}
	}

	if (!added) {
		list_add_tail(&tctrl.cpu_event_list, &ev->cpu_head);
	}

	vmm_timer_schedule_next_event();

	arch_cpu_irq_restore(flags);

	return VMM_OK;
}

int vmm_timer_event_restart(struct vmm_timer_event * ev)
{
	if (!ev) {
		return VMM_EFAIL;
	}

	return vmm_timer_event_start(ev, ev->duration_nsecs);
}

int vmm_timer_event_expire(struct vmm_timer_event * ev)
{
	irq_flags_t flags;

	if (!ev) {
		return VMM_EFAIL;
	}

	/* prevent (timer) interrupt */
	flags = arch_cpu_irq_save();

	/* if the event is already engaged */
	if (ev->active) {
		/* We remove it from the list */
		list_del(&ev->cpu_head);
	}

	/* set the expiry_tstamp to before now */
	ev->expiry_tstamp = 0;
	ev->active = TRUE;

	/* add the event on list head as it is going to expire now */
	list_add(&tctrl.cpu_event_list, &ev->cpu_head);

	/* Force a clockchip interrupt */
	vmm_clockchip_force_expiry(tctrl.cpu_cc, vmm_timer_timestamp());

	/* allow (timer) interrupts */
	arch_cpu_irq_restore(flags);

	return VMM_OK;
}

int vmm_timer_event_stop(struct vmm_timer_event * ev)
{
	irq_flags_t flags;

	if (!ev) {
		return VMM_EFAIL;
	}

	flags = arch_cpu_irq_save();

	ev->expiry_tstamp = 0;

	if (ev->active) {
		list_del(&ev->cpu_head);
		ev->active = FALSE;
	}

	vmm_timer_schedule_next_event();

	arch_cpu_irq_restore(flags);

	return VMM_OK;
}

struct vmm_timer_event *vmm_timer_event_create(const char *name,
					vmm_timer_event_handler_t handler,
					void *priv)
{
	bool found;
	struct dlist *l;
	struct vmm_timer_event *e;

	e = NULL;
	found = FALSE;
	list_for_each(l, &tctrl.event_list) {
		e = list_entry(l, struct vmm_timer_event, head);
		if (vmm_strcmp(name, e->name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (found) {
		return NULL;
	}

	e = vmm_malloc(sizeof(struct vmm_timer_event));
	if (!e) {
		return NULL;
	}

	INIT_LIST_HEAD(&e->head);
	vmm_strcpy(e->name, name);
	e->active = FALSE;
	INIT_LIST_HEAD(&e->cpu_head);
	e->regs = NULL;
	e->expiry_tstamp = 0;
	e->duration_nsecs = 0;
	e->handler = handler;
	e->priv = priv;

	list_add_tail(&tctrl.event_list, &e->head);

	return e;
}

int vmm_timer_event_destroy(struct vmm_timer_event * ev)
{
	bool found;
	struct dlist *l;
	struct vmm_timer_event *e;

	if (!ev) {
		return VMM_EFAIL;
	}

	if (list_empty(&tctrl.event_list)) {
		return VMM_EFAIL;
	}

	e = NULL;
	found = FALSE;
	list_for_each(l, &tctrl.event_list) {
		e = list_entry(l, struct vmm_timer_event, head);
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

struct vmm_timer_event *vmm_timer_event_find(const char *name)
{
	bool found;
	struct dlist *l;
	struct vmm_timer_event *e;

	if (!name) {
		return NULL;
	}

	found = FALSE;
	e = NULL;

	list_for_each(l, &tctrl.event_list) {
		e = list_entry(l, struct vmm_timer_event, head);
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

struct vmm_timer_event *vmm_timer_event_get(int index)
{
	bool found;
	struct dlist *l;
	struct vmm_timer_event *ret;

	if (index < 0) {
		return NULL;
	}

	ret = NULL;
	found = FALSE;

	list_for_each(l, &tctrl.event_list) {
		ret = list_entry(l, struct vmm_timer_event, head);
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

void vmm_timer_start(void)
{
	u64 tstamp;

	vmm_clockchip_set_mode(tctrl.cpu_cc, VMM_CLOCKCHIP_MODE_ONESHOT);

	tstamp = vmm_timer_timestamp();

	tctrl.cpu_next_event = tstamp + 1000000;

	vmm_clockchip_program_event(tctrl.cpu_cc, 
				    tstamp, tctrl.cpu_next_event);

	tctrl.cpu_started = TRUE;
}

void vmm_timer_stop(void)
{
	vmm_clockchip_set_mode(tctrl.cpu_cc, VMM_CLOCKCHIP_MODE_SHUTDOWN);

	tctrl.cpu_started = FALSE;
}

int __init vmm_timer_init(void)
{
	struct vmm_clocksource * cs;

	/* Clear timer control structure */
	vmm_memset(&tctrl, 0, sizeof(tctrl));

	/* Initialize Per CPU event status */
	tctrl.cpu_started = FALSE;
	tctrl.cpu_inprocess = FALSE;

	/* Initialize Per CPU current event pointer */
	tctrl.cpu_curr = NULL;

	/* Initialize Per CPU event list */
	INIT_LIST_HEAD(&tctrl.cpu_event_list);

	/* Initialize event list */
	INIT_LIST_HEAD(&tctrl.event_list);

	/* Find suitable clockchip */
	if (!(tctrl.cpu_cc = vmm_clockchip_best())) {
		vmm_panic("%s: No clockchip found\n", __func__);
	}

	/* Update event handler of clockchip */
	vmm_clockchip_set_event_handler(tctrl.cpu_cc, 
					&timer_clockchip_event_handler);

	/* Find suitable clocksource */
	if (!(cs = vmm_clocksource_best())) {
		vmm_panic("%s: No clocksource found\n", __func__);
	}

	return vmm_timecounter_init(&tctrl.cpu_tc, cs, 0);
}
