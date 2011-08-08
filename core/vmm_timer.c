/**
 * Copyright (c) 2010 Himanshu Chauhan.
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
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief Implementation of timer subsystem
 */

#include <vmm_cpu.h>
#include <vmm_error.h>
#include <vmm_string.h>
#include <vmm_devtree.h>
#include <vmm_timer.h>

vmm_timer_ctrl_t tctrl;

void vmm_timer_tick_process(vmm_user_regs_t * regs, u32 ticks)
{
	struct dlist *l;
	vmm_ticker_t *t;

	/* Sanity check */
	if (!ticks) {
		return;
	}

	/* Increment jiffies */
	tctrl.tickstamp += ticks;

	/* Call enabled tickers */
	t = NULL;
	list_for_each(l, &tctrl.ticker_list) {
		t = list_entry(l, vmm_ticker_t, head);
		if (t->enabled) {
			t->hndl(regs, ticks);
		}
	}
}

u64 vmm_timer_get_tickstamp(void)
{
	return tctrl.tickstamp;
}

void vmm_timer_set_tickstamp(u64 tickstamp)
{
	tctrl.tickstamp = tickstamp;
}

u32 vmm_timer_tick_usecs(void)
{
	return tctrl.tick_usecs;
}

int vmm_timer_enable_ticker(vmm_ticker_t * tk)
{
	tk->enabled = TRUE;

	return VMM_OK;
}

int vmm_timer_disable_ticker(vmm_ticker_t * tk)
{
	tk->enabled = FALSE;

	return VMM_OK;
}

int vmm_timer_register_ticker(vmm_ticker_t * tk)
{
	bool found;
	struct dlist *l;
	vmm_ticker_t *t;

	if (!tk) {
		return VMM_EFAIL;
	}

	t = NULL;
	found = FALSE;
	list_for_each(l, &tctrl.ticker_list) {
		t = list_entry(l, vmm_ticker_t, head);
		if (vmm_strcmp(t->name, tk->name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (found) {
		return VMM_EINVALID;
	}

	INIT_LIST_HEAD(&tk->head);

	list_add_tail(&tctrl.ticker_list, &tk->head);

	return VMM_OK;
}

int vmm_timer_unregister_ticker(vmm_ticker_t * tk)
{
	bool found;
	struct dlist *l;
	vmm_ticker_t *t;

	if (!tk) {
		return VMM_EFAIL;
	}

	if (list_empty(&tctrl.ticker_list)) {
		return VMM_EFAIL;
	}

	t = NULL;
	found = FALSE;
	list_for_each(l, &tctrl.ticker_list) {
		t = list_entry(l, vmm_ticker_t, head);
		if (vmm_strcmp(t->name, tk->name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (!found) {
		return VMM_ENOTAVAIL;
	}

	list_del(&t->head);

	return VMM_OK;
}

vmm_ticker_t *vmm_timer_find_ticker(const char *name)
{
	bool found;
	struct dlist *l;
	vmm_ticker_t *t;

	if (!name) {
		return NULL;
	}

	found = FALSE;
	t = NULL;

	list_for_each(l, &tctrl.ticker_list) {
		t = list_entry(l, vmm_ticker_t, head);
		if (vmm_strcmp(t->name, name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (!found) {
		return NULL;
	}

	return t;
}

vmm_ticker_t *vmm_timer_ticker(int index)
{
	bool found;
	struct dlist *l;
	vmm_ticker_t *ret;

	if (index < 0) {
		return NULL;
	}

	ret = NULL;
	found = FALSE;

	list_for_each(l, &tctrl.ticker_list) {
		ret = list_entry(l, vmm_ticker_t, head);
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

u32 vmm_timer_ticker_count(void)
{
	u32 retval = 0;
	struct dlist *l;

	list_for_each(l, &tctrl.ticker_list) {
		retval++;
	}

	return retval;
}

void vmm_timer_start(void)
{
	/** Setup timer */
	vmm_cpu_timer_setup(tctrl.tick_usecs);

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

	/* Set tickstamp to zero */
	tctrl.tickstamp = 0;

	/* Find out tick delay in microseconds */
	vnode = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPRATOR_STRING
				   VMM_DEVTREE_VMMINFO_NODE_NAME);
	if (!vnode) {
		return VMM_EFAIL;
	}
	attrval = vmm_devtree_attrval(vnode,
				      VMM_DEVTREE_TICK_DELAY_USECS_ATTR_NAME);
	if (!attrval) {
		return VMM_EFAIL;
	}
	tctrl.tick_usecs = *((u32 *) attrval);

	/* Initialize ticker list */
	INIT_LIST_HEAD(&tctrl.ticker_list);

	return VMM_OK;
}



