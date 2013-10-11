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
 * @file bcm2835_intc.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief BCM2835 intc implementation
 */

#include <vmm_error.h>
#include <vmm_compiler.h>
#include <vmm_stdio.h>
#include <vmm_host_io.h>
#include <vmm_host_irq.h>
#include <vmm_devtree.h>
#include <libs/bitops.h>
#include <arch_host_irq.h>

#include <bcm2835_intc.h>

/* Put the bank and irq (32 bits) into the hwirq */
#define MAKE_HWIRQ(b, n)	((b << 5) | (n))
#define HWIRQ_BANK(i)		(i >> 5)
#define HWIRQ_BIT(i)		(1UL << (i & 0x1f))

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

static int reg_pending[] __initconst = { 0x00, 0x04, 0x08 };
static int reg_enable[] __initconst = { 0x18, 0x10, 0x14 };
static int reg_disable[] __initconst = { 0x24, 0x1c, 0x20 };
static int bank_irqs[] __initconst = { 8, 32, 32 };

static const int shortcuts[] = {
	7, 9, 10, 18, 19,		/* Bank 1 */
	21, 22, 23, 24, 25, 30		/* Bank 2 */
};

struct armctrl_ic {
	virtual_addr_t base_va;
	void *base;
	void *pending[NR_BANKS];
	void *enable[NR_BANKS];
	void *disable[NR_BANKS];
	int irqs[NR_BANKS];
};

static struct armctrl_ic intc __read_mostly;

static void bcm2835_intc_irq_mask(struct vmm_host_irq *irqd)
{
	vmm_writel(HWIRQ_BIT(irqd->num), intc.disable[HWIRQ_BANK(irqd->num)]);
}

static void bcm2835_intc_irq_unmask(struct vmm_host_irq *irqd)
{
	vmm_writel(HWIRQ_BIT(irqd->num), intc.enable[HWIRQ_BANK(irqd->num)]);
}

static struct vmm_host_irq_chip bcm2835_intc_chip = {
	.name       = "INTC",
	.irq_mask   = bcm2835_intc_irq_mask,
	.irq_unmask = bcm2835_intc_irq_unmask,
};

static u32 bcm2835_intc_active_irq(u32 cpu_irq_no)
{
	register u32 stat, irq;

	if ((stat = vmm_readl(intc.pending[0]))) {
		if (stat & BANK0_HWIRQ_MASK) {
			stat = stat & BANK0_HWIRQ_MASK;
			irq = MAKE_HWIRQ(0, ffs(stat) - 1);
		} else if (stat & SHORTCUT1_MASK) {
			stat = (stat & SHORTCUT1_MASK) >> SHORTCUT_SHIFT;
			irq = MAKE_HWIRQ(1, shortcuts[ffs(stat) - 1]);
		} else if (stat & SHORTCUT2_MASK) {
			stat = (stat & SHORTCUT2_MASK) >> SHORTCUT_SHIFT;
			irq = MAKE_HWIRQ(2, shortcuts[ffs(stat) - 1]);
		} else if (stat & BANK1_HWIRQ) {
			stat = vmm_readl(intc.pending[1]);
			irq = MAKE_HWIRQ(1, ffs(stat) - 1);
		} else if (stat & BANK2_HWIRQ) {
			stat = vmm_readl(intc.pending[2]);
			irq = MAKE_HWIRQ(2, ffs(stat) - 1);
		} else {
			BUG();
		}
	}

	return irq;
}

int __init bcm2835_intc_init(void)
{
	int rc;
	u32 b, i = 0, irq;
	struct vmm_devtree_node *node;

	node = vmm_devtree_find_compatible(NULL, NULL, 
					   "brcm,bcm2835-armctrl-ic");
	if (!node) {
		return VMM_ENODEV;
	}

	rc = vmm_devtree_regmap(node, &intc.base_va, 0);
	if (rc) {
		return rc;
	}

	intc.base = (void *)intc.base_va;

	for (b = 0; b < NR_BANKS; b++) {
		intc.pending[b] = intc.base + reg_pending[b];
		intc.enable[b] = intc.base + reg_enable[b];
		intc.disable[b] = intc.base + reg_disable[b];
		intc.irqs[b] = bank_irqs[b];

		for (i = 0; i < intc.irqs[b]; i++) {
			irq = MAKE_HWIRQ(b, i);
			vmm_host_irq_set_chip(irq, &bcm2835_intc_chip);
			vmm_host_irq_set_handler(irq, vmm_handle_level_irq);
		}
	}

	vmm_host_irq_set_active_callback(bcm2835_intc_active_irq);

	return 0;
}

