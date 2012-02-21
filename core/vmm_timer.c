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

#include <arch_cpu.h>
#include <vmm_error.h>
#include <vmm_string.h>
#include <vmm_heap.h>
#include <vmm_timer.h>

/** Control structure for Timer Subsystem */
struct vmm_timer_ctrl {
	u64 cycles_last;
	u64 cycles_mask;
	u32 cycles_mult;
	u32 cycles_shift;
	u64 timestamp;
	bool cpu_started;
	bool cpu_inprocess;
	u64 cpu_next_event;
	struct vmm_timer_event *cpu_curr;
	struct dlist cpu_event_list;
	struct dlist event_list;
};

static struct vmm_timer_ctrl tctrl;

u64 vmm_timer_timestamp(void)
{
	u64 cycles_now, cycles_delta;
	u64 ns_offset;

	cycles_now = arch_cpu_clocksource_cycles();
	cycles_delta = (cycles_now - tctrl.cycles_last) & tctrl.cycles_mask;
	tctrl.cycles_last = cycles_now;

	ns_offset = (cycles_delta * tctrl.cycles_mult) >> tctrl.cycles_shift;
	tctrl.timestamp += ns_offset;

	return tctrl.timestamp;
}

#ifdef CONFIG_PROFILE
u64 __notrace vmm_timer_timestamp_for_profile(void)
{
	u64 cycles_now, cycles_delta;
	u64 ns_offset;

	cycles_now = arch_cpu_clocksource_cycles();
	cycles_delta = (cycles_now - tctrl.cycles_last) & tctrl.cycles_mask;
	ns_offset = (cycles_delta * tctrl.cycles_mult) >> tctrl.cycles_shift;

	return tctrl.timestamp + ns_offset;
}
#endif

static void vmm_timer_schedule_next_event(void)
{
	u64 tstamp;
	bool config_clockevent = FALSE;
	struct vmm_timer_event *e;

	/* If not started yet or still processing events then we give up */
	if (!tctrl.cpu_started || tctrl.cpu_inprocess) {
		return;
	}

	/* If no events, we give up */
	if (list_empty(&tctrl.cpu_event_list)) {
		return;
	}

	/* retrieve first timer in the list of active timers */
	e = list_entry(list_first(&tctrl.cpu_event_list), 
		       struct vmm_timer_event,
		       cpu_head);

	if (tctrl.cpu_curr != e) {
		/* The current event is not the one at the head of the list. */
		config_clockevent = TRUE;
	} else {
		/* The current event is one at the head of the list. 
		 * It is possible that expiry time of current event is made
		 * sooner, so we check and configure clockevent if required.
		 */
		if (e->expiry_tstamp <= tctrl.cpu_next_event) {
			config_clockevent = TRUE;
		}
	}

	if (config_clockevent) {
		tctrl.cpu_curr = e;
		tstamp = vmm_timer_timestamp();
		if (tstamp < e->expiry_tstamp) {
			tctrl.cpu_next_event = e->expiry_tstamp;
			arch_cpu_clockevent_start(e->expiry_tstamp - tstamp);
		} else {
			tctrl.cpu_next_event = tstamp;
			arch_cpu_clockevent_expire();
		}
	}
}

/**
 * This is call from interrupt context. So we don't need to protect the list
 * when manipulating it.
 */
void vmm_timer_clockevent_process(arch_regs_t * regs)
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

	/* trigger a timer interrupt */
	arch_cpu_clockevent_expire();

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
	arch_cpu_clockevent_start(1000000);

	tctrl.cpu_next_event = vmm_timer_timestamp() + 1000000;

	tctrl.cpu_started = TRUE;
}

void vmm_timer_stop(void)
{
	arch_cpu_clockevent_stop();

	tctrl.cpu_started = FALSE;
}

int __init vmm_timer_init(void)
{
	int rc;

	/* Initialize Per CPU event status */
	tctrl.cpu_started = FALSE;
	tctrl.cpu_inprocess = FALSE;

	/* Initialize Per CPU current event pointer */
	tctrl.cpu_curr = NULL;

	/* Initialize Per CPU event list */
	INIT_LIST_HEAD(&tctrl.cpu_event_list);

	/* Initialize event list */
	INIT_LIST_HEAD(&tctrl.event_list);

	/* Initialize cpu specific timer event */
	if ((rc = arch_cpu_clockevent_init())) {
		return rc;
	}

	/* Initialize cpu specific timer cycle counter */
	if ((rc = arch_cpu_clocksource_init())) {
		return rc;
	}

	/* Setup configuration for reading cycle counter */
	tctrl.cycles_mask = arch_cpu_clocksource_mask();
	tctrl.cycles_mult = arch_cpu_clocksource_mult();
	tctrl.cycles_shift = arch_cpu_clocksource_shift();
	tctrl.cycles_last = arch_cpu_clocksource_cycles();

	/* Starting value of timestamp */
	tctrl.timestamp = 0x0;

	return VMM_OK;
}
