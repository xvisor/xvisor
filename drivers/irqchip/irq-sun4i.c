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
 * @file irq-sun4i.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Allwinner Sun4i interrupt controller
 */

#include <vmm_types.h>
#include <vmm_error.h>
#include <vmm_limits.h>
#include <vmm_devtree.h>
#include <vmm_host_io.h>
#include <vmm_host_irq.h>
#include <vmm_host_aspace.h>

static virtual_addr_t aw_vic_base;

/* Register read/write macros */
#define __p(off)		((void *)(aw_vic_base + (off)))
#define readl(off)		vmm_readl(__p(off))
#define writel(val, off)	vmm_writel((val), __p(off))

/* Max number of irqs */
#define AW_NR_IRQS			  96

/* Interrupt controller registers */
#define AW_INT_VECTOR_REG                 (0x00)
#define AW_INT_BASE_ADR_REG               (0x04)
#define AW_INT_PROTECTION_REG             (0x08)
#define AW_INT_NMI_CTRL_REG               (0x0c)
#define AW_INT_IRQ_PENDING_REG0           (0x10)
#define AW_INT_IRQ_PENDING_REG1           (0x14)
#define AW_INT_IRQ_PENDING_REG2           (0x18)
#define AW_INT_FIQ_PENDING_REG0           (0x20)
#define AW_INT_FIQ_PENDING_REG1           (0x24)
#define AW_INT_FIQ_PENDING_REG2           (0x28)
#define AW_INT_SELECT_REG0                (0x30)
#define AW_INT_SELECT_REG1                (0x34)
#define AW_INT_SELECT_REG2                (0x38)
#define AW_INT_ENABLE_REG0                (0x40)
#define AW_INT_ENABLE_REG1                (0x44)
#define AW_INT_ENABLE_REG2                (0x48)
#define AW_INT_MASK_REG0                  (0x50)
#define AW_INT_MASK_REG1                  (0x54)
#define AW_INT_MASK_REG2                  (0x58)
#define AW_INT_RESP_REG0                  (0x60)
#define AW_INT_RESP_REG1                  (0x64)
#define AW_INT_RESP_REG2                  (0x68)
#define AW_INT_FASTFORCE_REG0             (0x70)
#define AW_INT_FASTFORCE_REG1             (0x74)
#define AW_INT_FASTFORCE_REG2             (0x78)
#define AW_INT_SRCPRIO_REG0               (0x80)
#define AW_INT_SRCPRIO_REG1               (0x84)
#define AW_INT_SRCPRIO_REG2               (0x88)
#define AW_INT_SRCPRIO_REG3               (0x8c)
#define AW_INT_SRCPRIO_REG4               (0x90)

/* Non-maskable interrupt number */
#define AW_INT_IRQNO_ENMI                 0

static void aw_irq_ack(struct vmm_host_irq *irqd)
{
	unsigned int irq = irqd->num;

	if (irq < 32) {
		writel(readl(AW_INT_ENABLE_REG0) & ~(1<<irq), AW_INT_ENABLE_REG0);
		writel(readl(AW_INT_MASK_REG0) | (1 << irq), AW_INT_MASK_REG0);
		writel(readl(AW_INT_IRQ_PENDING_REG0) | (1<<irq), AW_INT_IRQ_PENDING_REG0);
	} else if(irq < 64) {
		irq -= 32;
		writel(readl(AW_INT_ENABLE_REG1) & ~(1<<irq), AW_INT_ENABLE_REG1);
		writel(readl(AW_INT_MASK_REG1) | (1 << irq), AW_INT_MASK_REG1);
		writel(readl(AW_INT_IRQ_PENDING_REG1) | (1<<irq), AW_INT_IRQ_PENDING_REG1);
	} else if(irq < 96) {
		irq -= 64;
		writel(readl(AW_INT_ENABLE_REG2) & ~(1<<irq), AW_INT_ENABLE_REG2);
		writel(readl(AW_INT_MASK_REG2) | (1 << irq), AW_INT_MASK_REG2);
		writel(readl(AW_INT_IRQ_PENDING_REG2) | (1<<irq), AW_INT_IRQ_PENDING_REG2);
	}
}

