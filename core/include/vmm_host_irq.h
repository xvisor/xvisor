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
#include <vmm_spinlocks.h>
#include <vmm_devtree.h>
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
 * @VMM_IRQ_STATE_PER_CPU		- Interrupt is per cpu
 * @VMM_IRQ_STATE_AFFINITY_SET		- Interrupt affinity was set
 * @VMM_IRQ_STATE_LEVEL			- Interrupt is level triggered
 * @VMM_IRQ_STATE_ROUTED		- Interrupt is routed to some guest
 * @VMM_IRQ_STATE_IPI			- Interrupt is an inter-processor interrupt
 * @VMM_IRQ_STATE_EXTENDED		- Interrupt is an extended interrupt
 * @VMM_IRQ_STATE_DISABLED		- Disabled state of the interrupt
 * @VMM_IRQ_STATE_MASKED		- Masked state of the interrupt
 */
enum vmm_irq_states {
	VMM_IRQ_STATE_TRIGGER_MASK	= 0xf,
	VMM_IRQ_STATE_PER_CPU		= (1 << 11),
	VMM_IRQ_STATE_AFFINITY_SET	= (1 << 12),
	VMM_IRQ_STATE_LEVEL		= (1 << 13),
	VMM_IRQ_STATE_ROUTED		= (1 << 14),
	VMM_IRQ_STATE_IPI		= (1 << 15),
	VMM_IRQ_STATE_EXTENDED		= (1 << 16),
	VMM_IRQ_STATE_DISABLED		= (1 << 17),
	VMM_IRQ_STATE_MASKED		= (1 << 18),
};

/**
 * enum vmm_routed_irq_states
 * @VMM_ROUTED_IRQ_STATE_PENDING	- Routed interrupt is pending
 * @VMM_ROUTED_IRQ_STATE_ACTIVE		- Routed interrupt is active
 * @VMM_ROUTED_IRQ_STATE_MASKED		- Routed interrupt is masked
 */
