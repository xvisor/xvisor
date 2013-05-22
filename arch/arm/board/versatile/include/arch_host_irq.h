/**
 * Copyright (c) 2012 Jean-Christophe Dubois.
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
 * @author Jean-Chrsitophe Dubois (jcd@tribudubois.net)
 * @brief board specific host irq functions
 */
#ifndef _ARCH_HOST_IRQ_H__
#define _ARCH_HOST_IRQ_H__

#include <vmm_types.h>
#include <vmm_devtree.h>
#include <vmm_host_aspace.h>
#include <vmm_host_io.h>
#include <vic_config.h>
#include <vic.h>

#define ARCH_HOST_IRQ_COUNT		NR_IRQS_VERSATILE

/* This mask is used to route interrupts 21 to 31 on VIC */
#define PIC_MASK			0xFFD00000

/* Get current active host irq */
static inline u32 arch_host_irq_active(u32 cpu_irq_no)
{
	return vic_active_irq(0);
}

/* Initialize board specifig host irq hardware (i.e PIC) */
static inline int arch_host_irq_init(void)
{
	int rc;
	virtual_addr_t vic_base;
	virtual_addr_t sic_base;
	struct vmm_devtree_node *vnode, *snode;

	vnode = vmm_devtree_find_compatible(NULL, NULL, "arm,versatile-vic");
	if (!vnode) {
		return VMM_ENODEV;
	}

	rc = vmm_devtree_regmap(vnode, &vic_base, 0);
	if (rc) {
		return rc;
	}

	snode = vmm_devtree_find_compatible(NULL, NULL, "arm,versatile-sic");
	if (!snode) {
		return VMM_ENODEV;
	}

	rc = vmm_devtree_regmap(snode, &sic_base, 0);
	if (rc) {
		return rc;
	}

	rc = vic_init(0, 0, vic_base);
	if (rc) {
		return rc;
	}

	vmm_writel(~0, (volatile void *)sic_base + SIC_IRQ_ENABLE_CLEAR);

	/*
	 * Using Linux Method: Interrupts on secondary controller from 0 to 8
	 * are routed to source 31 on PIC.
	 * Interrupts from 21 to 31 are routed directly to the VIC on
	 * the corresponding number on primary controller. This is controlled
	 * by setting PIC_ENABLEx.
	 */
	vmm_writel(PIC_MASK, (volatile void *)sic_base + SIC_INT_PIC_ENABLE);

	return VMM_OK;
}

#endif
