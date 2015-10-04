/**
 * Copyright (c) 2013 Anup Patel.
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
 * @file irq-bcm2835.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief BCM2835/BCM2836 SOC intc driver
 *
 * The source has been largely adapted from Linux
 * drivers/irqchip/irq-bcm2835.c
 *
 * The original code is licensed under the GPL.
 *
 * Copyright 2010 Broadcom
 * Copyright 2012 Simon Arlott, Chris Boot, Stephen Warren
 *
 * Quirk 1: Shortcut interrupts don't set the bank 1/2 register pending bits
 *
 * If an interrupt fires on bank 1 that isn't in the shortcuts list, bit 8
 * on bank 0 is set to signify that an interrupt in bank 1 has fired, and
 * to look in the bank 1 status register for more information.
 *
 * If an interrupt fires on bank 1 that _is_ in the shortcuts list, its
 * shortcut bit in bank 0 is set as well as its interrupt bit in the bank 1
 * status register, but bank 0 bit 8 is _not_ set.
 *
 * Quirk 2: You can't mask the register 1/2 pending interrupts
 *
 * In a proper cascaded interrupt controller, the interrupt lines with
 * cascaded interrupt controllers on them are just normal interrupt lines.
 * You can mask the interrupts and get on with things. With this controller
 * you can't do that.
 *
 * Quirk 3: The shortcut interrupts can't be (un)masked in bank 0
 *
 * Those interrupts that have shortcuts can only be masked/unmasked in
 * their respective banks' enable/disable registers. Doing so in the bank 0
 * enable/disable registers has no effect.
 *
 * The FIQ control register:
 *  Bits 0-6: IRQ (index in order of interrupts from banks 1, 2, then 0)
 *  Bit    7: Enable FIQ generation
 *  Bits  8+: Unused
 *
 * An interrupt must be disabled before configuring it for FIQ generation
 * otherwise both handlers will fire at the same time!
 */

#include <vmm_error.h>
#include <vmm_limits.h>
#include <vmm_compiler.h>
#include <vmm_stdio.h>
#include <vmm_smp.h>
#include <vmm_host_io.h>
#include <vmm_host_irq.h>
#include <vmm_host_irqdomain.h>
#include <vmm_devtree.h>
#include <libs/bitops.h>

/* Put the bank and irq (32 bits) into the hwirq */
#define MAKE_HWIRQ(b, n)	(((b) << 5) | (n))
#define HWIRQ_BANK(i)		((i) >> 5)
#define HWIRQ_BIT(i)		(1UL << ((i) & 0x1f))

#define NR_IRQS_BANK0		8
#define BANK0_HWIRQ_MASK	0xff
/* Shortcuts can't be disabled so any unknown new ones need to be masked */
#define SHORTCUT1_MASK		0x00007c00
#define SHORTCUT2_MASK		0x001f8000
#define SHORTCUT_SHIFT		10
#define BANK1_HWIRQ		BIT(8)
#define BANK2_HWIRQ		BIT(9)
#define BANK0_VALID_MASK	(BANK0_HWIRQ_MASK | BANK1_HWIRQ | BANK2_HWIRQ \
					| SHORTCUT1_MASK | SHORTCUT2_MASK)

#define REG_FIQ_CONTROL		0x0c

#define NR_BANKS		3
#define IRQS_PER_BANK		32
#define NR_IRQS			(NR_BANKS*IRQS_PER_BANK)

static int reg_pending[] __initconst = { 0x00, 0x04, 0x08 };
static int reg_enable[] __initconst = { 0x18, 0x10, 0x14 };
static int reg_disable[] __initconst = { 0x24, 0x1c, 0x20 };
static int bank_irqs[] __initconst = { 8, 32, 32 };

static const int shortcuts[] = {
	7, 9, 10, 18, 19,		/* Bank 1 */
	21, 22, 23, 24, 25, 30		/* Bank 2 */
};

struct armctrl_ic {
	u32 parent_irq;
	struct vmm_host_irqdomain *domain;
	virtual_addr_t base_va;
	void *base;
	void *pending[NR_BANKS];
	void *enable[NR_BANKS];
	void *disable[NR_BANKS];
	int irqs[NR_BANKS];
};

static struct armctrl_ic intc __read_mostly;

static void bcm283x_intc_irq_mask(struct vmm_host_irq *d)
{
	vmm_writel(HWIRQ_BIT(d->hwirq), intc.disable[HWIRQ_BANK(d->hwirq)]);
}

static void bcm283x_intc_irq_unmask(struct vmm_host_irq *d)
{
	vmm_writel(HWIRQ_BIT(d->hwirq), intc.enable[HWIRQ_BANK(d->hwirq)]);
}

static struct vmm_host_irq_chip bcm283x_intc_chip = {
	.name       = "INTC",
	.irq_mask   = bcm283x_intc_irq_mask,
	.irq_unmask = bcm283x_intc_irq_unmask,
};

