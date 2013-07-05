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
	vmm_spinlock_t event_list_lock;
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

static void timer_schedule_next_event(struct vmm_timer_local_ctrl *tlcp)
{
	u64 tstamp;
	struct vmm_timer_event *e;

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
		vmm_clockchip_program_event(tlcp->cc, tstamp, tstamp);
	}
}

/* This is called from interrupt context. We need to protect the 
 * event list when manipulating it.
 */
static void timer_clockchip_event_handler(struct vmm_clockchip *cc)
{
	irq_flags_t flags;
	struct vmm_timer_event *e;
	struct vmm_timer_local_ctrl *tlcp = &this_cpu(tlc);

	vmm_spin_lock_irqsave_lite(&tlcp->event_list_lock, flags);

	tlcp->inprocess = TRUE;

	/* process expired active events */
	while (!list_empty(&tlcp->event_list)) {
		e = list_entry(list_first(&tlcp->event_list),
			       struct vmm_timer_event, head);
		/* Current timestamp */
		if (e->expiry_tstamp <= vmm_timer_timestamp()) {
			/* Set current CPU event to NULL */
			tlcp->curr = NULL;
			/* Consume expired active events */
			list_del(&e->head);
			e->expiry_tstamp = 0;
			e->active = FALSE;
			vmm_spin_unlock_irqrestore_lite(&tlcp->event_list_lock, flags);
			e->handler(e);
			vmm_spin_lock_irqsave_lite(&tlcp->event_list_lock, flags);
		} else {
			/* no more expired events */
			break;
		}
	}

	tlcp->inprocess = FALSE;

	/* Schedule next timer event */
	timer_schedule_next_event(tlcp);

	vmm_spin_unlock_irqrestore_lite(&tlcp->event_list_lock, flags);
}

int vmm_timer_event_start(struct vmm_timer_event *ev, u64 duration_nsecs)
{
	u64 tstamp;
	bool added;
	irq_flags_t flags, flags1;
	struct dlist *l;
	struct vmm_timer_event *e;
	u32 hcpu = vmm_smp_processor_id();
	struct vmm_timer_local_ctrl *tt, *tlcp = &per_cpu(tlc, hcpu);

	if (!ev) {
		return VMM_EFAIL;
	}

	vmm_spin_lock_irqsave_lite(&tlcp->event_list_lock, flags);

	if (ev->active) {
		if (ev->active_hcpu != hcpu) {
			tt = &per_cpu(tlc, ev->active_hcpu);
			vmm_spin_lock_irqsave_lite(&tt->event_list_lock, 
						   flags1);
			list_del(&ev->head);
			ev->active = FALSE;
			ev->active_hcpu = 0;
			ev->expiry_tstamp = 0;
			vmm_spin_unlock_irqrestore_lite(&tt->event_list_lock, 
							flags1);
		} else {
			list_del(&ev->head);
			ev->active = FALSE;
			ev->active_hcpu = 0;
			ev->expiry_tstamp = 0;
		}
	}

	tstamp = vmm_timer_timestamp();

	ev->expiry_tstamp = tstamp + duration_nsecs;
	ev->duration_nsecs = duration_nsecs;
	ev->active = TRUE;
	ev->active_hcpu = hcpu;
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

	timer_schedule_next_event(tlcp);

	vmm_spin_unlock_irqrestore_lite(&tlcp->event_list_lock, flags);

	return VMM_OK;
}

int vmm_timer_event_restart(struct vmm_timer_event *ev)
{
	if (!ev) {
		return VMM_EFAIL;
	}

	return vmm_timer_event_start(ev, ev->duration_nsecs);
}

int vmm_timer_event_stop(struct vmm_timer_event *ev)
{
	irq_flags_t flags;
	struct vmm_timer_local_ctrl *tlcp;

	if (!ev) {
		return VMM_EFAIL;
	}

	if (ev->active) {
		tlcp = &per_cpu(tlc, ev->active_hcpu);
		vmm_spin_lock_irqsave_lite(&tlcp->event_list_lock, flags);

		list_del(&ev->head);
		ev->active = FALSE;
		ev->active_hcpu = 0;
		ev->expiry_tstamp = 0;

		vmm_spin_unlock_irqrestore_lite(&tlcp->event_list_lock, flags);
	}

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
	INIT_SPIN_LOCK(&tlcp->event_list_lock);
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
