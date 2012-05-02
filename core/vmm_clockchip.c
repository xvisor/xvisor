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
 * @brief Implementation to manage timer subsystem clock sources
 */

#include <arch_timer.h>
#include <vmm_error.h>
#include <vmm_string.h>
#include <vmm_stdio.h>
#include <vmm_clockchip.h>

/** Control structure for clockchip manager */
struct vmm_clockchip_ctrl {
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
	struct dlist *l;
	struct vmm_clockchip *cct;

	if (!cc) {
		return VMM_EFAIL;
	}

	cct = NULL;
	found = FALSE;
	list_for_each(l, &ccctrl.clkchip_list) {
		cct = list_entry(l, struct vmm_clockchip, head);
		if (vmm_strcmp(cct->name, cc->name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (found) {
		return VMM_EFAIL;
	}

	INIT_LIST_HEAD(&cc->head);
	list_add_tail(&ccctrl.clkchip_list, &cc->head);

	return VMM_OK;
}

int vmm_clockchip_unregister(struct vmm_clockchip *cc)
{
	bool found;
	struct dlist *l;
	struct vmm_clockchip *cct;

	if (!cc) {
		return VMM_EFAIL;
	}

	if (list_empty(&ccctrl.clkchip_list)) {
		return VMM_EFAIL;
	}

	cct = NULL;
	found = FALSE;
	list_for_each(l, &ccctrl.clkchip_list) {
		cct = list_entry(l, struct vmm_clockchip, head);
		if (vmm_strcmp(cct->name, cc->name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (!found) {
		return VMM_ENOTAVAIL;
	}

	list_del(&cc->head);

	return VMM_OK;
}

struct vmm_clockchip *vmm_clockchip_best(void)
{
	int rating = 0;
	struct dlist *l;
	struct vmm_clockchip *cc, *best_cc;

	cc = NULL;
	best_cc = NULL;

	list_for_each(l, &ccctrl.clkchip_list) {
		cc = list_entry(l, struct vmm_clockchip, head);
		if (cc->rating > rating) {
			best_cc = cc;
			rating = cc->rating;
		}
	}

	return best_cc;
}

struct vmm_clockchip *vmm_clockchip_find(const char *name)
{
	bool found;
	struct dlist *l;
	struct vmm_clockchip *cc;

	if (!name) {
		return NULL;
	}

	found = FALSE;
	cc = NULL;

	list_for_each(l, &ccctrl.clkchip_list) {
		cc = list_entry(l, struct vmm_clockchip, head);
		if (vmm_strcmp(cc->name, name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (!found) {
		return NULL;
	}

	return cc;
}

struct vmm_clockchip *vmm_clockchip_get(int index)
{
	bool found;
	struct dlist *l;
	struct vmm_clockchip *ret;

	if (index < 0) {
		return NULL;
	}

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

	if (!found) {
		return NULL;
	}

	return ret;
}

u32 vmm_clockchip_count(void)
{
	u32 retval = 0;
	struct dlist *l;

	list_for_each(l, &ccctrl.clkchip_list) {
		retval++;
	}

	return retval;
}

int __init vmm_clockchip_init(void)
{
	int rc;

	/* Initialize clock source list */
	INIT_LIST_HEAD(&ccctrl.clkchip_list);

	/* Initialize arch specific timer clock sources */
	if ((rc = arch_clockchip_init())) {
		return rc;
	}

	return VMM_OK;
}
