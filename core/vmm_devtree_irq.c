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

#include <vmm_stdio.h>
#include <vmm_host_io.h>
#include <vmm_devtree.h>
#include <vmm_host_irq.h>
#include <vmm_host_extirq.h>

#define pr_warn(msg...)			vmm_printf(msg)
#ifdef DEBUG
#define pr_debug(msg...)                vmm_printf(msg)
#else
#define pr_debug(msg...)
#endif

int vmm_devtree_irq_get(struct vmm_devtree_node *node,
		        u32 *irq, int index)
{
	u32 alen;
	const char *aval;

	if (!node || !irq || index < 0) {
		return VMM_EFAIL;
	}

	aval = vmm_devtree_attrval(node, VMM_DEVTREE_INTERRUPTS_ATTR_NAME);
	if (!aval) {
		return VMM_ENOTAVAIL;
	}

	alen = vmm_devtree_attrlen(node, VMM_DEVTREE_INTERRUPTS_ATTR_NAME);
	if (alen < (index * sizeof(u32))) {
		return VMM_ENOTAVAIL;
	}

	aval += index * sizeof(u32);
	*irq  = vmm_be32_to_cpu(*((u32 *)aval));

	return VMM_OK;
}

u32 vmm_devtree_irq_count(struct vmm_devtree_node *node)
{
	u32 alen;

	if (!node) {
		return 0;
	}

	alen = vmm_devtree_attrlen(node, VMM_DEVTREE_INTERRUPTS_ATTR_NAME);

	return alen / sizeof(u32);
}

struct vmm_devtree_node *vmm_devtree_extirq_find_parent(
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

int vmm_devtree_extirq_parse_one(struct vmm_devtree_node *device,
				 int index,
				 struct vmm_devtree_phandle_args *out_irq)
{
	struct vmm_devtree_node *p = NULL;
	struct vmm_devtree_attr *attr = NULL;
	u32 *intspec = NULL;
	u32 intsize = 0;
	u32 intlen = 0;
	int res = VMM_EINVALID;
	int i;

	pr_debug("of_irq_parse_one: dev=%s, index=%d\n", device->name,
		 index);

	attr = vmm_devtree_getattr(device, "interrupts");
	/* FIXME: Linux interrupt-extended management not implemented yet */
	if (NULL == attr) {
		return VMM_EINVALID;
	}
	intlen = attr->len / sizeof (u32);
	intspec = attr->value;
	pr_debug(" intspec=%d intlen=%d\n", vmm_be32_to_cpu(*intspec), intlen);

	/* Look for the interrupt parent. */
	p = vmm_devtree_extirq_find_parent(device);
	if (p == NULL) {
		return VMM_EINVALID;
	}

	/* Get size of interrupt specifier */
	res = vmm_devtree_read_u32(p, "#interrupt-cells", &intsize);
	if (VMM_OK != res) {
		goto out;
	}

	pr_debug(" intsize=%d intlen=%d\n", intsize, intlen);

	/* Check index */
	if ((index + 1) * intsize > intlen)
		goto out;

	/* Copy intspec into irq structure */
	intspec += index * intsize;
	out_irq->np = p;
	out_irq->args_count = intsize;
	for (i = 0; i < intsize && i < VMM_MAX_PHANDLE_ARGS; i++) {
		out_irq->args[i] = vmm_be32_to_cpu(*intspec++);
	}

	/* FIXME interrupt-map translations checking not implemented yet */
	return VMM_OK;
out:
	vmm_devtree_dref_node(device);
	return res;
}

static int vmm_host_extirq_match_node(extirq_grp_t *group,
				      struct vmm_devtree_node *node)
{
	if (group->of_node == node) {
		return 1;
	}
	return 0;
}

extirq_grp_t *vmm_devtree_extirq_find_group(struct vmm_devtree_node *node)
{
	return vmm_host_extirq_group_match(node,
					   (void *)vmm_host_extirq_match_node);
}

unsigned int vmm_devtree_extirq_create_mapping(
	struct vmm_devtree_phandle_args *irq_data)
{
	extirq_grp_t *group = NULL;
	struct vmm_host_irq *irq = NULL;
	long unsigned int hwirq;
	unsigned int type = VMM_IRQ_TYPE_NONE;
	unsigned int virq;

	if (irq_data->np) {
		group = vmm_devtree_extirq_find_group(irq_data->np);
	} else {
		return irq_data->args[0];
	}

	if (!group) {
		pr_warn("no irq group found for %s !\n",
			irq_data->np->name);
		return 0;
	}
	pr_debug("Group %s found\n", group->of_node->name);

	/* If group has no translation, then we assume interrupt line */
	if (group->ops->xlate == NULL) {
		hwirq = irq_data->args[0];
	} else {
		if (group->ops->xlate(group, irq_data->np, irq_data->args,
				      irq_data->args_count, &hwirq, &type)) {
			return hwirq;
		}
	}

	/* Create mapping */
	virq = vmm_host_extirq_create_mapping(group, hwirq);
	pr_debug("Extended IRQ %d set as the %dth irq on %s\n", virq, hwirq,
		 group->of_node->name);
	if (!virq) {
		return virq;
	}

	irq = vmm_host_irq_get(virq);
	if (!irq) {
		return VMM_EFAIL;
	}

	/* Set type if specified and different than the current one */
	if (type != VMM_IRQ_TYPE_NONE &&
	    type != irq->state) {
		vmm_host_irq_set_type(virq, type);
	}

	return virq;
}

unsigned int vmm_devtree_extirq_parse_map(struct vmm_devtree_node *dev,
					  int index)
{
	struct vmm_devtree_phandle_args oirq;

	if (vmm_devtree_extirq_parse_one(dev, index, &oirq))
		return 0;

	return vmm_devtree_extirq_create_mapping(&oirq);
}
