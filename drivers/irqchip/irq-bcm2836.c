/**
 * Copyright (c) 2015 Anup Patel.
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
 * @file irq-bcm2836.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief BCM2836 local intc driver
 *
 * The source has been largely adapted from Linux
 * drivers/irqchip/irq-bcm2836.c
 *
 * The original code is licensed under the GPL.
 *
 * Root interrupt controller for the BCM2836 (Raspberry Pi 2).
 *
 * Copyright 2015 Broadcom
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

/*
 * The low 2 bits identify the CPU that the GPU IRQ goes to, and the
 * next 2 bits identify the CPU that the GPU FIQ goes to.
 */
#define LOCAL_GPU_ROUTING		0x00c
/* When setting bits 0-3, enables PMU interrupts on that CPU. */
#define LOCAL_PM_ROUTING_SET		0x010
/* When setting bits 0-3, disables PMU interrupts on that CPU. */
#define LOCAL_PM_ROUTING_CLR		0x014
/*
 * The low 4 bits of this are the CPU's timer IRQ enables, and the
 * next 4 bits are the CPU's timer FIQ enables (which override the IRQ
 * bits).
 */
#define LOCAL_TIMER_INT_CONTROL0	0x040
/*
 * The low 4 bits of this are the CPU's per-mailbox IRQ enables, and
 * the next 4 bits are the CPU's per-mailbox FIQ enables (which
 * override the IRQ bits).
 */
#define LOCAL_MAILBOX_INT_CONTROL0	0x050
/*
 * The CPU's interrupt status register.  Bits are defined by the the
 * LOCAL_IRQ_* bits below.
 */
#define LOCAL_IRQ_PENDING0		0x060
/* Same status bits as above, but for FIQ. */
#define LOCAL_FIQ_PENDING0		0x070
/*
 * Mailbox0 write-to-set bits.  There are 16 mailboxes, 4 per CPU, and
 * these bits are organized by mailbox number and then CPU number.  We
 * use mailbox 0 for IPIs.  The mailbox's interrupt is raised while
 * any bit is set.
 */
#define LOCAL_MAILBOX0_SET0		0x080
/* Mailbox0 write-to-clear bits. */
#define LOCAL_MAILBOX0_CLR0		0x0c0

#define LOCAL_IRQ_CNTPSIRQ		0
#define LOCAL_IRQ_CNTPNSIRQ		1
#define LOCAL_IRQ_CNTHPIRQ		2
#define LOCAL_IRQ_CNTVIRQ		3
#define LOCAL_IRQ_MAILBOX0		4
#define LOCAL_IRQ_MAILBOX1		5
#define LOCAL_IRQ_MAILBOX2		6
#define LOCAL_IRQ_MAILBOX3		7
#define LOCAL_IRQ_GPU_FAST		8
#define LOCAL_IRQ_PMU_FAST		9
#define LAST_IRQ			LOCAL_IRQ_PMU_FAST
#define NR_IRQS				(LAST_IRQ + 1)

struct bcm2836_arm_irqchip_intc {
	struct vmm_host_irqdomain *domain;
	virtual_addr_t base_va;
	void *base;
};

static struct bcm2836_arm_irqchip_intc intc  __read_mostly;

static void bcm2836_arm_irqchip_mask_per_cpu_irq(unsigned int reg_offset,
						 unsigned int bit,
						 int cpu)
{
	void *reg = intc.base + reg_offset + 4 * cpu;

	vmm_writel(vmm_readl(reg) & ~BIT(bit), reg);
}

static void bcm2836_arm_irqchip_unmask_per_cpu_irq(unsigned int reg_offset,
						   unsigned int bit,
						   int cpu)
{
	void *reg = intc.base + reg_offset + 4 * cpu;

	vmm_writel(vmm_readl(reg) | BIT(bit), reg);
}

static void bcm2836_arm_irqchip_mask_timer_irq(struct vmm_host_irq *d)
{
	bcm2836_arm_irqchip_mask_per_cpu_irq(LOCAL_TIMER_INT_CONTROL0,
					     d->hwirq - LOCAL_IRQ_CNTPSIRQ,
					     vmm_smp_processor_id());
}

static void bcm2836_arm_irqchip_unmask_timer_irq(struct vmm_host_irq *d)
{
	bcm2836_arm_irqchip_unmask_per_cpu_irq(LOCAL_TIMER_INT_CONTROL0,
					       d->hwirq - LOCAL_IRQ_CNTPSIRQ,
					       vmm_smp_processor_id());
}

static struct vmm_host_irq_chip bcm2836_arm_irqchip_timer = {
	.name = "bcm2836-timer",
	.irq_mask = bcm2836_arm_irqchip_mask_timer_irq,
	.irq_unmask = bcm2836_arm_irqchip_unmask_timer_irq
};

