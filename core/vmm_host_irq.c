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

#include <arch_cpu_irq.h>
#include <arch_host_irq.h>
#include <vmm_error.h>
#include <vmm_string.h>
#include <vmm_spinlocks.h>
#include <vmm_heap.h>
#include <vmm_host_irq.h>

struct vmm_host_irqs_ctrl {
	vmm_spinlock_t lock;
	struct vmm_host_irq * irq;
};

static struct vmm_host_irqs_ctrl hirqctrl;

int vmm_host_irq_exec(u32 cpu_irq_no, arch_regs_t * regs)
{
	struct dlist * l;
	struct vmm_host_irq * irq;
	struct vmm_host_irq_hndl * hirq;
	u32 hirq_num = arch_host_irq_active(cpu_irq_no);
	if (hirq_num < ARCH_HOST_IRQ_COUNT) {
		irq = &hirqctrl.irq[hirq_num];
		irq->count++;
		if (!(irq->state & VMM_IRQ_STATE_PER_CPU)) {
			irq->state |= VMM_IRQ_STATE_INPROGRESS;
		}
		if (irq->chip && irq->chip->irq_ack) {
			irq->chip->irq_ack(irq);
		}
		list_for_each(l, &irq->hndl_list) {
			hirq = list_entry(l, struct vmm_host_irq_hndl, head);
			if (hirq->hndl(hirq_num, regs, hirq->dev) ==
						VMM_IRQ_HANDLED) {
				break;
			}
		}
		if (irq->chip && irq->chip->irq_eoi) {
			irq->chip->irq_eoi(irq);
		}
		if (!(irq->state & VMM_IRQ_STATE_PER_CPU)) {
			irq->state &= ~VMM_IRQ_STATE_INPROGRESS;
		}
		return VMM_OK;
	}
	return VMM_ENOTAVAIL;
}

u32 vmm_host_irq_count(void)
{
	return ARCH_HOST_IRQ_COUNT;
}

struct vmm_host_irq * vmm_host_irq_get(u32 hirq_num)
{
	if (hirq_num < ARCH_HOST_IRQ_COUNT) {
		return &hirqctrl.irq[hirq_num];
	}
	return NULL;
}

int vmm_host_irq_set_chip(u32 hirq_num, struct vmm_host_irq_chip *chip)
{
	if (hirq_num < ARCH_HOST_IRQ_COUNT) {
		hirqctrl.irq[hirq_num].chip = chip;
		return VMM_OK;
	}
	return VMM_EFAIL;
}

int vmm_host_irq_set_chip_data(u32 hirq_num, void * chip_data)
{
	if (hirq_num < ARCH_HOST_IRQ_COUNT) {
		hirqctrl.irq[hirq_num].chip_data = chip_data;
		return VMM_OK;
	}
	return VMM_EFAIL;
}

int vmm_host_irq_set_affinity(u32 hirq_num, 
			      const struct vmm_cpumask *dest, 
			      bool force)
{
	struct vmm_host_irq * irq;
	if (hirq_num < ARCH_HOST_IRQ_COUNT) {
		irq = &hirqctrl.irq[hirq_num];
		if (irq->chip && irq->chip->irq_set_affinity) {
			irq->state |= VMM_IRQ_STATE_AFFINITY_SET;
			return irq->chip->irq_set_affinity(irq, dest, force);
		}
	}
	return VMM_EFAIL;
}

int vmm_host_irq_set_type(u32 hirq_num, u32 type)
{
	int rc = VMM_EFAIL;
	struct vmm_host_irq * irq;
	if (hirq_num < ARCH_HOST_IRQ_COUNT) {
		irq = &hirqctrl.irq[hirq_num];
		type &= VMM_IRQ_TYPE_SENSE_MASK;
		if (type == VMM_IRQ_TYPE_NONE) {
			return VMM_OK;
		}
		if (irq->chip && irq->chip->irq_set_type) {
			rc = irq->chip->irq_set_type(irq, type);
		} else {
			return VMM_OK;
		}
		if (rc == VMM_OK) {
			irq->state &= ~VMM_IRQ_STATE_TRIGGER_MASK;
			irq->state |= type;
			if (type & VMM_IRQ_TYPE_LEVEL_MASK) {
				irq->state |= VMM_IRQ_STATE_LEVEL;
			} else {
				irq->state &= ~VMM_IRQ_STATE_LEVEL;
			}
		}
	}
	return rc;
}

int vmm_host_irq_mark_per_cpu(u32 hirq_num)
{
	struct vmm_host_irq * irq;
	if (hirq_num < ARCH_HOST_IRQ_COUNT) {
		irq = &hirqctrl.irq[hirq_num];
		irq->state |= VMM_IRQ_STATE_PER_CPU;
		return VMM_OK;
	}
	return VMM_EFAIL;
}

