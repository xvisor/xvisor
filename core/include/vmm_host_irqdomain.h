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
 * @file vmm_host_irqdomain.h
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @author Anup Patel (anup@brainfault.org)
 * @brief IRQ domain support, kind of Xvior compatible Linux IRQ domain.
 */

#ifndef _VMM_HOST_IRQDOMAIN_H__
#define _VMM_HOST_IRQDOMAIN_H__

#include <vmm_types.h>
#include <vmm_spinlocks.h>
#include <vmm_devtree.h>
#include <libs/list.h>

struct vmm_chardev;
struct vmm_host_irqdomain;

/**
 * struct vmm_host_irqdomain_ops - Methods for vmm_host_irqdomain objects
 * @match: Match an interrupt controller device node to a host, returns
 *         1 on a match
 * @map: Create or update a mapping between a virtual irq number and a hw
 *       irq number. This is called only once for a given mapping.
 * @unmap: Dispose of such a mapping
 * @xlate: Given a device tree node and interrupt specifier, decode
 *         the hardware irq number and linux irq type value.
 *
 * Functions below are provided by the driver and called whenever a new
 * mapping is created or an old mapping is disposed. The driver can then
 * proceed to whatever internal data structures management is required.
 * It also needs to setup the irq_desc when returning from map().
 */
struct vmm_host_irqdomain_ops {
	int (*match)(struct vmm_host_irqdomain *d,
		     struct vmm_devtree_node *node);
	int (*map)(struct vmm_host_irqdomain *d,
		   unsigned int hirq, unsigned int hwirq);
	void (*unmap)(struct vmm_host_irqdomain *d, unsigned int hirq);
	int (*xlate)(struct vmm_host_irqdomain *d,
		     struct vmm_devtree_node *node,
		     const u32 *intspec, unsigned int intsize,
		     unsigned long *out_hwirq, unsigned int *out_type);
};

/**
 * struct vmm_host_irqdomain - IRQ domain, kind of Linux IRQ domain
 * @head:	List head for registration
 * @base:	Base
 * @count:	The number of IRQs contained.
 * @ops:	Pointer to vmm_host_irqdomain methods.
 * @irqs:	The extended IRQ array
 *
 * Optional elements
 * @of_node:	The device node using this domain
 * @host_data:	The controller private data pointer. Not touched by extended
 *		IRQ core code.
 * @bmap_lock:	The IRQ domain bitmap lock
 * @bmap:	The IRQ domain bitmap
 */
struct vmm_host_irqdomain {
	struct dlist				head;
	bool					uses_irqext;
	unsigned int				base;
	unsigned int				count;
	unsigned int				end;
	const struct vmm_host_irqdomain_ops	*ops;
	struct vmm_devtree_node			*of_node;
	void					*host_data;
	vmm_spinlock_t				bmap_lock;
	unsigned long				*bmap;
};

/** Convert host IRQ to HW IRQ */
int vmm_host_irqdomain_to_hwirq(struct vmm_host_irqdomain *domain,
				unsigned int hirq);

/** Convert HW IRQ to host IRQ */
int vmm_host_irqdomain_to_hirq(struct vmm_host_irqdomain *domain,
				unsigned int hwirq);

/** Find host IRQ for givne HW IRQ */
int vmm_host_irqdomain_find_mapping(struct vmm_host_irqdomain *domain,
				    unsigned int hwirq);

/** Find matching host IRQ domain based on given match function */
struct vmm_host_irqdomain *vmm_host_irqdomain_match(void *data,
			int (*fn)(struct vmm_host_irqdomain *, void *));

/** Dump host IRQ domain debug info */
void vmm_host_irqdomain_debug_dump(struct vmm_chardev *cdev);

/** Find host IRQ domain for given host IRQ */
struct vmm_host_irqdomain *vmm_host_irqdomain_get(unsigned int hirq);

/** Create mapping in host IRQ domain for given HW IRQ */
int vmm_host_irqdomain_create_mapping(struct vmm_host_irqdomain *domain,
				      unsigned int hwirq);

/** Dispose mapping in host IRQ domain associated with given host IRQ */
void vmm_host_irqdomain_dispose_mapping(unsigned int hirq);

/** Allocate and map host IRQs */
int vmm_host_irqdomain_alloc(struct vmm_host_irqdomain *domain,
			     unsigned int irq_count);

/** Free and unmap host IRQs */
void vmm_host_irqdomain_free(struct vmm_host_irqdomain *domain,
			     unsigned int hirq, unsigned int irq_count);

/** Translate device tree cells to HW IRQ for given host IRQ domain
 *  using xlate() callback provided in host IRQ domain ops.
 */
int vmm_host_irqdomain_xlate(struct vmm_host_irqdomain *domain,
			     const u32 *intspec, unsigned int intsize,
			     unsigned long *out_hwirq, unsigned int *out_type);

/** Common xlate() callback to translate device tree cell */
int vmm_host_irqdomain_xlate_onecell(struct vmm_host_irqdomain *domain,
			struct vmm_devtree_node *node,
			const u32 *intspec, unsigned int intsize,
			unsigned long *out_hwirq, unsigned int *out_type);

/**
 * Allocate and register a new host IRQ domain.
 * @of_node: pointer to interrupt controller's device tree node.
 * @base: Base host IRQ number. If < 0 then extended IRQs are created.
 * @size: Number of interrupts in the domain.
 * @ops: map/unmap domain callbacks.
 * @host_data: Controller private data pointer.
 */
struct vmm_host_irqdomain *vmm_host_irqdomain_add(
				struct vmm_devtree_node *of_node,
				int base, unsigned int size,
				const struct vmm_host_irqdomain_ops *ops,
				void *host_data);

/** Remove existing host IRQ domain */
void vmm_host_irqdomain_remove(struct vmm_host_irqdomain *domain);

/** Initialize host IRQ domain framework */
int vmm_host_irqdomain_init(void);

extern const struct vmm_host_irqdomain_ops irqdomain_simple_ops;

#endif /* _VMM_HOST_IRQDOMAIN_H__ */
