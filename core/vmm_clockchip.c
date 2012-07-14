/**
 * Copyright (c) 2012 Anup Patel.
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
 * @file vmm_clockchip.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation to clockchip managment
 */

#include <vmm_error.h>
#include <vmm_string.h>
#include <vmm_spinlocks.h>
#include <vmm_smp.h>
#include <vmm_stdio.h>
#include <vmm_clockchip.h>
#include <arch_timer.h>

/** Control structure for clockchip manager */
struct vmm_clockchip_ctrl {
	vmm_spinlock_t lock;
	struct dlist clkchip_list;
};

static struct vmm_clockchip_ctrl ccctrl;

void vmm_clockchip_set_event_handler(struct vmm_clockchip *cc, 
				  vmm_clockchip_event_handler_t event_handler)
{
	if (cc && event_handler) {
		cc->event_handler = event_handler;
	}
}

int vmm_clockchip_program_event(struct vmm_clockchip *cc, 
				u64 now_ns, u64 expires_ns)
{
	unsigned long long clc;
	u64 delta;

	if (expires_ns < now_ns) {
		return VMM_EFAIL;
	}

	if (cc->mode != VMM_CLOCKCHIP_MODE_ONESHOT)
		return 0;

	delta = expires_ns - now_ns;
	cc->next_event = expires_ns;

	if (delta > cc->max_delta_ns)
		delta = cc->max_delta_ns;
	if (delta < cc->min_delta_ns)
		delta = cc->min_delta_ns;

	clc = delta * cc->mult;
	clc >>= cc->shift;

	return cc->set_next_event((unsigned long) clc, cc);
}

int vmm_clockchip_force_expiry(struct vmm_clockchip *cc, u64 now_ns)
{
	if (cc->mode != VMM_CLOCKCHIP_MODE_ONESHOT)
		return 0;

	cc->next_event = now_ns;

	return cc->expire(cc);
}

void vmm_clockchip_set_mode(struct vmm_clockchip *cc, 
			    enum vmm_clockchip_mode mode)
{
	if (cc && cc->mode != mode) {
		cc->set_mode(mode, cc);
		cc->mode = mode;

		/* Multiplicator of 0 is invalid and we'd crash on it. */
		if (mode == VMM_CLOCKCHIP_MODE_ONESHOT) {
			if (!cc->mult) {
				vmm_panic("%s: clockchip mult=0 not allowed\n",
								__func__);
			}
		}
	}
}

int vmm_clockchip_register(struct vmm_clockchip *cc)
{
	bool found;
	irq_flags_t flags;
	struct dlist *l;
	struct vmm_clockchip *cct;

	if (!cc) {
		return VMM_EFAIL;
	}

	cct = NULL;
	found = FALSE;

	vmm_spin_lock_irqsave(&ccctrl.lock, flags);

	list_for_each(l, &ccctrl.clkchip_list) {
		cct = list_entry(l, struct vmm_clockchip, head);
		if (cct == cc) {
			found = TRUE;
			break;
		}
	}

	if (found) {
		vmm_spin_unlock_irqrestore(&ccctrl.lock, flags);
		return VMM_EFAIL;
	}

	INIT_LIST_HEAD(&cc->head);
	list_add_tail(&cc->head, &ccctrl.clkchip_list);

	vmm_spin_unlock_irqrestore(&ccctrl.lock, flags);

	return VMM_OK;
}

int vmm_clockchip_unregister(struct vmm_clockchip *cc)
{
	bool found;
	irq_flags_t flags;
	struct dlist *l;
	struct vmm_clockchip *cct;

	if (!cc) {
		return VMM_EFAIL;
	}

	vmm_spin_lock_irqsave(&ccctrl.lock, flags);

	if (list_empty(&ccctrl.clkchip_list)) {
		vmm_spin_unlock_irqrestore(&ccctrl.lock, flags);
		return VMM_EFAIL;
	}

	cct = NULL;
	found = FALSE;
	list_for_each(l, &ccctrl.clkchip_list) {
		cct = list_entry(l, struct vmm_clockchip, head);
		if (cct == cc) {
			found = TRUE;
			break;
		}
	}

	if (!found) {
		vmm_spin_unlock_irqrestore(&ccctrl.lock, flags);
		return VMM_ENOTAVAIL;
	}

	list_del(&cc->head);

	vmm_spin_unlock_irqrestore(&ccctrl.lock, flags);

	return VMM_OK;
}

struct vmm_clockchip *vmm_clockchip_find_best(const struct vmm_cpumask *mask)
{
	int rating = 0;
	irq_flags_t flags;
	struct dlist *l;
	struct vmm_clockchip *cc, *best_cc;

	cc = NULL;
	best_cc = NULL;

	vmm_spin_lock_irqsave(&ccctrl.lock, flags);

	list_for_each(l, &ccctrl.clkchip_list) {
		cc = list_entry(l, struct vmm_clockchip, head);
		if ((cc->rating > rating) &&
		    vmm_cpumask_intersects(cc->cpumask, mask)) {
			best_cc = cc;
			rating = cc->rating;
		}
	}

	vmm_spin_unlock_irqrestore(&ccctrl.lock, flags);

	return best_cc;
}

struct vmm_clockchip *vmm_clockchip_get(int index)
{
	bool found;
	irq_flags_t flags;
	struct dlist *l;
	struct vmm_clockchip *ret;

	if (index < 0) {
		return NULL;
	}

	vmm_spin_lock_irqsave(&ccctrl.lock, flags);

	ret = NULL;
	found = FALSE;

	list_for_each(l, &ccctrl.clkchip_list) {
		ret = list_entry(l, struct vmm_clockchip, head);
		if (!index) {
			found = TRUE;
			break;
		}
		index--;
	}

	vmm_spin_unlock_irqrestore(&ccctrl.lock, flags);

	if (!found) {
		return NULL;
	}

	return ret;
}

u32 vmm_clockchip_count(void)
{
	u32 retval = 0;
	irq_flags_t flags;
	struct dlist *l;

	vmm_spin_lock_irqsave(&ccctrl.lock, flags);

	list_for_each(l, &ccctrl.clkchip_list) {
		retval++;
	}

	vmm_spin_unlock_irqrestore(&ccctrl.lock, flags);

	return retval;
}

int __init vmm_clockchip_init(void)
{
	int rc;
	u32 cpu = vmm_smp_processor_id();

	if (!cpu) {
		/* Initialize clock chip list lock */
		INIT_SPIN_LOCK(&ccctrl.lock);

		/* Initialize clock chip list */
		INIT_LIST_HEAD(&ccctrl.clkchip_list);
	}

	/* Initialize arch specific clock chips */
	if ((rc = arch_clockchip_init())) {
		return rc;
	}

	return VMM_OK;
}
