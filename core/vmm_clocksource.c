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
 * @file vmm_clocksource.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation to manage timer subsystem clock sources
 */

#include <arch_timer.h>
#include <vmm_error.h>
#include <vmm_string.h>
#include <vmm_clocksource.h>

/** Control structure for Timer Subsystem */
struct vmm_clocksource_ctrl {
	struct dlist clksrc_list;
};

static struct vmm_clocksource_ctrl tcsctrl;

#ifdef CONFIG_PROFILE
u64 __notrace vmm_timecounter_read(struct vmm_timecounter *tc)
#else
u64 vmm_timecounter_read(struct vmm_timecounter *tc)
#endif
{
	u64 cycles_now, cycles_delta;
	u64 ns_offset;

	cycles_now = tc->cs->read(tc->cs);
	cycles_delta = (cycles_now - tc->cycles_last) & tc->cs->mask;
	tc->cycles_last = cycles_now;

	ns_offset = (cycles_delta * tc->cs->mult) >> tc->cs->shift;
	tc->nsec += ns_offset;

	return tc->nsec;
}

int vmm_timecounter_start(struct vmm_timecounter *tc)
{
	if (!tc) {
		return VMM_EFAIL;
	}

	if (tc->cs->enable) {
		tc->cs->enable(tc->cs);
	}

	return VMM_OK;
}

int vmm_timecounter_stop(struct vmm_timecounter *tc)
{
	if (!tc) {
		return VMM_EFAIL;
	}

	if (tc->cs->disable) {
		tc->cs->disable(tc->cs);
	}

	return VMM_OK;
}

int vmm_timecounter_init(struct vmm_timecounter *tc,
			 struct vmm_clocksource *cs,
			 u64 start_nsec)
{
	if (!tc || !cs) {
		return VMM_EFAIL;
	}

	tc->cs = cs;
	tc->cycles_last = cs->read(cs);
	tc->nsec = start_nsec;

	return VMM_OK;
}

int vmm_clocksource_register(struct vmm_clocksource *cs)
{
	bool found;
	struct dlist *l;
	struct vmm_clocksource *cst;

	if (!cs) {
		return VMM_EFAIL;
	}

	cst = NULL;
	found = FALSE;
	list_for_each(l, &tcsctrl.clksrc_list) {
		cst = list_entry(l, struct vmm_clocksource, head);
		if (vmm_strcmp(cst->name, cs->name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (found) {
		return VMM_EFAIL;
	}

	INIT_LIST_HEAD(&cs->head);
	list_add_tail(&tcsctrl.clksrc_list, &cs->head);

	return VMM_OK;
}

int vmm_clocksource_unregister(struct vmm_clocksource *cs)
{
	bool found;
	struct dlist *l;
	struct vmm_clocksource *cst;

	if (!cs) {
		return VMM_EFAIL;
	}

	if (list_empty(&tcsctrl.clksrc_list)) {
		return VMM_EFAIL;
	}

	cst = NULL;
	found = FALSE;
	list_for_each(l, &tcsctrl.clksrc_list) {
		cst = list_entry(l, struct vmm_clocksource, head);
		if (vmm_strcmp(cst->name, cs->name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (!found) {
		return VMM_ENOTAVAIL;
	}

	list_del(&cs->head);

	return VMM_OK;
}

struct vmm_clocksource *vmm_clocksource_best(void)
{
	int rating = 0;
	struct dlist *l;
	struct vmm_clocksource *cs, *best_cs;

	cs = NULL;
	best_cs = NULL;

	list_for_each(l, &tcsctrl.clksrc_list) {
		cs = list_entry(l, struct vmm_clocksource, head);
		if (cs->rating > rating) {
			best_cs = cs;
			rating = cs->rating;
		}
	}

	return best_cs;
}

struct vmm_clocksource *vmm_clocksource_find(const char *name)
{
	bool found;
	struct dlist *l;
	struct vmm_clocksource *cs;

	if (!name) {
		return NULL;
	}

	found = FALSE;
	cs = NULL;

	list_for_each(l, &tcsctrl.clksrc_list) {
		cs = list_entry(l, struct vmm_clocksource, head);
		if (vmm_strcmp(cs->name, name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (!found) {
		return NULL;
	}

	return cs;
}

struct vmm_clocksource *vmm_clocksource_get(int index)
{
	bool found;
	struct dlist *l;
	struct vmm_clocksource *ret;

	if (index < 0) {
		return NULL;
	}

	ret = NULL;
	found = FALSE;

	list_for_each(l, &tcsctrl.clksrc_list) {
		ret = list_entry(l, struct vmm_clocksource, head);
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

u32 vmm_clocksource_count(void)
{
	u32 retval = 0;
	struct dlist *l;

	list_for_each(l, &tcsctrl.clksrc_list) {
		retval++;
	}

	return retval;
}

int __init vmm_clocksource_init(void)
{
	int rc;

	/* Initialize clock source list */
	INIT_LIST_HEAD(&tcsctrl.clksrc_list);

	/* Initialize arch specific timer clock sources */
	if ((rc = arch_clocksource_init())) {
		return rc;
	}

	return VMM_OK;
}