/* Mask an IRQ line, which means disabling the IRQ line */
static void aw_irq_mask(struct vmm_host_irq *irqd)
{
	unsigned int irq = irqd->num;

	if (irq < 32) {
		writel(readl(AW_INT_ENABLE_REG0) & ~(1<<irq), AW_INT_ENABLE_REG0);
		writel(readl(AW_INT_MASK_REG0) | (1 << irq), AW_INT_MASK_REG0);
	} else if(irq < 64) {
		irq -= 32;
		writel(readl(AW_INT_ENABLE_REG1) & ~(1<<irq), AW_INT_ENABLE_REG1);
		writel(readl(AW_INT_MASK_REG1) | (1 << irq), AW_INT_MASK_REG1);
	} else if(irq < 96) {
		irq -= 64;
		writel(readl(AW_INT_ENABLE_REG2) & ~(1<<irq), AW_INT_ENABLE_REG2);
		writel(readl(AW_INT_MASK_REG2) | (1 << irq), AW_INT_MASK_REG2);
	}
}

static void aw_irq_unmask(struct vmm_host_irq *irqd)
{
	unsigned int irq = irqd->num;

	if (irq < 32) {
		writel(readl(AW_INT_ENABLE_REG0) | (1<<irq), AW_INT_ENABLE_REG0);
		writel(readl(AW_INT_MASK_REG0) & ~(1 << irq), AW_INT_MASK_REG0);
		if(irq == AW_INT_IRQNO_ENMI) /* must clear pending bit when enabled */
			writel((1 << AW_INT_IRQNO_ENMI), AW_INT_IRQ_PENDING_REG0);
	} else if(irq < 64) {
		irq -= 32;
		writel(readl(AW_INT_ENABLE_REG1) | (1<<irq), AW_INT_ENABLE_REG1);
		writel(readl(AW_INT_MASK_REG1) & ~(1 << irq), AW_INT_MASK_REG1);
	} else if(irq < 96) {
		irq -= 64;
		writel(readl(AW_INT_ENABLE_REG2) | (1<<irq), AW_INT_ENABLE_REG2);
		writel(readl(AW_INT_MASK_REG2) & ~(1 << irq), AW_INT_MASK_REG2);
	}
}

static struct vmm_host_irq_chip aw_vic_chip = {
	.name       = "AW_INTC",
	.irq_ack    = aw_irq_ack,
	.irq_mask   = aw_irq_mask,
	.irq_unmask = aw_irq_unmask,
};

static u32 aw_intc_irq_active(u32 cpu_irq_no)
{
	register u32 i, s;

	/* Find pending irq */
	for (i = 0; i < 3; i++) {
		if ((s = readl(AW_INT_IRQ_PENDING_REG0 + (i*4)))) {
			i = i*32;
			while (!(s & 0xF)) {
				i += 4;
				s >>= 4;
			}
			while (!(s & 0x1)) {
				i += 1;
				s >>= 1;
			}
			return i;
		}
	}

	/* Did not find any pending irq 
	 * so return invalid irq number 
	 */
	return UINT_MAX;
}

static int __cpuinit aw_intc_devtree_init(struct vmm_devtree_node *node)
{
	int rc;
	u32 i = 0;

	rc = vmm_devtree_regmap(node, &aw_vic_base, 0);
	if (rc) {
		return rc;
	}

	/* Disable & clear all interrupts */
	writel(0, AW_INT_ENABLE_REG0);
	writel(0, AW_INT_ENABLE_REG1);
	writel(0, AW_INT_ENABLE_REG2);

	writel(0xffffffff, AW_INT_MASK_REG0);
	writel(0xffffffff, AW_INT_MASK_REG1);
	writel(0xffffffff, AW_INT_MASK_REG2);

	writel(0xffffffff, AW_INT_IRQ_PENDING_REG0);
	writel(0xffffffff, AW_INT_IRQ_PENDING_REG1);
	writel(0xffffffff, AW_INT_IRQ_PENDING_REG2);
	writel(0xffffffff, AW_INT_FIQ_PENDING_REG0);
	writel(0xffffffff, AW_INT_FIQ_PENDING_REG1);
	writel(0xffffffff, AW_INT_FIQ_PENDING_REG2);

	/*enable protection mode*/
	writel(0x01, AW_INT_PROTECTION_REG);
	/*config the external interrupt source type*/
	writel(0x00, AW_INT_NMI_CTRL_REG);

	for (i = 0; i < AW_NR_IRQS; i++) {
		vmm_host_irq_set_chip(i, &aw_vic_chip);
		vmm_host_irq_set_handler(i, vmm_handle_level_irq);
	}

	/* Set active irq callback */
	vmm_host_irq_set_active_callback(aw_intc_irq_active);

	return VMM_OK;
}

VMM_HOST_IRQ_INIT_DECLARE(sunxiintc,
			  "allwinner,sun4i-ic",
			  aw_intc_devtree_init);
