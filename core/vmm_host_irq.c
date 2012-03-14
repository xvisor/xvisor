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
 * @author Anup Patel (anup@brainfault.org)
 * @brief source code for host interrupts
 */

#include <arch_cpu.h>
#include <arch_board.h>
#include <vmm_error.h>
#include <vmm_string.h>
#include <vmm_spinlocks.h>
#include <vmm_heap.h>
#include <vmm_host_irq.h>

struct vmm_host_irqs_ctrl {
	vmm_spinlock_t lock;
	u32 irq_count;
	struct vmm_host_irq * irq;
};

static struct vmm_host_irqs_ctrl hirqctrl;

int vmm_host_irq_exec(u32 cpu_irq_no, arch_regs_t * regs)
{
	struct dlist * l;
	struct vmm_host_irq * irq;
	struct vmm_host_irq_hndl * hirq;
	u32 hirq_no = arch_pic_irq_active(cpu_irq_no);
	if (hirq_no < hirqctrl.irq_count) {
		irq = &hirqctrl.irq[hirq_no];
		if (irq->enabled) {
			arch_pic_irq_ack(hirq_no);
			irq = &hirqctrl.irq[hirq_no];
			list_for_each(l, &irq->hndl_list) {
				hirq = list_entry(l, struct vmm_host_irq_hndl, head);
				hirq->hndl(hirq_no, regs, hirq->dev);
			}
			arch_pic_irq_eoi(hirq_no);
		}
	}
	return VMM_ENOTAVAIL;
}

bool vmm_host_irq_isenabled(u32 hirq_no)
{
	if (hirq_no < hirqctrl.irq_count) {
		return hirqctrl.irq[hirq_no].enabled;
	}
	return FALSE;
}

int vmm_host_irq_enable(u32 hirq_no)
{
	if (hirq_no < hirqctrl.irq_count) {
		if (!hirqctrl.irq[hirq_no].enabled) {
			hirqctrl.irq[hirq_no].enabled = TRUE;
			arch_pic_irq_unmask(hirq_no);
			return VMM_OK;
		}
	}
	return VMM_EFAIL;
}

int vmm_host_irq_disable(u32 hirq_no)
{
	if (hirq_no < hirqctrl.irq_count) {
		if (hirqctrl.irq[hirq_no].enabled) {
			hirqctrl.irq[hirq_no].enabled = FALSE;
			arch_pic_irq_mask(hirq_no);
			return VMM_OK;
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
	struct dlist *l;
	struct vmm_host_irq *irq;
	struct vmm_host_irq_hndl *hirq;
	if (hirq_no < hirqctrl.irq_count) {
		flags = vmm_spin_lock_irqsave(&hirqctrl.lock);
		irq = &hirqctrl.irq[hirq_no];
		found = FALSE;
		list_for_each(l, &irq->hndl_list) {
			hirq = list_entry(l, struct vmm_host_irq_hndl, head);
			if (hirq->hndl == handler) {
				found = TRUE;
				break;
			}
		}
		if (found) {
			vmm_spin_unlock_irqrestore(&hirqctrl.lock, flags);
			return VMM_EFAIL;
		}
		hirq = vmm_malloc(sizeof(struct vmm_host_irq_hndl));
		if (!hirq) {
			vmm_spin_unlock_irqrestore(&hirqctrl.lock, flags);
			return VMM_EFAIL;
		}
		INIT_LIST_HEAD(&hirq->head);
		hirq->hndl = handler;
		hirq->dev = dev;
		list_add_tail(&irq->hndl_list, &hirq->head);
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
	struct dlist *l;
	struct vmm_host_irq *irq;
	struct vmm_host_irq_hndl * hirq;
	if (hirq_no < hirqctrl.irq_count) {
		flags = vmm_spin_lock_irqsave(&hirqctrl.lock);
		irq = &hirqctrl.irq[hirq_no];
		found = FALSE;
		list_for_each(l, &irq->hndl_list) {
			hirq = list_entry(l, struct vmm_host_irq_hndl, head);
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
		if (list_empty(&irq->hndl_list)) {
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
	hirqctrl.irq_count = arch_pic_irq_count();

	/* Allocate memory for irq array */
	hirqctrl.irq = vmm_malloc(sizeof(struct vmm_host_irq) * 
				  hirqctrl.irq_count);

	/* Reset the handler array */
	for (ite = 0; ite < hirqctrl.irq_count; ite++) {
		hirqctrl.irq[ite].enabled = FALSE;
		INIT_LIST_HEAD(&hirqctrl.irq[ite].hndl_list);
	}

	/* Initialize board specific PIC */
	if ((ret = arch_pic_init())) {
		return ret;
	}

	/** Setup interrupts in CPU */
	if ((ret = arch_cpu_irq_setup())) {
		return ret;
	}

	/** Enable interrupts in CPU */
	arch_cpu_irq_enable();

	return VMM_OK;
}
