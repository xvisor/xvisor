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

#include <vmm_error.h>
#include <vmm_smp.h>
#include <vmm_percpu.h>
#include <vmm_spinlocks.h>
#include <vmm_heap.h>
#include <vmm_stdio.h>
#include <vmm_clocksource.h>
#include <vmm_clockchip.h>
#include <vmm_timer.h>
#include <arch_cpu_irq.h>
#include <libs/stringlib.h>

/** Control structure for Timer Subsystem */
struct vmm_timer_local_ctrl {
	struct vmm_timecounter tc;
	struct vmm_clockchip *cc;
	bool started;
	bool inprocess;
	u64 next_event;
	struct vmm_timer_event *curr;
	struct dlist event_list;
};

static DEFINE_PER_CPU(struct vmm_timer_local_ctrl, tlc);

#if defined(CONFIG_PROFILE)
u64 __notrace vmm_timer_timestamp_for_profile(void)
{
	return vmm_timecounter_read_for_profile(&(this_cpu(tlc).tc));
}
#endif

u64 vmm_timer_timestamp(void)
{
	u64 ret;
	irq_flags_t flags;

	arch_cpu_irq_save(flags);
	ret = vmm_timecounter_read(&(this_cpu(tlc).tc));
	arch_cpu_irq_restore(flags);

	return ret;
}

static void vmm_timer_schedule_next_event(void)
{
	u64 tstamp;
	struct vmm_timer_event *e;
	struct vmm_timer_local_ctrl *tlcp = &this_cpu(tlc);

	/* If not started yet or still processing events then we give up */
	if (!tlcp->started || tlcp->inprocess) {
		return;
	}

	/* If no events, we give up */
	if (list_empty(&tlcp->event_list)) {
		return;
	}

	/* Retrieve first event from list of active events */
	e = list_entry(list_first(&tlcp->event_list), 
		       struct vmm_timer_event,
		       head);

	/* Configure clockevent device for first event */
	tlcp->curr = e;
	tstamp = vmm_timer_timestamp();
	if (tstamp < e->expiry_tstamp) {
		tlcp->next_event = e->expiry_tstamp;
		vmm_clockchip_program_event(tlcp->cc, 
				    tstamp, e->expiry_tstamp);
	} else {
		tlcp->next_event = tstamp;
		vmm_clockchip_force_expiry(tlcp->cc, tstamp);
	}
}

/**
 * This is call from interrupt context. So we don't need to protect the list
 * when manipulating it.
 */
