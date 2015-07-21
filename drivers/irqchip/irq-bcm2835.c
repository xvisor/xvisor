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
 * @brief BCM2835 intc driver
 */

#include <vmm_error.h>
#include <vmm_limits.h>
#include <vmm_compiler.h>
#include <vmm_stdio.h>
#include <vmm_host_io.h>
#include <vmm_host_irq.h>
#include <vmm_devtree.h>
#include <libs/bitops.h>

/** Maximum number of IRQs in bcm2835 intc */
#define BCM2835_INTC_MAX_IRQ		96

#define ARM_IRQ0_BASE			0
#define INTERRUPT_ARM_TIMER		(ARM_IRQ0_BASE + 0)
#define INTERRUPT_ARM_MAILBOX		(ARM_IRQ0_BASE + 1)
#define INTERRUPT_ARM_DOORBELL_0 	(ARM_IRQ0_BASE + 2)
#define INTERRUPT_ARM_DOORBELL_1 	(ARM_IRQ0_BASE + 3)
#define INTERRUPT_VPU0_HALTED		(ARM_IRQ0_BASE + 4)
#define INTERRUPT_VPU1_HALTED		(ARM_IRQ0_BASE + 5)
#define INTERRUPT_ILLEGAL_TYPE0		(ARM_IRQ0_BASE + 6)
#define INTERRUPT_ILLEGAL_TYPE1		(ARM_IRQ0_BASE + 7)
#define INTERRUPT_PENDING1		(ARM_IRQ0_BASE + 8)
#define INTERRUPT_PENDING2		(ARM_IRQ0_BASE + 9)
#define INTERRUPT_JPEG			(ARM_IRQ0_BASE + 10)
#define INTERRUPT_USB			(ARM_IRQ0_BASE + 11)
#define INTERRUPT_3D			(ARM_IRQ0_BASE + 12)
#define INTERRUPT_DMA2			(ARM_IRQ0_BASE + 13)
#define INTERRUPT_DMA3			(ARM_IRQ0_BASE + 14)
#define INTERRUPT_I2C 			(ARM_IRQ0_BASE + 15)
#define INTERRUPT_SPI 			(ARM_IRQ0_BASE + 16)
#define INTERRUPT_I2SPCM		(ARM_IRQ0_BASE + 17)
#define INTERRUPT_SDIO			(ARM_IRQ0_BASE + 18)
#define INTERRUPT_UART			(ARM_IRQ0_BASE + 19)
#define INTERRUPT_ARASANSDIO		(ARM_IRQ0_BASE + 20)

