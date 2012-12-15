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
#include <vmm_cpumask.h>
#include <arch_regs.h>
#include <libs/list.h>

/**
 * enum vmm_irq_trigger_types
 * @VMM_IRQ_TYPE_NONE			- default, unspecified type
 * @VMM_IRQ_TYPE_EDGE_RISING		- rising edge triggered
 * @VMM_IRQ_TYPE_EDGE_FALLING		- falling edge triggered
 * @VMM_IRQ_TYPE_EDGE_BOTH		- rising and falling edge triggered
 * @VMM_IRQ_TYPE_LEVEL_HIGH		- high level triggered
 * @VMM_IRQ_TYPE_LEVEL_LOW		- low level triggered
 * @VMM_IRQ_TYPE_LEVEL_MASK		- Mask to filter out the level bits
 * @VMM_IRQ_TYPE_SENSE_MASK		- Mask for all the above bits
 */
enum vmm_irq_trigger_types {
	VMM_IRQ_TYPE_NONE		= 0x00000000,
	VMM_IRQ_TYPE_EDGE_RISING	= 0x00000001,
	VMM_IRQ_TYPE_EDGE_FALLING	= 0x00000002,
	VMM_IRQ_TYPE_EDGE_BOTH		= (VMM_IRQ_TYPE_EDGE_FALLING | VMM_IRQ_TYPE_EDGE_RISING),
	VMM_IRQ_TYPE_LEVEL_HIGH		= 0x00000004,
	VMM_IRQ_TYPE_LEVEL_LOW		= 0x00000008,
	VMM_IRQ_TYPE_LEVEL_MASK		= (VMM_IRQ_TYPE_LEVEL_LOW | VMM_IRQ_TYPE_LEVEL_HIGH),
	VMM_IRQ_TYPE_SENSE_MASK		= 0x0000000f,
};

/**
 * enum vmm_irq_states
 * @VMM_IRQ_STATE_TRIGGER_MASK		- Mask for the trigger type bits
 * @VMM_IRQ_STATE_NO_BALANCING		- Balancing disabled for this IRQ
 * @VMM_IRQ_STATE_PER_CPU		- Interrupt is per cpu
 * @VMM_IRQ_STATE_AFFINITY_SET		- Interrupt affinity was set
 * @VMM_IRQ_STATE_LEVEL			- Interrupt is level triggered
 * @VMM_IRQ_STATE_DISABLED		- Disabled state of the interrupt
 * @VMM_IRQ_STATE_MASKED		- Masked state of the interrupt
 * @VMM_IRQ_STATE_INPROGRESS		- In progress state of the interrupt
 */
enum vmm_irq_states {
	VMM_IRQ_STATE_TRIGGER_MASK	= 0xf,
	VMM_IRQ_STATE_PER_CPU		= (1 << 11),
	VMM_IRQ_STATE_AFFINITY_SET	= (1 << 12),
	VMM_IRQ_STATE_LEVEL		= (1 << 13),
	VMM_IRQ_STATE_DISABLED		= (1 << 16),
	VMM_IRQ_STATE_MASKED		= (1 << 17),
	VMM_IRQ_STATE_INPROGRESS	= (1 << 18),
};

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

/** Host IRQ Chip Abstraction 
 * @irq_enable:		enable the interrupt (defaults to chip->unmask if NULL)
 * @irq_disable:	disable the interrupt (defaults to chip->mask if NULL)
 * @irq_ack:		start of a new interrupt
 * @irq_mask:		mask an interrupt source
 * @irq_unmask:		unmask an interrupt source
 * @irq_eoi:		end of interrupt
 * @irq_set_affinity:	set the CPU affinity on SMP machines
 * @irq_set_type:	set the flow type (VMM_IRQ_TYPE_LEVEL/etc.) of an IRQ
 */
struct vmm_host_irq_chip {
	const char *name;
	void (*irq_enable)(struct vmm_host_irq *irq);
	void (*irq_disable)(struct vmm_host_irq *irq);
	void (*irq_ack)(struct vmm_host_irq *irq);
	void (*irq_mask)(struct vmm_host_irq *irq);
	void (*irq_unmask)(struct vmm_host_irq *irq);
	void (*irq_eoi)(struct vmm_host_irq *irq);
	int  (*irq_set_affinity)(struct vmm_host_irq *irq, 
				 const struct vmm_cpumask *dest, 
				 bool force);
	int  (*irq_set_type)(struct vmm_host_irq *irq, u32 flow_type);
};

/** Host IRQ Abstraction */
struct vmm_host_irq {
	u32 num;
	const char *name;
	u32 state;
	u32 count[CONFIG_CPU_COUNT];
	void *chip_data;
	struct vmm_host_irq_chip *chip;
	struct dlist hndl_list;
};

/** Explicity report a host irq 
 * (Note: To be called from architecture specific code)
 * (Note: This will be typically called by nested/secondary PICs) 
 */
