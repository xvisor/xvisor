/**
 * Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWide.
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
 * @file vmm_host_extirq.h
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @brief Extended IRQ support, kind of IRQ domain for Xvisor.
 */

#ifndef _VMM_HOST_EXTIRQ_H__
# define _VMM_HOST_EXTIRQ_H__

#include <vmm_types.h>
#include <vmm_error.h>
#include <vmm_devtree.h>
#include <vmm_host_irq.h>
#include <libs/list.h>

struct vmm_chardev;
struct vmm_host_extirq_group;

/**
 * struct vmm_host_extirq_group_ops - Methods for vmm_host_extirq_group objects
 * @match: Match an interrupt controller device node to a host, returns
 *         1 on a match
 * @map: Create or update a mapping between a virtual irq number and a hw
 *       irq number. This is called only once for a given mapping.
 * @unmap: Dispose of such a mapping
 * @xlate: Given a device tree node and interrupt specifier, decode
 *         the hardware irq number and linux irq type value.
 *
 * Functions below are provided by the driver and called whenever a new mapping
 * is created or an old mapping is disposed. The driver can then proceed to
 * whatever internal data structures management is required. It also needs
 * to setup the irq_desc when returning from map().
 */
struct vmm_host_extirq_group_ops {
	int (*match)(struct vmm_host_extirq_group *d, struct vmm_devtree_node *node);
	int (*map)(struct vmm_host_extirq_group *d, unsigned int virq, unsigned int hw);
	void (*unmap)(struct vmm_host_extirq_group *d, unsigned int virq);
	int (*xlate)(struct vmm_host_extirq_group *d, struct vmm_devtree_node *node,
		     const u32 *intspec, unsigned int intsize,
		     unsigned long *out_hwirq, unsigned int *out_type);
};

/**
 * struct vmm_host_extirq_group - Extended IRQ group, kind of Linux IRQ domain
 * @head:	List head for registration
 * @count:	The number of extended IRQ contained.
 * @ops:	Pointer to vmm_host_extirq methods.
 * @irqs:	The extended IRQ array
 *
 * Optional elements
 * @of_node:	The device node using this group
 * @hwirq:	The associated real HW irq.
 * @host_data:	The controller private data pointer. Not touched by extended
 *		IRQ core code.
 */
/* @map:	The IRQ group mapping. */
struct vmm_host_extirq_group {
	struct dlist				head;
	unsigned int				base;
	unsigned int				count;
	unsigned int				end;
	void					*host_data;
	struct vmm_devtree_node			*of_node;
	const struct vmm_host_extirq_group_ops	*ops;
};

struct vmm_host_irq *vmm_host_extirq_get(u32 eirq_no);

int vmm_host_extirq_to_hwirq(struct vmm_host_extirq_group *group,
			     unsigned int irq);

int vmm_host_extirq_find_mapping(struct vmm_host_extirq_group *group,
				 unsigned int offset);

struct vmm_host_extirq_group *
vmm_host_extirq_group_get(unsigned int	irq_num);

int vmm_host_extirq_create_mapping(struct vmm_host_extirq_group *group,
				   unsigned int	irq_num);

void vmm_host_extirq_dispose_mapping(unsigned int irq_num);

void vmm_host_extirq_debug_dump(struct vmm_chardev *cdev);

/**
 * vmm_host_extirq_add() - Allocate and register a new extended IRQ group.
 * @of_node: pointer to interrupt controller's device tree node.
 * @size: Number of interrupts in the domain.
 * @ops: map/unmap domain callbacks.
 * @host_data: Controller private data pointer.
 */
struct vmm_host_extirq_group
*vmm_host_extirq_add(struct vmm_devtree_node *of_node,
		     unsigned int size,
		     const struct vmm_host_extirq_group_ops *ops,
		     void *host_data);

void vmm_host_extirq_remove(struct vmm_host_extirq_group *group);

int vmm_host_extirq_init(void);

extern const struct vmm_host_extirq_group_ops extirq_simple_ops;

#endif /* _VMM_HOST_EXTIRQ_H__ */
