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
	vmm_rwlock_t event_list_lock;
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

/* Note: This function must be called with tlcp->event_list_lock held. */
static void __timer_schedule_next_event(struct vmm_timer_local_ctrl *tlcp)
{
	u64 tstamp;
	struct vmm_timer_event *e;

	/* If not started yet or still processing events then we give up */
	if ((tlcp->started == FALSE) || (tlcp->inprocess == TRUE)) {
		return;
	}

	/* If no events, we give up */
	if (list_empty(&tlcp->event_list)) {
		return;
	}

	/* Retrieve first event from list of active events */
	e = list_entry(list_first(&tlcp->event_list), 
		       struct vmm_timer_event,
		       active_head);

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

/* Note: This function must be called with ev->active_lock held. */
static void __timer_event_stop(struct vmm_timer_event *ev)
{
	irq_flags_t flags;
	struct vmm_timer_local_ctrl *tlcp;

	if (!ev->active_state) {
		return;
	}

	tlcp = &per_cpu(tlc, ev->active_hcpu);

	vmm_write_lock_irqsave_lite(&tlcp->event_list_lock, flags);

	ev->active_state = FALSE;
	list_del(&ev->active_head);
	ev->expiry_tstamp = 0;

	vmm_write_unlock_irqrestore_lite(&tlcp->event_list_lock, flags);
}

/* This is called from interrupt context. We need to protect the 
 * event list when manipulating it.
 */
static void timer_clockchip_event_handler(struct vmm_clockchip *cc)
{
	irq_flags_t flags, flags1;
	struct vmm_timer_event *e;
	struct vmm_timer_local_ctrl *tlcp = &this_cpu(tlc);

	vmm_read_lock_irqsave_lite(&tlcp->event_list_lock, flags);

	tlcp->inprocess = TRUE;

	/* Process expired active events */
	while (!list_empty(&tlcp->event_list)) {
		e = list_entry(list_first(&tlcp->event_list),
			       struct vmm_timer_event, active_head);
		/* Current timestamp */
		if (e->expiry_tstamp <= vmm_timer_timestamp()) {
			/* Unlock event list for processing expired event */
			vmm_read_unlock_irqrestore_lite(&tlcp->event_list_lock, flags);
			/* Set current CPU event to NULL */
			tlcp->curr = NULL;
			/* Stop expired active event */
			vmm_spin_lock_irqsave_lite(&e->active_lock, flags1);
			__timer_event_stop(e);
			vmm_spin_unlock_irqrestore_lite(&e->active_lock, flags1);
			/* Call event handler */
			e->handler(e);
			/* Lock back event list */
			vmm_read_lock_irqsave_lite(&tlcp->event_list_lock, flags);
		} else {
			/* No more expired events */
			break;
		}
	}

	tlcp->inprocess = FALSE;

	/* Schedule next timer event */
	__timer_schedule_next_event(tlcp);

	vmm_read_unlock_irqrestore_lite(&tlcp->event_list_lock, flags);
}

bool vmm_timer_event_pending(struct vmm_timer_event *ev)
{
	bool ret;
	irq_flags_t flags;

	if (!ev) {
		return FALSE;
	}

	vmm_spin_lock_irqsave_lite(&ev->active_lock, flags);
	ret = ev->active_state;
	vmm_spin_unlock_irqrestore_lite(&ev->active_lock, flags);

	return ret;
}

int vmm_timer_event_start(struct vmm_timer_event *ev, u64 duration_nsecs)
{
	u32 hcpu;
	u64 tstamp;
	bool found_pos = FALSE;
	irq_flags_t flags, flags1;
	struct vmm_timer_event *e = NULL;
	struct vmm_timer_local_ctrl *tlcp;

	if (!ev) {
		return VMM_EFAIL;
	}

	hcpu = vmm_smp_processor_id();
	tlcp = &per_cpu(tlc, hcpu);
	tstamp = vmm_timer_timestamp();

	vmm_spin_lock_irqsave_lite(&ev->active_lock, flags);

	__timer_event_stop(ev);

	ev->expiry_tstamp = tstamp + duration_nsecs;
	ev->duration_nsecs = duration_nsecs;
	ev->active_state = TRUE;
	ev->active_hcpu = hcpu;

	vmm_write_lock_irqsave_lite(&tlcp->event_list_lock, flags1);

	list_for_each_entry(e, &tlcp->event_list, active_head) {
		if (ev->expiry_tstamp < e->expiry_tstamp) {
			found_pos = TRUE;
			break;
		}
	}

	if (!found_pos) {
		list_add_tail(&ev->active_head, &tlcp->event_list);
	} else {
		list_add_tail(&ev->active_head, &e->active_head);
	}

	__timer_schedule_next_event(tlcp);

	vmm_write_unlock_irqrestore_lite(&tlcp->event_list_lock, flags1);

	vmm_spin_unlock_irqrestore_lite(&ev->active_lock, flags);

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

	if (!ev) {
		return VMM_EFAIL;
	}

	vmm_spin_lock_irqsave_lite(&ev->active_lock, flags);

	__timer_event_stop(ev);

	vmm_spin_unlock_irqrestore_lite(&ev->active_lock, flags);

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
	INIT_RW_LOCK(&tlcp->event_list_lock);
	INIT_LIST_HEAD(&tlcp->event_list);

	/* Bind suitable clockchip to current host CPU */
	tlcp->cc = vmm_clockchip_bind_best(cpu);
	if (!tlcp->cc) {
		vmm_printf("%s: No clockchip for CPU%d\n", __func__, cpu);
		return VMM_ENODEV;
	}

	/* Update event handler of clockchip */
	vmm_clockchip_set_event_handler(tlcp->cc, 
					&timer_clockchip_event_handler);

	if (vmm_smp_is_bootcpu()) {
		/* Find suitable clocksource */
		if (!(cs = vmm_clocksource_best())) {
			vmm_printf("%s: No clocksource found\n", __func__);
			return VMM_ENODEV;
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