#define ARM_IRQ1_BASE			32
#define INTERRUPT_TIMER0		(ARM_IRQ1_BASE + 0)
#define INTERRUPT_TIMER1		(ARM_IRQ1_BASE + 1)
#define INTERRUPT_TIMER2		(ARM_IRQ1_BASE + 2)
#define INTERRUPT_TIMER3		(ARM_IRQ1_BASE + 3)
#define INTERRUPT_CODEC0		(ARM_IRQ1_BASE + 4)
#define INTERRUPT_CODEC1		(ARM_IRQ1_BASE + 5)
#define INTERRUPT_CODEC2		(ARM_IRQ1_BASE + 6)
#define INTERRUPT_VC_JPEG		(ARM_IRQ1_BASE + 7)
#define INTERRUPT_ISP			(ARM_IRQ1_BASE + 8)
#define INTERRUPT_VC_USB		(ARM_IRQ1_BASE + 9)
#define INTERRUPT_VC_3D			(ARM_IRQ1_BASE + 10)
#define INTERRUPT_TRANSPOSER		(ARM_IRQ1_BASE + 11)
#define INTERRUPT_MULTICORESYNC0 	(ARM_IRQ1_BASE + 12)
#define INTERRUPT_MULTICORESYNC1 	(ARM_IRQ1_BASE + 13)
#define INTERRUPT_MULTICORESYNC2 	(ARM_IRQ1_BASE + 14)
#define INTERRUPT_MULTICORESYNC3 	(ARM_IRQ1_BASE + 15)
#define INTERRUPT_DMA0			(ARM_IRQ1_BASE + 16)
#define INTERRUPT_DMA1			(ARM_IRQ1_BASE + 17)
#define INTERRUPT_VC_DMA2		(ARM_IRQ1_BASE + 18)
#define INTERRUPT_VC_DMA3		(ARM_IRQ1_BASE + 19)
#define INTERRUPT_DMA4			(ARM_IRQ1_BASE + 20)
#define INTERRUPT_DMA5			(ARM_IRQ1_BASE + 21)
#define INTERRUPT_DMA6			(ARM_IRQ1_BASE + 22)
#define INTERRUPT_DMA7			(ARM_IRQ1_BASE + 23)
#define INTERRUPT_DMA8			(ARM_IRQ1_BASE + 24)
#define INTERRUPT_DMA9			(ARM_IRQ1_BASE + 25)
#define INTERRUPT_DMA10			(ARM_IRQ1_BASE + 26)
#define INTERRUPT_DMA11			(ARM_IRQ1_BASE + 27)
#define INTERRUPT_DMA12			(ARM_IRQ1_BASE + 28)
#define INTERRUPT_AUX			(ARM_IRQ1_BASE + 29)
#define INTERRUPT_ARM			(ARM_IRQ1_BASE + 30)
#define INTERRUPT_VPUDMA		(ARM_IRQ1_BASE + 31)

#define ARM_IRQ2_BASE			64
#define INTERRUPT_HOSTPORT		(ARM_IRQ2_BASE + 0)
#define INTERRUPT_VIDEOSCALER		(ARM_IRQ2_BASE + 1)
#define INTERRUPT_CCP2TX		(ARM_IRQ2_BASE + 2)
#define INTERRUPT_SDC			(ARM_IRQ2_BASE + 3)
#define INTERRUPT_DSI0			(ARM_IRQ2_BASE + 4)
#define INTERRUPT_AVE			(ARM_IRQ2_BASE + 5)
#define INTERRUPT_CAM0			(ARM_IRQ2_BASE + 6)
#define INTERRUPT_CAM1			(ARM_IRQ2_BASE + 7)
#define INTERRUPT_HDMI0			(ARM_IRQ2_BASE + 8)
#define INTERRUPT_HDMI1			(ARM_IRQ2_BASE + 9)
#define INTERRUPT_PIXELVALVE1		(ARM_IRQ2_BASE + 10)
#define INTERRUPT_I2CSPISLV		(ARM_IRQ2_BASE + 11)
#define INTERRUPT_DSI1			(ARM_IRQ2_BASE + 12)
#define INTERRUPT_PWA0			(ARM_IRQ2_BASE + 13)
#define INTERRUPT_PWA1			(ARM_IRQ2_BASE + 14)
#define INTERRUPT_CPR			(ARM_IRQ2_BASE + 15)
#define INTERRUPT_SMI			(ARM_IRQ2_BASE + 16)
#define INTERRUPT_GPIO0			(ARM_IRQ2_BASE + 17)
#define INTERRUPT_GPIO1			(ARM_IRQ2_BASE + 18)
#define INTERRUPT_GPIO2			(ARM_IRQ2_BASE + 19)
#define INTERRUPT_GPIO3			(ARM_IRQ2_BASE + 20)
#define INTERRUPT_VC_I2C		(ARM_IRQ2_BASE + 21)
#define INTERRUPT_VC_SPI		(ARM_IRQ2_BASE + 22)
#define INTERRUPT_VC_I2SPCM		(ARM_IRQ2_BASE + 23)
#define INTERRUPT_VC_SDIO		(ARM_IRQ2_BASE + 24)
#define INTERRUPT_VC_UART		(ARM_IRQ2_BASE + 25)
#define INTERRUPT_SLIMBUS		(ARM_IRQ2_BASE + 26)
#define INTERRUPT_VEC			(ARM_IRQ2_BASE + 27)
#define INTERRUPT_CPG			(ARM_IRQ2_BASE + 28)
#define INTERRUPT_RNG			(ARM_IRQ2_BASE + 29)
#define INTERRUPT_VC_ARASANSDIO		(ARM_IRQ2_BASE + 30)
#define INTERRUPT_AVSPMON		(ARM_IRQ2_BASE + 31)