static u32 bcm283x_intc_active_irq(u32 cpu_irq_no)
{
	register u32 stat, hwirq;

	if ((stat = vmm_readl(intc.pending[0]))) {
		if (stat & BANK0_HWIRQ_MASK) {
			stat = stat & BANK0_HWIRQ_MASK;
			hwirq = MAKE_HWIRQ(0, ffs(stat) - 1);
		} else if (stat & SHORTCUT1_MASK) {
			stat = (stat & SHORTCUT1_MASK) >> SHORTCUT_SHIFT;
			hwirq = MAKE_HWIRQ(1, shortcuts[ffs(stat) - 1]);
		} else if (stat & SHORTCUT2_MASK) {
			stat = (stat & SHORTCUT2_MASK) >> SHORTCUT_SHIFT;
			hwirq = MAKE_HWIRQ(2, shortcuts[ffs(stat) - 1]);
		} else if (stat & BANK1_HWIRQ) {
			stat = vmm_readl(intc.pending[1]);
			hwirq = MAKE_HWIRQ(1, ffs(stat) - 1);
		} else if (stat & BANK2_HWIRQ) {
			stat = vmm_readl(intc.pending[2]);
			hwirq = MAKE_HWIRQ(2, ffs(stat) - 1);
		} else {
			BUG();
		}
	} else {
		hwirq = UINT_MAX;
	}

	return vmm_host_irqdomain_find_mapping(intc.domain, hwirq);
}

static vmm_irq_return_t bcm2836_intc_cascade_irq(int irq, void *dev)
{
	vmm_host_generic_irq_exec(bcm283x_intc_active_irq(0));

	return VMM_IRQ_HANDLED;
}

static int bcm283x_intc_xlate(struct vmm_host_irqdomain *d,
			struct vmm_devtree_node *ctrlr,
			const u32 *intspec, unsigned int intsize,
			unsigned long *out_hwirq, unsigned int *out_type)
{
	if (WARN_ON(intsize != 2))
		return VMM_EINVALID;

	if (WARN_ON(intspec[0] >= NR_BANKS))
		return VMM_EINVALID;

	if (WARN_ON(intspec[1] >= IRQS_PER_BANK))
		return VMM_EINVALID;

	if (WARN_ON(intspec[0] == 0 && intspec[1] >= NR_IRQS_BANK0))
		return VMM_EINVALID;

	*out_hwirq = MAKE_HWIRQ(intspec[0], intspec[1]);
	*out_type = VMM_IRQ_TYPE_NONE;

	return 0;
}

static struct vmm_host_irqdomain_ops bcm283x_intc_ops = {
	.xlate = bcm283x_intc_xlate,
};

static int __cpuinit bcm283x_intc_init(struct vmm_devtree_node *node,
					bool is_bcm2836)
{
	int rc, irq;
	u32 b, i = 0, irq_start = 0;

	if (!vmm_smp_is_bootcpu()) {
		return VMM_OK;
	}

	intc.parent_irq = vmm_devtree_irq_parse_map(node, 0);
	if (!intc.parent_irq) {
		intc.parent_irq = UINT_MAX;
	}

	if (vmm_devtree_read_u32(node, "irq_start", &irq_start)) {
		irq_start = 0;
	}

	intc.domain = vmm_host_irqdomain_add(node, (int)irq_start, NR_IRQS,
					     &bcm283x_intc_ops, NULL);
	if (!intc.domain) {
		return VMM_EFAIL;
	}

	rc = vmm_devtree_request_regmap(node, &intc.base_va, 0,
					"BCM2835 INTC");
	if (rc) {
		vmm_host_irqdomain_remove(intc.domain);
		return rc;
	}
	intc.base = (void *)intc.base_va;

	for (b = 0; b < NR_BANKS; b++) {
		intc.pending[b] = intc.base + reg_pending[b];
		intc.enable[b] = intc.base + reg_enable[b];
		intc.disable[b] = intc.base + reg_disable[b];
		intc.irqs[b] = bank_irqs[b];

		for (i = 0; i < intc.irqs[b]; i++) {
			irq = vmm_host_irqdomain_create_mapping(intc.domain,
							   MAKE_HWIRQ(b, i));
			BUG_ON(irq < 0);
			vmm_host_irq_set_chip(irq, &bcm283x_intc_chip);
			vmm_host_irq_set_handler(irq, vmm_handle_level_irq);
		}
	}

	if (intc.parent_irq != UINT_MAX) {
		if (vmm_host_irq_register(intc.parent_irq, "BCM2836 INTC",
					  bcm2836_intc_cascade_irq, &intc)) {
			BUG();
		}
	} else {
		vmm_host_irq_set_active_callback(bcm283x_intc_active_irq);
	}

	return 0;
}

static int __cpuinit bcm2835_intc_init(struct vmm_devtree_node *node)
{
	return bcm283x_intc_init(node, FALSE);
}

VMM_HOST_IRQ_INIT_DECLARE(bcm2835intc,
			  "brcm,bcm2835-armctrl-ic",
			  bcm2835_intc_init);

static int __cpuinit bcm2836_intc_init(struct vmm_devtree_node *node)
{
	return bcm283x_intc_init(node, TRUE);
}

VMM_HOST_IRQ_INIT_DECLARE(bcm2836intc,
			  "brcm,bcm2836-armctrl-ic",
			  bcm2836_intc_init);