int vmm_host_irq_unmark_per_cpu(u32 hirq_num)
{
	struct vmm_host_irq * irq;
	if (hirq_num < ARCH_HOST_IRQ_COUNT) {
		irq = &hirqctrl.irq[hirq_num];
		irq->state &= ~VMM_IRQ_STATE_PER_CPU;
		return VMM_OK;
	}
	return VMM_EFAIL;
}

int vmm_host_irq_enable(u32 hirq_num)
{
	struct vmm_host_irq * irq;
	if (hirq_num < ARCH_HOST_IRQ_COUNT) {
		irq = &hirqctrl.irq[hirq_num];
		irq->state &= ~VMM_IRQ_STATE_DISABLED;
		if (irq->chip) {
			if (irq->chip->irq_enable) {
				irq->chip->irq_enable(irq);
			} else {
				irq->chip->irq_unmask(irq);
			}
		}
		irq->state &= ~VMM_IRQ_STATE_MASKED;
		return VMM_OK;
	}
	return VMM_EFAIL;
}

int vmm_host_irq_disable(u32 hirq_num)
{
	struct vmm_host_irq * irq;
	if (hirq_num < ARCH_HOST_IRQ_COUNT) {
		irq = &hirqctrl.irq[hirq_num];
		irq->state |= VMM_IRQ_STATE_DISABLED;
		if (irq->chip) {
			if (irq->chip->irq_disable) {
				irq->chip->irq_disable(irq);
			} else {
				irq->chip->irq_mask(irq);
			}
		}
		irq->state |= VMM_IRQ_STATE_MASKED;
		return VMM_OK;
	}
	return VMM_EFAIL;
}

int vmm_host_irq_unmask(u32 hirq_num)
{
	struct vmm_host_irq * irq;
	if (hirq_num < ARCH_HOST_IRQ_COUNT) {
		irq = &hirqctrl.irq[hirq_num];
		if (irq->chip && irq->chip->irq_unmask) {
			irq->chip->irq_mask(irq);
			irq->state &= ~VMM_IRQ_STATE_MASKED;
		}
		return VMM_OK;
	}
	return VMM_EFAIL;
}

int vmm_host_irq_mask(u32 hirq_num)
{
	struct vmm_host_irq * irq;
	if (hirq_num < ARCH_HOST_IRQ_COUNT) {
		irq = &hirqctrl.irq[hirq_num];
		if (irq->chip && irq->chip->irq_mask) {
			irq->chip->irq_mask(irq);
			irq->state |= VMM_IRQ_STATE_MASKED;
		}
		return VMM_OK;
	}
	return VMM_EFAIL;
}

int vmm_host_irq_register(u32 hirq_num, 
			  const char *name,
			  vmm_host_irq_handler_t handler,
			  void *dev)
{
	bool found;
	irq_flags_t flags;
	struct dlist *l;
	struct vmm_host_irq *irq;
	struct vmm_host_irq_hndl *hirq;
	if (hirq_num < ARCH_HOST_IRQ_COUNT) {
		vmm_spin_lock_irqsave(&hirqctrl.lock, flags);
		irq = &hirqctrl.irq[hirq_num];
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
		irq->name = name;
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

int vmm_host_irq_unregister(u32 hirq_num, 
			    vmm_host_irq_handler_t handler)
{
	bool found;
	irq_flags_t flags;
	struct dlist *l;
	struct vmm_host_irq *irq;
	struct vmm_host_irq_hndl * hirq;
	if (hirq_num < ARCH_HOST_IRQ_COUNT) {
		vmm_spin_lock_irqsave(&hirqctrl.lock, flags);
		irq = &hirqctrl.irq[hirq_num];
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
			return vmm_host_irq_disable(hirq_num);
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

	/* Allocate memory for irq array */
	hirqctrl.irq = vmm_malloc(sizeof(struct vmm_host_irq) * 
				  ARCH_HOST_IRQ_COUNT);

	/* Reset the handler array */
	for (ite = 0; ite < ARCH_HOST_IRQ_COUNT; ite++) {
		hirqctrl.irq[ite].num = ite;
		hirqctrl.irq[ite].name = NULL;
		hirqctrl.irq[ite].state = (VMM_IRQ_TYPE_NONE | 
					   VMM_IRQ_STATE_DISABLED | 
					   VMM_IRQ_STATE_MASKED);
		hirqctrl.irq[ite].count = 0;
		hirqctrl.irq[ite].chip = NULL;
		hirqctrl.irq[ite].chip_data = NULL;
		INIT_LIST_HEAD(&hirqctrl.irq[ite].hndl_list);
	}

	/* Initialize board specific PIC */
	if ((ret = arch_host_irq_init())) {
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
