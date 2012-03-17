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
 * @file vmm_host_irq.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for host interrupts
 */
#ifndef _VMM_HOST_IRQ_H__
#define _VMM_HOST_IRQ_H__

#include <vmm_types.h>
#include <vmm_list.h>
#include <arch_regs.h>

/**
 * enum vmm_irq_return
 * @VMM_IRQ_NONE		interrupt was not from this device
 * @VMM_IRQ_HANDLED		interrupt was handled by this device
 */
enum vmm_irq_return {
	VMM_IRQ_NONE		= (0 << 0),
	VMM_IRQ_HANDLED		= (1 << 0),
};

typedef enum vmm_irq_return vmm_irq_return_t;

typedef vmm_irq_return_t (*vmm_host_irq_handler_t) (u32 irq_no, 
						    arch_regs_t * regs,
						    void *dev);

/** Host IRQ Handler Abstraction */
struct vmm_host_irq_hndl {
	struct dlist head;
	vmm_host_irq_handler_t hndl;
	void *dev;
};

struct vmm_host_irq;

/** Host IRQ Chip Abstraction */
struct vmm_host_irq_chip {
	const char *name;
	void (*irq_ack)(struct vmm_host_irq *irq);
	void (*irq_mask)(struct vmm_host_irq *irq);
	void (*irq_unmask)(struct vmm_host_irq *irq);
	void (*irq_eoi)(struct vmm_host_irq *irq);
};

/** Host IRQ Abstraction */
struct vmm_host_irq {
	u32 num;
	bool enabled;
	u32 count;
	void * chip_data;
	struct vmm_host_irq_chip * chip;
	struct dlist hndl_list;
};

/** Execute host interrupts (To be called from architecture specific code) */
int vmm_host_irq_exec(u32 cpu_irq_no, arch_regs_t * regs);

/* Set host irq chip for given host irq number */
int vmm_host_irq_set_chip(u32 hirq_num, struct vmm_host_irq_chip *chip);

/* Set host irq chip data for given host irq number */
int vmm_host_irq_set_chip_data(u32 hirq_num, void * chip_data);

/** Get host irq instance from host irq number */
struct vmm_host_irq * vmm_host_irq_get(u32 hirq_num);

/** Get host irq count from host irq instance */
static inline u32 vmm_host_irq_get_count(struct vmm_host_irq *irq)
{
	return (irq) ? irq->count : 0;
}

/** Get host irq chip instance from host irq instance */
static inline struct vmm_host_irq_chip * vmm_host_irq_get_chip(
						struct vmm_host_irq *irq)
{
	return (irq) ? irq->chip : NULL;
}

/** Get host irq chip data from host irq instance */
static inline void * vmm_host_irq_get_chip_data(struct vmm_host_irq *irq)
{
	return (irq) ? irq->chip_data : NULL;
}

/** Check if a host irq is enabled */
bool vmm_host_irq_isenabled(u32 hirq_num);

/** Enable a host irq (by default all irqs are enabled) */
int vmm_host_irq_enable(u32 hirq_num);

/** Disable a host irq */
int vmm_host_irq_disable(u32 hirq_num);

/** Register handler for given irq */
int vmm_host_irq_register(u32 hirq_num, 
			  vmm_host_irq_handler_t handler,
			  void *dev);

/** Unregister handler for given irq */
int vmm_host_irq_unregister(u32 hirq_num, 
			    vmm_host_irq_handler_t handler);

/** Interrupts initialization function */
int vmm_host_irq_init(void);

#endif