/* Put the bank and irq (32 bits) into the hwirq */
#define MAKE_HWIRQ(s, b, n)	((((b) << 5) | (n)) + (s))
#define HWIRQ_BANK(s, i)	(((i) - (s)) >> 5)
#define HWIRQ_BIT(s, i)		(1UL << (((i) - (s)) & 0x1f))

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
	u32 parent_irq;
	u32 irq_start;
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
	vmm_writel(HWIRQ_BIT(intc.irq_start, irqd->num),
		   intc.disable[HWIRQ_BANK(intc.irq_start, irqd->num)]);
}

static void bcm2835_intc_irq_unmask(struct vmm_host_irq *irqd)
{
	vmm_writel(HWIRQ_BIT(intc.irq_start, irqd->num),
		   intc.enable[HWIRQ_BANK(intc.irq_start, irqd->num)]);
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
			irq = MAKE_HWIRQ(intc.irq_start, 0, ffs(stat) - 1);
		} else if (stat & SHORTCUT1_MASK) {
			stat = (stat & SHORTCUT1_MASK) >> SHORTCUT_SHIFT;
			irq = MAKE_HWIRQ(intc.irq_start, 1, shortcuts[ffs(stat) - 1]);
		} else if (stat & SHORTCUT2_MASK) {
			stat = (stat & SHORTCUT2_MASK) >> SHORTCUT_SHIFT;
			irq = MAKE_HWIRQ(intc.irq_start, 2, shortcuts[ffs(stat) - 1]);
		} else if (stat & BANK1_HWIRQ) {
			stat = vmm_readl(intc.pending[1]);
			irq = MAKE_HWIRQ(intc.irq_start, 1, ffs(stat) - 1);
		} else if (stat & BANK2_HWIRQ) {
			stat = vmm_readl(intc.pending[2]);
			irq = MAKE_HWIRQ(intc.irq_start, 2, ffs(stat) - 1);
		} else {
			BUG();
		}
	} else {
		irq = UINT_MAX;
	}

	return irq;
}

static vmm_irq_return_t bcm2836_intc_cascade_irq(int irq, void *dev)
{
	vmm_host_generic_irq_exec(bcm2835_intc_active_irq(0));

	return VMM_IRQ_HANDLED;
}

static int __init bcm2835_intc_init(struct vmm_devtree_node *node)
{
	int rc;
	u32 b, i = 0, irq;

	if (vmm_devtree_irq_get(node, &intc.parent_irq, 0)) {
		intc.parent_irq = UINT_MAX;
	}

	if (vmm_devtree_read_u32(node, "irq_start", &intc.irq_start)) {
		intc.irq_start = 0;
	}

	rc = vmm_devtree_request_regmap(node, &intc.base_va, 0,
					"BCM2835 INTC");
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
			irq = MAKE_HWIRQ(intc.irq_start, b, i);
			vmm_host_irq_set_chip(irq, &bcm2835_intc_chip);
			vmm_host_irq_set_handler(irq, vmm_handle_level_irq);
		}
	}

	if (intc.parent_irq != UINT_MAX) {
		if (vmm_host_irq_register(intc.parent_irq, "BCM2836 INTC",
					  bcm2836_intc_cascade_irq, &intc)) {
			BUG();
		}
	} else {
		vmm_host_irq_set_active_callback(bcm2835_intc_active_irq);
	}

	return 0;
}
VMM_HOST_IRQ_INIT_DECLARE(bcm2835intc,
			  "brcm,bcm2835-armctrl-ic",
			  bcm2835_intc_init);

