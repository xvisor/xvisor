/**
 * Copyright (c) 2012 Anup Patel.
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
 * @file arch_host_irq.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief board specific host irq functions
 */
#ifndef _ARCH_HOST_IRQ_H__
#define _ARCH_HOST_IRQ_H__

#include <vmm_types.h>
#include <sun4i_plat.h>
#include <sunxi/intc.h>

#define ARCH_HOST_IRQ_COUNT			AW_NR_IRQS

/* Get current active host irq */
static inline u32 arch_host_irq_active(u32 cpu_irq_no)
{
	return aw_intc_irq_active(0);
}

/* Initialize board specifig host irq hardware (i.e PIC) */
static inline int arch_host_irq_init(void)
{
	struct vmm_devtree_node *node;

	node = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPARATOR_STRING
				   VMM_DEVTREE_HOSTINFO_NODE_NAME
				   VMM_DEVTREE_PATH_SEPARATOR_STRING "soc"
				   VMM_DEVTREE_PATH_SEPARATOR_STRING "intc");
	if (!node) {
		return VMM_ENODEV;
	}

	return aw_intc_devtree_init(node);
}

#endif