static void bcm2836_arm_irqchip_mask_mbox_irq(struct vmm_host_irq *d)
{
	bcm2836_arm_irqchip_mask_per_cpu_irq(LOCAL_MAILBOX_INT_CONTROL0,
					     d->hwirq - LOCAL_IRQ_MAILBOX0,
					     vmm_smp_processor_id());
}

static void bcm2836_arm_irqchip_unmask_mbox_irq(struct vmm_host_irq *d)
{
	bcm2836_arm_irqchip_unmask_per_cpu_irq(LOCAL_MAILBOX_INT_CONTROL0,
					       d->hwirq - LOCAL_IRQ_MAILBOX0,
					       vmm_smp_processor_id());
}

#ifdef CONFIG_SMP
static void bcm2836_arm_irqchip_raise(struct vmm_host_irq *d,
				      const struct vmm_cpumask *mask)
{
	int cpu;
	void *mbox;

	for_each_cpu(cpu, mask)	{
		mbox = intc.base + LOCAL_MAILBOX0_SET0 + 0x10 * cpu;
		mbox += (d->hwirq - LOCAL_IRQ_MAILBOX0) * 0x4;
		vmm_writel(1 << 0, mbox);
	}
}
#endif

static struct vmm_host_irq_chip bcm2836_arm_irqchip_mbox = {
	.name = "bcm2836-mbox",
	.irq_mask = bcm2836_arm_irqchip_mask_mbox_irq,
	.irq_unmask = bcm2836_arm_irqchip_unmask_mbox_irq,
#ifdef CONFIG_SMP
	.irq_raise = bcm2836_arm_irqchip_raise
#endif
};

static void bcm2836_arm_irqchip_mask_gpu_irq(struct vmm_host_irq *d)
{
	/* Nothing to do here. */
}

static void bcm2836_arm_irqchip_unmask_gpu_irq(struct vmm_host_irq *d)
{
	/* Nothing to do here. */
}

static struct vmm_host_irq_chip bcm2836_arm_irqchip_gpu = {
	.name = "bcm2836-gpu",
	.irq_mask = bcm2836_arm_irqchip_mask_gpu_irq,
	.irq_unmask = bcm2836_arm_irqchip_unmask_gpu_irq
};

static void bcm2836_arm_irqchip_mask_pmu_irq(struct vmm_host_irq *d)
{
	vmm_writel(1 << vmm_smp_processor_id(), intc.base + LOCAL_PM_ROUTING_CLR);
}

static void bcm2836_arm_irqchip_unmask_pmu_irq(struct vmm_host_irq *d)
{
	vmm_writel(1 << vmm_smp_processor_id(), intc.base + LOCAL_PM_ROUTING_SET);
}

static struct vmm_host_irq_chip bcm2836_arm_irqchip_pmu = {
	.name = "bcm2836-pmu",
	.irq_mask = bcm2836_arm_irqchip_mask_pmu_irq,
	.irq_unmask = bcm2836_arm_irqchip_unmask_pmu_irq
};

static void bcm2836_arm_irqchip_register_irq(u32 hwirq, bool is_ipi,
					     struct vmm_host_irq_chip *chip)
{
	u32 irq = vmm_host_irqdomain_create_mapping(intc.domain, hwirq);

	vmm_host_irq_mark_per_cpu(irq);
	if (is_ipi) {
		vmm_host_irq_mark_ipi(irq);
	}
	vmm_host_irq_set_chip(irq, chip);
	vmm_host_irq_set_handler(irq, vmm_handle_percpu_irq);
}

static u32 bcm2836_intc_active_irq(u32 cpu_irq_no)
{
	u32 ret = UINT_MAX;
	u32 cpu, stat, hwirq;
	void *mbox;

	cpu = vmm_smp_processor_id();
	stat = vmm_readl(intc.base + LOCAL_IRQ_PENDING0 + 0x4 * cpu);

	if (stat) {
		hwirq = ffs(stat) - 1;
		if (LOCAL_IRQ_MAILBOX0 <= hwirq &&
		    hwirq <= LOCAL_IRQ_MAILBOX3) {
			mbox = intc.base + LOCAL_MAILBOX0_CLR0 + 0x10 * cpu;
			mbox += (hwirq - LOCAL_IRQ_MAILBOX0) * 0x4;
			vmm_writel(vmm_readl(mbox), mbox);
		}
		ret = vmm_host_irqdomain_find_mapping(intc.domain, hwirq);
	}

	return ret;
}

static struct vmm_host_irqdomain_ops bcm2836_intc_ops = {
	.xlate = vmm_host_irqdomain_xlate_onecell,
};