int vmm_host_generic_irq_exec(u32 cpu_irq_no, arch_regs_t *regs);

/** Report external irq as seen from CPU 
 * (Note: To be called from architecture specific code) 
 */
int vmm_host_irq_exec(u32 cpu_irq_no, arch_regs_t *regs);

/** Get host irq count */
u32 vmm_host_irq_count(void);

/** Get host irq instance from host irq number */
struct vmm_host_irq *vmm_host_irq_get(u32 hirq_num);

/* Set host irq chip for given host irq number */
int vmm_host_irq_set_chip(u32 hirq_num, struct vmm_host_irq_chip *chip);

/* Set host irq chip data for given host irq number */
int vmm_host_irq_set_chip_data(u32 hirq_num, void *chip_data);

/** Get host irq number from host irq instance */
static inline u32 vmm_host_irq_get_num(struct vmm_host_irq *irq)
{
	return (irq) ? irq->num : 0;
}

/** Check if a host irq is per-cpu */
static inline bool vmm_host_irq_is_per_cpu(struct vmm_host_irq *irq)
{
	return irq->state & VMM_IRQ_STATE_PER_CPU;
}

/** Check if a host irq is affinity was set */
static inline bool vmm_host_irq_affinity_was_set(struct vmm_host_irq *irq)
{
	return irq->state & VMM_IRQ_STATE_AFFINITY_SET;
}

/** Get trigger type of a host irq */
static inline u32 vmm_host_irq_get_trigger_type(struct vmm_host_irq *irq)
{
	return irq->state & VMM_IRQ_STATE_TRIGGER_MASK;
}

#if 0
/*
 * Must only be called inside irq_chip.irq_set_type() functions.
 */
static inline void vmm_host_irq_set_trigger_type(struct vmm_host_irq *irq, u32 type)
{
	irq->state &= ~VMM_IRQ_STATE_TRIGGER_MASK;
	irq->state |= type & VMM_IRQ_STATE_TRIGGER_MASK;
}
#endif

/** Check if a host irq is of level type */
static inline bool vmm_host_irq_is_level_type(struct vmm_host_irq *irq)
{
	return irq->state & VMM_IRQ_STATE_LEVEL;
}

/** Check if a host irq is disabled */
static inline bool vmm_host_irq_is_disabled(struct vmm_host_irq *irq)
{
	return irq->state & VMM_IRQ_STATE_DISABLED;
}

/** Check if a host irq is masked */
static inline bool vmm_host_irq_irq_masked(struct vmm_host_irq *irq)
{
	return irq->state & VMM_IRQ_STATE_MASKED;
}

/** Check if a host irq is in-progress */
static inline bool vmm_host_irq_irq_inprogress(struct vmm_host_irq *irq)
{
	return irq->state & VMM_IRQ_STATE_INPROGRESS;
}

/** Get host irq count from host irq instance */
static inline u32 vmm_host_irq_get_count(struct vmm_host_irq *irq, u32 cpu)
{
	if (cpu < CONFIG_CPU_COUNT) {
		return (irq) ? irq->count[cpu] : 0;
	}
	return 0;
}

/** Get host irq chip instance from host irq instance */
static inline struct vmm_host_irq_chip *vmm_host_irq_get_chip(
						struct vmm_host_irq *irq)
{
	return (irq) ? irq->chip : NULL;
}

/** Get host irq chip data from host irq instance */
static inline void *vmm_host_irq_get_chip_data(struct vmm_host_irq *irq)
{
	return (irq) ? irq->chip_data : NULL;
}

/** Set cpu affinity of given host irq */
int vmm_host_irq_set_affinity(u32 hirq_num, 
			      const struct vmm_cpumask *dest, 
			      bool force);

/** Set trigger type for given host irq */
int vmm_host_irq_set_type(u32 hirq_num, u32 type);

/** Mark per cpu state for given host irq */
int vmm_host_irq_mark_per_cpu(u32 hirq_num);

/** UnMark per cpu state for given host irq */
int vmm_host_irq_unmark_per_cpu(u32 hirq_num);

/** Enable a host irq (by default all irqs are disabled) */
int vmm_host_irq_enable(u32 hirq_num);

/** Disable a host irq */
int vmm_host_irq_disable(u32 hirq_num);

/** Unmask a host irq (by default all irqs are masked) */
int vmm_host_irq_unmask(u32 hirq_num);

/** Mask a host irq */
int vmm_host_irq_mask(u32 hirq_num);

/** Register handler for given irq */
int vmm_host_irq_register(u32 hirq_num, 
			  const char *name,
			  vmm_host_irq_handler_t handler,
			  void *dev);

/** Unregister handler for given irq */
int vmm_host_irq_unregister(u32 hirq_num, 
			    void *dev);

/** Interrupts initialization function */
int vmm_host_irq_init(void);

#endif
