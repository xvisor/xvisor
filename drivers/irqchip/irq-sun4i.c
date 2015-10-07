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
 *
 * The source has been largely adapted from Linux
 * drivers/irqchip/irq-sun4i.c
 *
 * The original code is licensed under the GPL.
 *
 * Allwinner A1X SoCs IRQ chip driver.
 *
 * Copyright (C) 2012 Maxime Ripard
 *
 * Maxime Ripard <maxime.ripard@free-electrons.com>
 *
 * Based on code from
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Benn Huang <benn@allwinnertech.com>
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <vmm_types.h>
#include <vmm_error.h>
#include <vmm_limits.h>
#include <vmm_devtree.h>
#include <vmm_host_io.h>
#include <vmm_host_irq.h>
#include <vmm_host_aspace.h>

/* Max number of irqs */
#define AW_NR_BANKS			3
#define AW_IRQS_PER_BANK		32
#define AW_NR_IRQS			(AW_NR_BANKS * AW_IRQS_PER_BANK)

/* Interrupt controller registers */
#define AW_INT_VECTOR_REG		(0x00)
#define AW_INT_BASE_ADR_REG		(0x04)
#define AW_INT_PROTECTION_REG		(0x08)
#define AW_INT_NMI_CTRL_REG		(0x0c)
#define AW_INT_IRQ_PENDING_REG0		(0x10)
#define AW_INT_IRQ_PENDING_REG1		(0x14)
#define AW_INT_IRQ_PENDING_REG2		(0x18)
#define AW_INT_FIQ_PENDING_REG0		(0x20)
#define AW_INT_FIQ_PENDING_REG1		(0x24)
#define AW_INT_FIQ_PENDING_REG2		(0x28)
#define AW_INT_SELECT_REG0		(0x30)
#define AW_INT_SELECT_REG1		(0x34)
#define AW_INT_SELECT_REG2		(0x38)
#define AW_INT_ENABLE_REG0		(0x40)
#define AW_INT_ENABLE_REG1		(0x44)
#define AW_INT_ENABLE_REG2		(0x48)
#define AW_INT_MASK_REG0		(0x50)
#define AW_INT_MASK_REG1		(0x54)
#define AW_INT_MASK_REG2		(0x58)
#define AW_INT_RESP_REG0		(0x60)
#define AW_INT_RESP_REG1		(0x64)
#define AW_INT_RESP_REG2		(0x68)
#define AW_INT_FASTFORCE_REG0		(0x70)
#define AW_INT_FASTFORCE_REG1		(0x74)
#define AW_INT_FASTFORCE_REG2		(0x78)
#define AW_INT_SRCPRIO_REG0		(0x80)
#define AW_INT_SRCPRIO_REG1		(0x84)
#define AW_INT_SRCPRIO_REG2		(0x88)
#define AW_INT_SRCPRIO_REG3		(0x8c)
#define AW_INT_SRCPRIO_REG4		(0x90)

/* Non-maskable interrupt number */
#define AW_INT_IRQNO_ENMI                 0

struct aw_vic {
	virtual_addr_t base;
	void *protection;
	void *nmi_ctrl;
	void *irq_pend0;
	void *irq_pend1;
	void *irq_pend2;
	void *fiq_pend0;
	void *fiq_pend1;
	void *fiq_pend2;
	void *enable0;
	void *enable1;
	void *enable2;
	void *mask0;
	void *mask1;
	void *mask2;
};

static struct aw_vic awvic;

static void aw_irq_ack(struct vmm_host_irq *d)
{
	unsigned int mask = 1 << (d->hwirq & 0x1f);

	if (d->hwirq < 32) {
		vmm_writel(vmm_readl(awvic.enable0) & ~mask, awvic.enable0);
		vmm_writel(vmm_readl(awvic.mask0) | mask, awvic.mask0);
		vmm_writel(vmm_readl(awvic.irq_pend0) | mask, awvic.irq_pend0);
	} else if (d->hwirq < 64) {
		vmm_writel(vmm_readl(awvic.enable1) & ~mask, awvic.enable1);
		vmm_writel(vmm_readl(awvic.mask1) | mask, awvic.mask1);
		vmm_writel(vmm_readl(awvic.irq_pend1) | mask, awvic.irq_pend1);
	} else if (d->hwirq < 96) {
		vmm_writel(vmm_readl(awvic.enable2) & ~mask, awvic.enable2);
		vmm_writel(vmm_readl(awvic.mask2) | mask, awvic.mask2);
		vmm_writel(vmm_readl(awvic.irq_pend2) | mask, awvic.irq_pend2);
	}
}

