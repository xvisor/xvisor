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
 * @file vmm_host_irq.c
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief source code for host interrupts
 */

#include <vmm_cpu.h>
#include <vmm_board.h>
#include <vmm_error.h>
#include <vmm_string.h>
#include <vmm_spinlocks.h>
#include <vmm_heap.h>
#include <vmm_host_irq.h>

struct vmm_host_irq {
	struct dlist head;
	vmm_host_irq_handler_t hndl;
	void *dev;
};

struct vmm_host_irqs_ctrl {
	vmm_spinlock_t lock;
	u32 irq_count;
	bool *enabled;
	struct dlist  * irq;
};

static struct vmm_host_irqs_ctrl hirqctrl;

int vmm_host_irq_exec(u32 cpu_irq_no, vmm_user_regs_t * regs)
{
	struct dlist * l;
	struct dlist * irq;
	struct vmm_host_irq * hirq;
	int cond, hirq_no = vmm_pic_cpu_to_host_map(cpu_irq_no);
	if (-1 < hirq_no && hirq_no < hirqctrl.irq_count) {
		if (hirqctrl.enabled[hirq_no]) {
			cond = vmm_pic_pre_condition(hirq_no);
			if (!cond) {
				irq = &hirqctrl.irq[hirq_no];
				list_for_each(l, irq) {
					hirq = list_entry(l, struct vmm_host_irq, head);
					hirq->hndl(hirq_no, regs, hirq->dev);
				}
				cond = vmm_pic_post_condition(hirq_no);
			}
			return cond;
		}
	}
	return VMM_ENOTAVAIL;
}

bool vmm_host_irq_isenabled(u32 hirq_no)
{
	if (hirq_no < hirqctrl.irq_count) {
		return hirqctrl.enabled[hirq_no];
	}
	return FALSE;
}

int vmm_host_irq_enable(u32 hirq_no)
{
	if (hirq_no < hirqctrl.irq_count) {
		if (!hirqctrl.enabled[hirq_no]) {
			hirqctrl.enabled[hirq_no] = TRUE;
			return vmm_pic_irq_enable(hirq_no);
		}
	}
	return VMM_EFAIL;
}

int vmm_host_irq_disable(u32 hirq_no)
{
	if (hirq_no < hirqctrl.irq_count) {
		if (hirqctrl.enabled[hirq_no]) {
			hirqctrl.enabled[hirq_no] = FALSE;
			return vmm_pic_irq_disable(hirq_no);
		}
	}
	return VMM_EFAIL;
}

int vmm_host_irq_register(u32 hirq_no, 
			  vmm_host_irq_handler_t handler,
			  void *dev)
{
	bool found;
	irq_flags_t flags;
	struct dlist *irq;
	struct dlist *l;
	struct vmm_host_irq * hirq;
	if (hirq_no < hirqctrl.irq_count) {
		flags = vmm_spin_lock_irqsave(&hirqctrl.lock);
		irq = &hirqctrl.irq[hirq_no];
		found = FALSE;
		list_for_each(l, irq) {
			hirq = list_entry(l, struct vmm_host_irq, head);
			if (hirq->hndl == handler) {
				found = TRUE;
				break;
			}
		}
		if (found) {
			vmm_spin_unlock_irqrestore(&hirqctrl.lock, flags);
			return VMM_EFAIL;
		}
		hirq = vmm_malloc(sizeof(struct vmm_host_irq));
		if (!hirq) {
			vmm_spin_unlock_irqrestore(&hirqctrl.lock, flags);
			return VMM_EFAIL;
		}
		INIT_LIST_HEAD(&hirq->head);
		hirq->hndl = handler;
		hirq->dev = dev;
		list_add_tail(irq, &hirq->head);
		vmm_spin_unlock_irqrestore(&hirqctrl.lock, flags);
		return VMM_OK;
	}
	return VMM_EFAIL;
}

int vmm_host_irq_unregister(u32 hirq_no, 
			    vmm_host_irq_handler_t handler)
{
	bool found;
	irq_flags_t flags;
	struct dlist *irq;
	struct dlist *l;
	struct vmm_host_irq * hirq;
	if (hirq_no < hirqctrl.irq_count) {
		flags = vmm_spin_lock_irqsave(&hirqctrl.lock);
		irq = &hirqctrl.irq[hirq_no];
		found = FALSE;
		list_for_each(l, irq) {
			hirq = list_entry(l, struct vmm_host_irq, head);
			if (hirq->hndl == handler) {
				found = TRUE;
				break;
			}
		}
		if (!found) {
			vmm_spin_unlock_irqrestore(&hirqctrl.lock, flags);
			return VMM_EFAIL;
		}
		list_del(&hirq->head);
		vmm_free(hirq);
		vmm_spin_unlock_irqrestore(&hirqctrl.lock, flags);
		if (list_empty(irq)) {
			return vmm_host_irq_disable(hirq_no);
		}
		return VMM_OK;
	}
	return VMM_EFAIL;
}

int __init vmm_host_irq_init(void)
{
	int ret;
	u32 ite;

	/* Clear the memory of control structure */
	vmm_memset(&hirqctrl, 0, sizeof(hirqctrl));

	/* Initialize spin lock */
	INIT_SPIN_LOCK(&hirqctrl.lock);

	/* Get host irq count */
	hirqctrl.irq_count = vmm_pic_irq_count();

	/* Allocate memory for enabled array */
	hirqctrl.enabled = (bool *)vmm_malloc(sizeof(bool) * hirqctrl.irq_count);

	/* Set default values to enabled array */
	for (ite = 0; ite < hirqctrl.irq_count; ite++) {
		hirqctrl.enabled[ite] = FALSE;
	}

	/* Allocate memory for irq array */
	hirqctrl.irq = vmm_malloc(sizeof(struct dlist) * hirqctrl.irq_count);

	/* Reset the handler array */
	for (ite = 0; ite < hirqctrl.irq_count; ite++) {
		INIT_LIST_HEAD(&hirqctrl.irq[ite]);
	}

	/* Initialize board specific PIC */
	ret = vmm_pic_init();
	if (ret) {
		return ret;
	}

	/** Setup interrupts in CPU */
	ret = vmm_cpu_irq_setup();
	if (ret) {
		return ret;
	}

	/** Enable interrupts in CPU */
	vmm_cpu_irq_enable();

	return VMM_OK;
}
