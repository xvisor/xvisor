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
 * @file vmm_devtree_irq.c
 * @author Jimmy Durand Wesolowski <jimmy.durand-wesolowski@openwide.fr>
 * @author Anup Patel (anup@brainfault.org)
 * @brief Host IRQ device tree functions
 *
 * The source has been largely adapted from the Linux kernel v3.16:
 * drivers/of/irq.c and kernel/irq/irqdomain.c
 *
 * The original code is licensed under the GPL.
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_host_io.h>
#include <vmm_devtree.h>
#include <vmm_host_irq.h>
#include <vmm_host_irqdomain.h>
#include <libs/mathlib.h>

#define pr_warn(msg...)			vmm_printf(msg)
#ifdef DEBUG
#define pr_debug(msg...)                vmm_printf(msg)
#else
#define pr_debug(msg...)
#endif

u32 vmm_devtree_irq_count(struct vmm_devtree_node *node)
{
        u32 alen, tmp, rc;
        struct vmm_devtree_node *parent = vmm_devtree_irq_find_parent(node);

        if (!node || !parent) {
                return 0;
        }

        rc = vmm_devtree_read_u32(parent, "#interrupt-cells", &tmp);
        vmm_devtree_dref_node(parent);
        if (rc) {
		return 0;
	}

        alen = vmm_devtree_attrlen(node, VMM_DEVTREE_INTERRUPTS_ATTR_NAME);

        return udiv32(alen, (sizeof(u32) * tmp));
}

struct vmm_devtree_node *vmm_devtree_irq_find_parent(
					struct vmm_devtree_node *child)
{
	struct vmm_devtree_node *p;
	const u32 *parp;

	if (!child) {
		return NULL;
	}
	vmm_devtree_ref_node(child);

	do {
		parp = vmm_devtree_attrval(child, "interrupt-parent");
		if (parp == NULL) {
			p = child->parent;
			vmm_devtree_ref_node(child->parent);
		} else {
			p = vmm_devtree_find_node_by_phandle(
				vmm_be32_to_cpu(*parp));
		}
		vmm_devtree_dref_node(child);
		child = p;
	} while (p && vmm_devtree_attrval(p, "#interrupt-cells") == NULL);

	return p;
}

int vmm_devtree_irq_parse_one(struct vmm_devtree_node *device, int index,
			      struct vmm_devtree_phandle_args *out_irq)
{
	struct vmm_devtree_node *p = NULL;
	struct vmm_devtree_attr *attr = NULL;
	u32 *intspec = NULL;
	u32 intsize = 0;
	u32 intlen = 0;
	int res = VMM_EINVALID;
	int i;

	if (!device || (index < 0) || !out_irq) {
		return VMM_EINVALID;
	}

	pr_debug("%s: dev=%s, index=%d\n", __func__, device->name, index);

	attr = vmm_devtree_getattr(device, "interrupts");
	if (NULL == attr) {
		return VMM_EINVALID;
	}

	intlen = attr->len / sizeof(u32);
	intspec = attr->value;
	pr_debug(" intspec=%d intlen=%d\n", vmm_be32_to_cpu(*intspec), intlen);

	/* Look for the interrupt parent. */
	p = vmm_devtree_irq_find_parent(device);
	if (NULL == p) {
		/* If no interrupt-parent fount then
		 * read interrupts attribute directly
		 */
		res = vmm_devtree_read_u32_atindex(device,
					VMM_DEVTREE_INTERRUPTS_ATTR_NAME,
					&intsize, index);
		if (res != VMM_OK) {
			return res;
		}
		out_irq->np = NULL;
		out_irq->args_count = 1;
		out_irq->args[0] = intsize;
		return VMM_OK;
	}

	/* Get size of interrupt specifier */
	res = vmm_devtree_read_u32(p, "#interrupt-cells", &intsize);
	if (VMM_OK != res) {
		vmm_devtree_dref_node(p);
		return res;
	}

	pr_debug(" intsize=%d intlen=%d\n", intsize, intlen);

	/* Check index */
	if ((index + 1) * intsize > intlen) {
		vmm_devtree_dref_node(p);
		return VMM_EINVALID;
	}

	/* Copy intspec into irq structure */
	intspec += index * intsize;
	out_irq->np = p;
	out_irq->args_count = intsize;
	for (i = 0; i < intsize && i < VMM_MAX_PHANDLE_ARGS; i++) {
		out_irq->args[i] = vmm_be32_to_cpu(*intspec++);
	}

	return VMM_OK;
}

static int devtree_irqdomain_match_node(struct vmm_host_irqdomain *domain,
					void *node)
{
	if (domain->of_node == node) {
		return 1;
	}
	return 0;
}

struct vmm_host_irqdomain *vmm_devtree_irqdomain_find(
					struct vmm_devtree_node *node)
{
	return vmm_host_irqdomain_match(node, &devtree_irqdomain_match_node);
}

static unsigned int vmm_devtree_irq_create_mapping(
				struct vmm_devtree_phandle_args *irq_data)
{
	int rc;
	struct vmm_host_irqdomain *domain = NULL;
	struct vmm_host_irq *irq = NULL;
	long unsigned int hwirq;
	unsigned int hirq, type = VMM_IRQ_TYPE_NONE;

	if (irq_data->np) {
		domain = vmm_devtree_irqdomain_find(irq_data->np);
		if (!domain) {
			/* If no domain found then this is static host irq.
			 *
			 * In this case, we call vmm_host_irq API to
			 * find-out vmm_host_irq_chip which will have
			 * xlate() callback similar (but not same as)
			 * to xlate() callback of vmm_host_irqdomain.
			 *
			 * The xlate() callback of vmm_host_irq_chip
			 * will translate interrupt cells into host irq
			 * number. It will also configure the host irq
			 * if required.
			 */

			/* Currently as a work-around we return value
			 * of first cell from interrupt cells.
			 */
			return irq_data->args[0];
		}
	} else {
		return irq_data->args[0];
	}

	pr_debug("Domain %s found\n", domain->of_node->name);

	/* Determine translation */
	rc = vmm_host_irqdomain_xlate(domain, irq_data->args,
				      irq_data->args_count, &hwirq, &type);
	if (rc < 0) {
		return rc;
	}

	/* Create mapping */
	rc = vmm_host_irqdomain_create_mapping(domain, hwirq);
	if (rc < 0) {
		return rc;
	}
	hirq = rc;

	pr_debug("Extended IRQ %d set as the %dth irq on %s\n", hirq, hwirq,
		 domain->of_node->name);

	irq = vmm_host_irq_get(hirq);
	if (!irq) {
		return VMM_EFAIL;
	}

	/* Set type if specified and different than the current one */
	if (type != VMM_IRQ_TYPE_NONE &&
	    type != irq->state) {
		vmm_host_irq_set_type(hirq, type);
	}

	return hirq;
}

unsigned int vmm_devtree_irq_parse_map(struct vmm_devtree_node *dev,
					int index)
{
	int hirq = 0;
	struct vmm_devtree_phandle_args oirq = { .np = NULL, .args_count = 0 };

	if (vmm_devtree_irq_parse_one(dev, index, &oirq)) {
		return 0;
	}

	if (oirq.args_count) {
		hirq = vmm_devtree_irq_create_mapping(&oirq);
	}

	if (oirq.np) {
		vmm_devtree_dref_node(oirq.np);
	}

	return (hirq < 0) ? 0 : hirq;
}