/* Mask an IRQ line, which means disabling the IRQ line */
static void aw_irq_mask(struct vmm_host_irq *d)
{
	unsigned int mask = 1 << (d->hwirq & 0x1f);

	if (d->hwirq < 32) {
		vmm_writel(vmm_readl(awvic.enable0) & ~mask, awvic.enable0);
		vmm_writel(vmm_readl(awvic.mask0) | mask, awvic.mask0);
	} else if (d->hwirq < 64) {
		vmm_writel(vmm_readl(awvic.enable1) & ~mask, awvic.enable1);
		vmm_writel(vmm_readl(awvic.mask1) | mask, awvic.mask1);
	} else if (d->hwirq < 96) {
		vmm_writel(vmm_readl(awvic.enable2) & ~mask, awvic.enable2);
		vmm_writel(vmm_readl(awvic.mask2) | mask, awvic.mask2);
	}
}

static void aw_irq_unmask(struct vmm_host_irq *d)
{
	unsigned int mask = 1 << (d->hwirq & 0x1f);

	if (d->hwirq < 32) {
		vmm_writel(vmm_readl(awvic.enable0) | mask, awvic.enable0);
		vmm_writel(vmm_readl(awvic.mask0) & ~mask, awvic.mask0);
		/* must clear pending bit when NMI is enabled */
		if (d->hwirq == AW_INT_IRQNO_ENMI)
			vmm_writel(mask, awvic.irq_pend0);
	} else if (d->hwirq < 64) {
		vmm_writel(vmm_readl(awvic.enable1) | mask, awvic.enable1);
		vmm_writel(vmm_readl(awvic.mask1) & ~mask, awvic.mask1);
	} else if (d->hwirq < 96) {
		vmm_writel(vmm_readl(awvic.enable2) | mask, awvic.enable2);
		vmm_writel(vmm_readl(awvic.mask2) & ~mask, awvic.mask2);
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
		if ((s = vmm_readl(awvic.irq_pend0 + (i*4)))) {
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
	void *base;

	/* Map registers */
	rc = vmm_devtree_request_regmap(node, &awvic.base, 0,
					"Allwinner INTC");
	if (rc) {
		return rc;
	}
	base = (void *)awvic.base;

	/* Precompute register addresses */
	awvic.protection = base + AW_INT_PROTECTION_REG;
	awvic.nmi_ctrl = base + AW_INT_NMI_CTRL_REG;
	awvic.irq_pend0 = base + AW_INT_IRQ_PENDING_REG0;
	awvic.irq_pend1 = base + AW_INT_IRQ_PENDING_REG1;
	awvic.irq_pend2 = base + AW_INT_IRQ_PENDING_REG2;
	awvic.fiq_pend0 = base + AW_INT_FIQ_PENDING_REG0;
	awvic.fiq_pend1 = base + AW_INT_FIQ_PENDING_REG1;
	awvic.fiq_pend2 = base + AW_INT_FIQ_PENDING_REG2;
	awvic.enable0 = base + AW_INT_ENABLE_REG0;
	awvic.enable1 = base + AW_INT_ENABLE_REG1;
	awvic.enable2 = base + AW_INT_ENABLE_REG2;
	awvic.mask0 = base + AW_INT_MASK_REG0;
	awvic.mask1 = base + AW_INT_MASK_REG1;
	awvic.mask2 = base + AW_INT_MASK_REG2;

	/* Disable & clear all interrupts */
	vmm_writel(0, awvic.enable0);
	vmm_writel(0, awvic.enable1);
	vmm_writel(0, awvic.enable2);
	vmm_writel(0xffffffff, awvic.mask0);
	vmm_writel(0xffffffff, awvic.mask1);
	vmm_writel(0xffffffff, awvic.mask2);

	/* Clear all pending interrupts */
	vmm_writel(0xffffffff, awvic.irq_pend0);
	vmm_writel(0xffffffff, awvic.irq_pend1);
	vmm_writel(0xffffffff, awvic.irq_pend2);
	vmm_writel(0xffffffff, awvic.fiq_pend0);
	vmm_writel(0xffffffff, awvic.fiq_pend1);
	vmm_writel(0xffffffff, awvic.fiq_pend2);

	/* Enable protection mode */
	vmm_writel(0x01, awvic.protection);

	/* Config the external interrupt source type */
	vmm_writel(0x00, awvic.nmi_ctrl);

	/* Setup irqdomain and irqchip */
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