static void timer_clockchip_event_handler(struct vmm_clockchip *cc,
					  arch_regs_t *regs)
{
	struct vmm_timer_event *e;
	struct vmm_timer_local_ctrl *tlcp = &this_cpu(tlc);

	tlcp->inprocess = TRUE;

	/* process expired active events */
	while (!list_empty(&tlcp->event_list)) {
		e = list_entry(list_first(&tlcp->event_list),
			       struct vmm_timer_event, head);
		/* Current timestamp */
		if (e->expiry_tstamp <= vmm_timer_timestamp()) {
			/* Set current CPU event to NULL */
			tlcp->curr = NULL;
			/* consume expired active events */
			list_del(&e->head);
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

	tlcp->inprocess = FALSE;

	/* Schedule next timer event */
	vmm_timer_schedule_next_event();
}

int vmm_timer_event_start(struct vmm_timer_event *ev, u64 duration_nsecs)
{
	bool added;
	irq_flags_t flags;
	struct dlist *l;
	struct vmm_timer_event *e;
	u64 tstamp;
	struct vmm_timer_local_ctrl *tlcp = &this_cpu(tlc);

	if (!ev) {
		return VMM_EFAIL;
	}

	tstamp = vmm_timer_timestamp();

	arch_cpu_irq_save(flags);

	if (ev->active) {
		/*
		 * if the timer event is already started, we remove it from
		 * the active list because it has changed.
		 */
		list_del(&ev->head);
	}

	ev->expiry_tstamp = tstamp + duration_nsecs;
	ev->duration_nsecs = duration_nsecs;
	ev->active = TRUE;
	added = FALSE;
	e = NULL;
	list_for_each(l, &tlcp->event_list) {
		e = list_entry(l, struct vmm_timer_event, head);
		if (ev->expiry_tstamp < e->expiry_tstamp) {
			list_add_tail(&ev->head, &e->head);
			added = TRUE;
			break;
		}
	}

	if (!added) {
		list_add_tail(&ev->head, &tlcp->event_list);
	}

	vmm_timer_schedule_next_event();

	arch_cpu_irq_restore(flags);

	return VMM_OK;
}

int vmm_timer_event_restart(struct vmm_timer_event *ev)
{
	if (!ev) {
		return VMM_EFAIL;
	}

	return vmm_timer_event_start(ev, ev->duration_nsecs);
}

int vmm_timer_event_expire(struct vmm_timer_event *ev)
{
	irq_flags_t flags;
	struct vmm_timer_local_ctrl *tlcp = &this_cpu(tlc);

	if (!ev) {
		return VMM_EFAIL;
	}

	/* prevent (timer) interrupt */
	arch_cpu_irq_save(flags);

	/* if the event is already engaged */
	if (ev->active) {
		/* We remove it from the list */
		list_del(&ev->head);
	}

	/* set the expiry_tstamp to before now */
	ev->expiry_tstamp = 0;
	ev->active = TRUE;

	/* add the event on list head as it is going to expire now */
	list_add(&ev->head, &tlcp->event_list);

	/* Force a clockchip interrupt */
	vmm_clockchip_force_expiry(tlcp->cc, vmm_timer_timestamp());

	/* allow (timer) interrupts */
	arch_cpu_irq_restore(flags);

	return VMM_OK;
}

int vmm_timer_event_stop(struct vmm_timer_event *ev)
{
	irq_flags_t flags;

	if (!ev) {
		return VMM_EFAIL;
	}

	arch_cpu_irq_save(flags);

	ev->expiry_tstamp = 0;

	if (ev->active) {
		list_del(&ev->head);
		ev->active = FALSE;
		vmm_timer_schedule_next_event();
	}

	arch_cpu_irq_restore(flags);

	return VMM_OK;
}

void vmm_timer_start(void)
{
	u64 tstamp;
	struct vmm_timer_local_ctrl *tlcp = &this_cpu(tlc);

	vmm_clockchip_set_mode(tlcp->cc, VMM_CLOCKCHIP_MODE_ONESHOT);

	tstamp = vmm_timer_timestamp();

	tlcp->next_event = tstamp + tlcp->cc->min_delta_ns;

	tlcp->started = TRUE;

	vmm_clockchip_program_event(tlcp->cc, tstamp, tlcp->next_event);
}

void vmm_timer_stop(void)
{
	struct vmm_timer_local_ctrl *tlcp = &this_cpu(tlc);

	vmm_clockchip_set_mode(tlcp->cc, VMM_CLOCKCHIP_MODE_SHUTDOWN);

	tlcp->started = FALSE;
}

int __cpuinit vmm_timer_init(void)
{
	int rc;
	u32 cpu = vmm_smp_processor_id();
	struct vmm_clocksource *cs;
	struct vmm_timer_local_ctrl *tlcp = &this_cpu(tlc);

	/* Clear timer control structure */
	memset(tlcp, 0, sizeof(*tlcp));

	/* Initialize Per CPU event status */
	tlcp->started = FALSE;
	tlcp->inprocess = FALSE;

	/* Initialize Per CPU current event pointer */
	tlcp->curr = NULL;

	/* Initialize Per CPU event list */
	INIT_LIST_HEAD(&tlcp->event_list);

	/* Find suitable clockchip */
	if (!(tlcp->cc = vmm_clockchip_find_best(vmm_cpumask_of(cpu)))) {
		vmm_panic("%s: No clockchip for CPU%d\n", __func__, cpu);
	}

	/* Update event handler of clockchip */
	vmm_clockchip_set_event_handler(tlcp->cc, 
					&timer_clockchip_event_handler);

	if (!cpu) {
		/* Find suitable clocksource */
		if (!(cs = vmm_clocksource_best())) {
			vmm_panic("%s: No clocksource found\n", __func__);
		}

		/* Initialize timecounter wrapper */
		if ((rc = vmm_timecounter_init(&tlcp->tc, cs, 0))) {
			return rc;
		}

		/* Start timecounter */
		if ((rc = vmm_timecounter_start(&tlcp->tc))) {
			return rc;
		}
	} else {
		/* Initialize timecounter wrapper of secondary CPUs
		 * such that time stamps visible on all CPUs is same;
		 */
		if ((rc = vmm_timecounter_init(&tlcp->tc, 
			per_cpu(tlc, 0).tc.cs, 
			vmm_timecounter_read(&per_cpu(tlc, 0).tc)))) {
			return rc;
		}
	}

	return VMM_OK;
}
