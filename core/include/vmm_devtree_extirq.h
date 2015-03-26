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
 * @file vmm_devtree_extirq.h
 * @author Jimmy Durand Wesolowski <jimmy.durand-wesolowski@openwide.fr>
 * @brief Host extended IRQ device tree function header
 *
 * The source has been largely adapted from the Linux kernel v3.16:
 * drivers/of/irq.c and kernel/irq/irqdomain.c
 *
 * The original code is licensed under the GPL.
 */

/**
 * of_irq_find_parent - Given a device node, find its interrupt parent node
 * @child: pointer to device node
 *
 * Returns a pointer to the interrupt parent node, or NULL if the interrupt
 * parent could not be determined.
 */
struct vmm_devtree_node *vmm_devtree_extirq_find_parent(
	struct vmm_devtree_node *child);

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
				 struct vmm_devtree_phandle_args *out_irq);

/**
 * vmm_devtree_extirq_parse_map - Parse and map an interrupt into Xvisor space
 * @dev: Device node of the device whose interrupt is to be mapped
 * @index: Index of the interrupt to map
 *
 * This function is a wrapper that chains of_irq_parse_one() and
 * irq_create_of_mapping() to make things easier to callers
 */
unsigned int vmm_devtree_extirq_parse_map(struct vmm_devtree_node *dev,
					  int index);