static int __cpuinit bcm2836_intc_init(struct vmm_devtree_node *node)
{
	int rc, i;
	u32 irq_start = 0;

	if (!vmm_smp_is_bootcpu()) {
		return VMM_OK;
	}

	if (vmm_devtree_read_u32(node, "irq_start", &irq_start)) {
		irq_start = 0;
	}

	intc.domain = vmm_host_irqdomain_add(node, (int)irq_start, NR_IRQS,
					     &bcm2836_intc_ops, NULL);
	if (!intc.domain) {
		return VMM_EFAIL;
	}

	rc = vmm_devtree_request_regmap(node, &intc.base_va, 0,
					"BCM2836 LOCAL INTC");
	if (rc) {
		vmm_host_irqdomain_remove(intc.domain);
		return rc;
	}
	intc.base = (void *)intc.base_va;

	/* Setup up per-CPU interrupts */
	bcm2836_arm_irqchip_register_irq(LOCAL_IRQ_CNTPSIRQ, FALSE,
					 &bcm2836_arm_irqchip_timer);
	bcm2836_arm_irqchip_register_irq(LOCAL_IRQ_CNTPNSIRQ, FALSE,
					 &bcm2836_arm_irqchip_timer);
	bcm2836_arm_irqchip_register_irq(LOCAL_IRQ_CNTHPIRQ, FALSE,
					 &bcm2836_arm_irqchip_timer);
	bcm2836_arm_irqchip_register_irq(LOCAL_IRQ_CNTVIRQ, FALSE,
					 &bcm2836_arm_irqchip_timer);
	/* Mailbox0 used for SMP spin loop so we don't use it for IPIs */
	bcm2836_arm_irqchip_register_irq(LOCAL_IRQ_MAILBOX0, FALSE,
					 &bcm2836_arm_irqchip_mbox);
	bcm2836_arm_irqchip_register_irq(LOCAL_IRQ_MAILBOX1, TRUE,
					 &bcm2836_arm_irqchip_mbox);
	bcm2836_arm_irqchip_register_irq(LOCAL_IRQ_MAILBOX2, TRUE,
					 &bcm2836_arm_irqchip_mbox);
	bcm2836_arm_irqchip_register_irq(LOCAL_IRQ_MAILBOX3, TRUE,
					 &bcm2836_arm_irqchip_mbox);
	bcm2836_arm_irqchip_register_irq(LOCAL_IRQ_GPU_FAST, FALSE,
					 &bcm2836_arm_irqchip_gpu);
	bcm2836_arm_irqchip_register_irq(LOCAL_IRQ_PMU_FAST, FALSE,
					 &bcm2836_arm_irqchip_pmu);

	/* Mask timer and mailbox interrupts */
	for (i = 0; i < 4; i++) {
		bcm2836_arm_irqchip_mask_per_cpu_irq(
			LOCAL_TIMER_INT_CONTROL0,
			LOCAL_IRQ_CNTPSIRQ - LOCAL_IRQ_CNTPSIRQ, i);
		bcm2836_arm_irqchip_mask_per_cpu_irq(
			LOCAL_TIMER_INT_CONTROL0,
			LOCAL_IRQ_CNTPNSIRQ - LOCAL_IRQ_CNTPSIRQ, i);
		bcm2836_arm_irqchip_mask_per_cpu_irq(
			LOCAL_TIMER_INT_CONTROL0,
			LOCAL_IRQ_CNTHPIRQ - LOCAL_IRQ_CNTPSIRQ, i);
		bcm2836_arm_irqchip_mask_per_cpu_irq(
			LOCAL_TIMER_INT_CONTROL0,
			LOCAL_IRQ_CNTVIRQ - LOCAL_IRQ_CNTPSIRQ, i);
		bcm2836_arm_irqchip_mask_per_cpu_irq(
			LOCAL_MAILBOX_INT_CONTROL0,
			(LOCAL_IRQ_MAILBOX0 - LOCAL_IRQ_MAILBOX0), i);
		bcm2836_arm_irqchip_mask_per_cpu_irq(
			LOCAL_MAILBOX_INT_CONTROL0,
			(LOCAL_IRQ_MAILBOX1 - LOCAL_IRQ_MAILBOX0), i);
		bcm2836_arm_irqchip_mask_per_cpu_irq(
			LOCAL_MAILBOX_INT_CONTROL0,
			(LOCAL_IRQ_MAILBOX2 - LOCAL_IRQ_MAILBOX0), i);
		bcm2836_arm_irqchip_mask_per_cpu_irq(
			LOCAL_MAILBOX_INT_CONTROL0,
			(LOCAL_IRQ_MAILBOX3 - LOCAL_IRQ_MAILBOX0), i);
	}

	vmm_host_irq_set_active_callback(bcm2836_intc_active_irq);

	return 0;
}

VMM_HOST_IRQ_INIT_DECLARE(bcm2836l1intc,
			  "brcm,bcm2836-l1-intc",
			  bcm2836_intc_init);