enum vmm_routed_irq_states {
	VMM_ROUTED_IRQ_STATE_PENDING	= (1 << 0),
	VMM_ROUTED_IRQ_STATE_ACTIVE	= (1 << 1),
	VMM_ROUTED_IRQ_STATE_MASKED	= (1 << 2),
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

struct vmm_host_irq;

typedef enum vmm_irq_return vmm_irq_return_t;
typedef void (*vmm_host_irq_handler_t)(struct vmm_host_irq *, u32, void *);
typedef vmm_irq_return_t (*vmm_host_irq_function_t) (int irq_no, void *dev);

/** Host IRQ Action Abstraction */
struct vmm_host_irq_action {
	struct dlist head;
	vmm_host_irq_function_t func;
	void *dev;
};

/** Host IRQ Chip Abstraction
 * @irq_enable:           enable the interrupt (defaults to chip->unmask if NULL)
 * @irq_disable:          disable the interrupt (defaults to chip->mask if NULL)
 * @irq_ack:              start of a new interrupt
 * @irq_mask:             mask an interrupt source
 * @irq_unmask:           unmask an interrupt source
 * @irq_eoi:              end of interrupt
 * @irq_set_affinity:     set the CPU affinity on SMP machines
 * @irq_set_type:         set the flow type (VMM_IRQ_TYPE_LEVEL/etc.) of an IRQ
 * @irq_get_routed_state: get the routed state of an IRQ
 * @irq_set_routed_state: set the routed state of an IRQ
 */
struct vmm_host_irq_chip {
	const char *name;
	void (*irq_enable)(struct vmm_host_irq *irq);
	void (*irq_disable)(struct vmm_host_irq *irq);
	void (*irq_ack)(struct vmm_host_irq *irq);
	void (*irq_mask)(struct vmm_host_irq *irq);
	void (*irq_mask_ack)(struct vmm_host_irq *irq);
	void (*irq_unmask)(struct vmm_host_irq *irq);
	void (*irq_eoi)(struct vmm_host_irq *irq);
	int  (*irq_set_affinity)(struct vmm_host_irq *irq,
				 const struct vmm_cpumask *dest,
				 bool force);
	int  (*irq_set_type)(struct vmm_host_irq *irq, u32 flow_type);
	void (*irq_raise)(struct vmm_host_irq *irq,
			  const struct vmm_cpumask *dest);
	u32  (*irq_get_routed_state)(struct vmm_host_irq *irq, u32 mask);
	void (*irq_set_routed_state)(struct vmm_host_irq *irq,
				     u32 val, u32 mask);
};

/** Host IRQ Abstraction */
struct vmm_host_irq {
	u32 num;
	u32 hwirq;
	const char *name;
	u32 state;
	u32 count[CONFIG_CPU_COUNT];
	bool in_progress[CONFIG_CPU_COUNT];
	void *chip_data;
	struct vmm_host_irq_chip *chip;
	vmm_host_irq_handler_t handler;
	void *handler_data;
	vmm_rwlock_t action_lock[CONFIG_CPU_COUNT];
	struct dlist action_list[CONFIG_CPU_COUNT];
};

/* nodeid table based host irq initialization callback */
typedef int (*vmm_host_irq_init_t)(struct vmm_devtree_node *);

/* declare nodeid table based initialization for host irq */
#define VMM_HOST_IRQ_INIT_DECLARE(name, compat, fn)	\
VMM_DEVTREE_NIDTBL_ENTRY(name, "host_irq", "", "", compat, fn)

/** Explicity report a host irq
 * (Note: To be called from architecture specific code)
 * (Note: This will be typically called by nested/secondary PICs)
 */
int vmm_host_generic_irq_exec(u32 hirq_no);

/** Report active irq as seen from CPU
 * (Note: To be called from architecture specific code)
 */
int vmm_host_active_irq_exec(u32 cpu_irq_no);

/** Set callback for retriving active host irq number */
void vmm_host_irq_set_active_callback(u32 (*active)(u32));

/** Initialize host irq instance 
 *  Note: This function is for internal use only.
 *  Note: Do not call this function directly.
 */
void __vmm_host_irq_init_desc(struct vmm_host_irq *irq,
			      u32 hirq, u32 hwirq, u32 state);

/** Get host irq count */
u32 vmm_host_irq_count(void);

/** Set hwirq associated with host irq instance */
int vmm_host_irq_set_hwirq(u32 hirq, u32 hwirq);

/** Get hwirq associated with host irq instance */
u32 vmm_host_irq_get_hwirq(u32 hirq);

/** Get host irq instance from host irq number */
struct vmm_host_irq *vmm_host_irq_get(u32 hirq);

/** Set host irq chip for given host irq number */
int vmm_host_irq_set_chip(u32 hirq, struct vmm_host_irq_chip *chip);

/** Get host irq chip instance from host irq instance */
struct vmm_host_irq_chip *vmm_host_irq_get_chip(struct vmm_host_irq *irq);

/** Set host irq chip data for given host irq number */
int vmm_host_irq_set_chip_data(u32 hirq, void *chip_data);

/** Get host irq chip data from host irq instance */
void *vmm_host_irq_get_chip_data(struct vmm_host_irq *irq);

/** Set host irq handler for given host irq number
 *  NOTE: For second argument, mention one of the
 *  vmm_handle_xxxxx functions from below
 */
int vmm_host_irq_set_handler(u32 hirq, vmm_host_irq_handler_t handler);

/** Get host irq handler for given host irq number */
vmm_host_irq_handler_t vmm_host_irq_get_handler(u32 hirq);

/** Set host irq handler data for given host irq number  */
int vmm_host_irq_set_handler_data(u32 hirq, void *data);

/** Get host irq handler data for given host irq number */
void *vmm_host_irq_get_handler_data(u32 hirq);

/** Per-CPU irq handler */
void vmm_handle_percpu_irq(struct vmm_host_irq *irq, u32 cpu, void *data);

/** Fast EOI irq handler */
void vmm_handle_fast_eoi(struct vmm_host_irq *irq, u32 cpu, void *data);

/** Level irq handler */
void vmm_handle_level_irq(struct vmm_host_irq *irq, u32 cpu, void *data);

/** Get host irq number from host irq instance */
static inline u32 vmm_host_irq_get_num(struct vmm_host_irq *irq)
{
	return (irq) ? irq->num : 0;
}

/** Set host irq name from host irq instance */
static inline void vmm_host_irq_set_name(struct vmm_host_irq *irq,
					 const char *name)
{
	if (irq) {
		irq->name = name;
	}
}

/** Get host irq name from host irq instance */
static inline const char *vmm_host_irq_get_name(struct vmm_host_irq *irq)
{
	return (irq) ? irq->name : 0;
}

/** Check if a host irq is per-cpu */
static inline bool vmm_host_irq_is_per_cpu(struct vmm_host_irq *irq)
{
	return (irq->state & VMM_IRQ_STATE_PER_CPU) ? TRUE : FALSE;
}

/** Check if a host irq is affinity was set */
static inline bool vmm_host_irq_affinity_was_set(struct vmm_host_irq *irq)
{
	return (irq->state & VMM_IRQ_STATE_AFFINITY_SET) ? TRUE : FALSE;
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
	return (irq->state & VMM_IRQ_STATE_LEVEL) ? TRUE : FALSE;
}

/** Check if a host irq is routed to some guest */
static inline bool vmm_host_irq_is_routed(struct vmm_host_irq *irq)
{
	return (irq->state & VMM_IRQ_STATE_ROUTED) ? TRUE : FALSE;
}

/** Check if a host irq is inter-processor interrupt */
static inline bool vmm_host_irq_is_ipi(struct vmm_host_irq *irq)
{
	return (irq->state & VMM_IRQ_STATE_IPI) ? TRUE : FALSE;
}

/** Check if a host irq is disabled */
static inline bool vmm_host_irq_is_disabled(struct vmm_host_irq *irq)
{
	return (irq->state & VMM_IRQ_STATE_DISABLED) ? TRUE : FALSE;
}

/** Check if a host irq is masked */
static inline bool vmm_host_irq_is_masked(struct vmm_host_irq *irq)
{
	return (irq->state & VMM_IRQ_STATE_MASKED) ? TRUE : FALSE;
}

/** Check if a host irq is in-progress */
static inline bool vmm_host_irq_is_inprogress(struct vmm_host_irq *irq, u32 cpu)
{
	if (cpu < CONFIG_CPU_COUNT) {
		return (irq) ? irq->in_progress[cpu] : FALSE;
	}
	return FALSE;
}

/** Get host irq count from host irq instance */
static inline u32 vmm_host_irq_get_count(struct vmm_host_irq *irq, u32 cpu)
{
	if (cpu < CONFIG_CPU_COUNT) {
		return (irq) ? irq->count[cpu] : 0;
	}
	return 0;
}

/** Set cpu affinity of given host irq */
int vmm_host_irq_set_affinity(u32 hirq,
			      const struct vmm_cpumask *dest,
			      bool force);

/** Set trigger type for given host irq */
int vmm_host_irq_set_type(u32 hirq, u32 type);

/** Mark host irq as per cpu */
int vmm_host_irq_mark_per_cpu(u32 hirq);

/** UnMark host irq as per cpu */
int vmm_host_irq_unmark_per_cpu(u32 hirq);

/** Mark host irq as routed to some guest */
int vmm_host_irq_mark_routed(u32 hirq);

/** UnMark host irq as routed to some guest */
int vmm_host_irq_unmark_routed(u32 hirq);

/** Get host irq routed state */
int vmm_host_irq_get_routed_state(u32 hirq, u32 *val, u32 mask);

/** Set/update host irq routed state */
int vmm_host_irq_set_routed_state(u32 hirq, u32 val, u32 mask);

/** Mark host irq as inter-processor interrupt */
int vmm_host_irq_mark_ipi(u32 hirq);

/** UnMark host irq as inter-processor interrupt */
int vmm_host_irq_unmark_ipi(u32 hirq);

/** Enable a host irq (by default all irqs are disabled) */
int vmm_host_irq_enable(u32 hirq);

/** Disable a host irq */
int vmm_host_irq_disable(u32 hirq);

/** Unmask a host irq (by default all irqs are masked) */
int vmm_host_irq_unmask(u32 hirq);

/** Mask a host irq */
int vmm_host_irq_mask(u32 hirq);

/** Raise a host irq from software */
int vmm_host_irq_raise(u32 hirq,
		       const struct vmm_cpumask *dest);

/** Find a host irq with matching state mask */
int vmm_host_irq_find(u32 hirq_start, u32 state_mask, u32 *hirq);

/** Register function callback for given irq */
int vmm_host_irq_register(u32 hirq,
			  const char *name,
			  vmm_host_irq_function_t func,
			  void *dev);

/** Unregister function callback for given irq */
int vmm_host_irq_unregister(u32 hirq,
			    void *dev);

/** Interrupts initialization function */
int vmm_host_irq_init(void);

/*
 * Entry/exit functions for chained handlers where the primary IRQ chip
 * may implement either fasteoi or level-trigger flow control.
 */
static inline void vmm_chained_irq_enter(struct vmm_host_irq_chip *chip,
					 struct vmm_host_irq *desc)
{
	/* FastEOI controllers require no action on entry. */
	if (chip->irq_eoi)
		return;

	if (chip->irq_mask_ack) {
		chip->irq_mask_ack(desc);
	} else {
		chip->irq_mask(desc);
		if (chip->irq_ack)
			chip->irq_ack(desc);
	}
}

static inline void vmm_chained_irq_exit(struct vmm_host_irq_chip *chip,
					struct vmm_host_irq *desc)
{
	if (chip->irq_eoi)
		chip->irq_eoi(desc);
        else
		chip->irq_unmask(desc);
}

#endif
