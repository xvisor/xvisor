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
#include <vmm_limits.h>
#include <vmm_spinlocks.h>
#include <vmm_smp.h>
#include <vmm_stdio.h>
#include <vmm_host_irq.h>
#include <vmm_clockchip.h>
#include <arch_timer.h>

/** Control structure for clockchip manager */
struct vmm_clockchip_ctrl {
	vmm_spinlock_t lock;
	struct dlist clkchip_list;
	const struct vmm_devtree_nodeid *clkchip_matches;
};

static struct vmm_clockchip_ctrl ccctrl;

static void default_event_handler(struct vmm_clockchip *cc)
{
	/* Just ignore. Do nothing. */
}

void vmm_clockchip_set_event_handler(struct vmm_clockchip *cc, 
		void (*event_handler) (struct vmm_clockchip *))
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
	struct vmm_clockchip *cct;

	if (!cc) {
		return VMM_EFAIL;
	}

	cct = NULL;
	found = FALSE;

	vmm_spin_lock_irqsave(&ccctrl.lock, flags);

	list_for_each_entry(cct, &ccctrl.clkchip_list, head) {
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
	cc->event_handler = default_event_handler;
	cc->bound_on = UINT_MAX;
	list_add_tail(&cc->head, &ccctrl.clkchip_list);

	vmm_spin_unlock_irqrestore(&ccctrl.lock, flags);

	return VMM_OK;
}

int vmm_clockchip_unregister(struct vmm_clockchip *cc)
{
	bool found;
	irq_flags_t flags;
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
	list_for_each_entry(cct, &ccctrl.clkchip_list, head) {
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

struct vmm_clockchip *vmm_clockchip_bind_best(u32 hcpu)
{
	int best_rating;
	irq_flags_t flags;
	const struct vmm_cpumask *mask;
	struct vmm_clockchip *cc, *best_cc;

	if (CONFIG_CPU_COUNT <= hcpu) {
		return NULL;
	}

	mask = vmm_cpumask_of(hcpu);
	cc = NULL;
	best_cc = NULL;
	best_rating = 0;

	vmm_spin_lock_irqsave(&ccctrl.lock, flags);

	list_for_each_entry(cc, &ccctrl.clkchip_list, head) {
		if ((cc->rating > best_rating) &&
		    (cc->bound_on == UINT_MAX) &&
		    vmm_cpumask_intersects(cc->cpumask, mask)) {
			best_cc = cc;
			best_rating = cc->rating;
		}
	}

	if (best_cc) {
		vmm_host_irq_set_affinity(best_cc->hirq, mask, TRUE);
		best_cc->bound_on = hcpu;
	}

	vmm_spin_unlock_irqrestore(&ccctrl.lock, flags);

	return best_cc;
}

int vmm_clockchip_unbind(struct vmm_clockchip *cc)
{
	irq_flags_t flags;

	if (!cc) {
		return VMM_EINVALID;
	}

	vmm_spin_lock_irqsave(&ccctrl.lock, flags);
	cc->bound_on = UINT_MAX;
	vmm_spin_unlock_irqrestore(&ccctrl.lock, flags);

	return VMM_OK;
}

struct vmm_clockchip *vmm_clockchip_get(int index)
{
	bool found;
	irq_flags_t flags;
	struct vmm_clockchip *cc;

	if (index < 0) {
		return NULL;
	}

	vmm_spin_lock_irqsave(&ccctrl.lock, flags);

	cc = NULL;
	found = FALSE;

	list_for_each_entry(cc, &ccctrl.clkchip_list, head) {
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

	return cc;
}

u32 vmm_clockchip_count(void)
{
	u32 retval = 0;
	irq_flags_t flags;
	struct vmm_clockchip *cc;

	vmm_spin_lock_irqsave(&ccctrl.lock, flags);

	list_for_each_entry(cc, &ccctrl.clkchip_list, head) {
		retval++;
	}

	vmm_spin_unlock_irqrestore(&ccctrl.lock, flags);

	return retval;
}

int __cpuinit __weak arch_clockchip_init(void)
{
	/* Default weak implementation in-case
	 * architecture does not provide one.
	 */
	return VMM_OK;
}

static void __cpuinit clockchip_nidtbl_found(struct vmm_devtree_node *node,
					const struct vmm_devtree_nodeid *match,
					void *data)
{
	int err;
	vmm_clockchip_init_t init_fn = match->data;

	if (!init_fn) {
		return;
	}

	err = init_fn(node);
#ifdef CONFIG_VERBOSE_MODE
	if (err) {
		vmm_printf("%s: CPU%d Init %s node failed (error %d)\n", 
			   __func__, vmm_smp_processor_id(), node->name, err);
	}
#else
	(void)err;
#endif
}

int __cpuinit vmm_clockchip_init(void)
{
	int rc;

	if (vmm_smp_is_bootcpu()) {
		/* Initialize clockchip list lock */
		INIT_SPIN_LOCK(&ccctrl.lock);

		/* Initialize clockchip list */
		INIT_LIST_HEAD(&ccctrl.clkchip_list);

		/* Determine clockchip matches from nodeid table */
		ccctrl.clkchip_matches = 
			vmm_devtree_nidtbl_create_matches("clockchip");
	}

	/* Initialize arch specific clockchips */
	if ((rc = arch_clockchip_init())) {
		return rc;
	}

	/* Probe all device tree nodes matching 
	 * clockchip nodeid table enteries.
	 */
	if (ccctrl.clkchip_matches) {
		vmm_devtree_iterate_matching(NULL,
					     ccctrl.clkchip_matches,
					     clockchip_nidtbl_found,
					     NULL);
	}

	return VMM_OK;
}
