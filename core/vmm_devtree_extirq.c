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
 * @file vmm_devtree_extirq.c
 * @author Jimmy Durand Wesolowski <jimmy.durand-wesolowski@openwide.fr>
 * @brief Host extended IRQ device tree functions
 *
 * The source has been largely adapted from the Linux kernel v3.16:
 * drivers/of/irq.c and kernel/irq/irqdomain.c
 *
 * The original code is licensed under the GPL.
 */

#include <vmm_stdio.h>
#include <vmm_host_io.h>
#include <vmm_devtree.h>
#include <vmm_host_extirq.h>

#define pr_warn(msg...)			vmm_printf(msg)
#ifdef DEBUG
#define pr_debug(msg...)                vmm_printf(msg)
#else
#define pr_debug(msg...)
#endif

/**
 * of_irq_find_parent - Given a device node, find its interrupt parent node
 * @child: pointer to device node
 *
 * Returns a pointer to the interrupt parent node, or NULL if the interrupt
 * parent could not be determined.
 */
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

/**
 * vmm_devtree_extirq_parse_one - Resolve an interrupt for a device
 * @device: the device whose interrupt is to be resolved
 * @index: index of the interrupt to resolve
 * @out_irq: structure filled by this function
 *
 * This function resolves an interrupt for a node by walking the interrupt tree,
 * finding which interrupt controller node it is attached to, and returning the
 * interrupt specifier that can be used to retrieve an Xvisor IRQ number.
 */
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

/**
 * vmm_devtree_extirq_parse_map - Parse and map an interrupt into Xvisor space
 * @dev: Device node of the device whose interrupt is to be mapped
 * @index: Index of the interrupt to map
 *
 * This function is a wrapper that chains of_irq_parse_one() and
 * irq_create_of_mapping() to make things easier to callers
 */
unsigned int vmm_devtree_extirq_parse_map(struct vmm_devtree_node *dev,
					  int index)
{
	struct vmm_devtree_phandle_args oirq;

	if (vmm_devtree_extirq_parse_one(dev, index, &oirq))
		return 0;

	return vmm_devtree_extirq_create_mapping(&oirq);
}
