/**
 * Copyright (c) 2011 Anup Patel.
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
 * @file gic.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Generic Interrupt Controller Implementation
 */

#include <vmm_error.h>
#include <vmm_macros.h>
#include <vmm_smp.h>
#include <vmm_cpumask.h>
#include <vmm_stdio.h>
#include <vmm_host_io.h>
#include <vmm_host_irq.h>
#include <arch_barrier.h>
#include <gic.h>

struct gic_chip_data {
	u32 irq_offset;
	u32 gic_irqs;
	virtual_addr_t dist_base;
	virtual_addr_t cpu_base;
};

#ifndef GIC_MAX_NR
#define GIC_MAX_NR	2
#endif

static struct gic_chip_data gic_data[GIC_MAX_NR];

#define gic_write(val, addr)	vmm_writel((val), (void *)(addr))
#define gic_read(addr)		vmm_readl((void *)(addr))

static inline virtual_addr_t gic_dist_base(struct vmm_host_irq *irq)
{
	struct gic_chip_data *gic_data = vmm_host_irq_get_chip_data(irq);
	return gic_data->dist_base;
}

static inline virtual_addr_t gic_cpu_base(struct vmm_host_irq *irq)
{
	struct gic_chip_data *gic_data = vmm_host_irq_get_chip_data(irq);
	return gic_data->cpu_base;
}

static inline u32 gic_irq(struct vmm_host_irq *irq)
{
	struct gic_chip_data *gic_data = vmm_host_irq_get_chip_data(irq);
	return irq->num - gic_data->irq_offset;
}

static u32 gic_active_irq(u32 cpu_irq_nr)
{
	u32 ret;

	ret = gic_read(gic_data[0].cpu_base + GIC_CPU_INTACK) & 0x3FF;
	ret += gic_data[0].irq_offset;

	return ret;
}

static void gic_eoi_irq(struct vmm_host_irq *irq)
{
	gic_write(gic_irq(irq), gic_cpu_base(irq) + GIC_CPU_EOI);
}

static void gic_mask_irq(struct vmm_host_irq *irq)
{
	gic_write(1 << (irq->num % 32), gic_dist_base(irq) +
		  GIC_DIST_ENABLE_CLEAR + (gic_irq(irq) / 32) * 4);
}

static void gic_unmask_irq(struct vmm_host_irq *irq)
{
	gic_write(1 << (irq->num % 32), gic_dist_base(irq) +
		  GIC_DIST_ENABLE_SET + (gic_irq(irq) / 32) * 4);
}

void gic_enable_ppi(u32 irq)
{
	irq_flags_t flags;

	arch_cpu_irq_save(flags);
	gic_unmask_irq(vmm_host_irq_get(irq));
	arch_cpu_irq_restore(flags);
}

static int gic_set_type(struct vmm_host_irq *irq, u32 type)
{
	virtual_addr_t base = gic_dist_base(irq);
	u32 gicirq = gic_irq(irq);
	u32 enablemask = 1 << (gicirq % 32);
	u32 enableoff = (gicirq / 32) * 4;
	u32 confmask = 0x2 << ((gicirq % 16) * 2);
	u32 confoff = (gicirq / 16) * 4;
	bool enabled = FALSE;
	u32 val;

	/* Interrupt configuration for SGIs can't be changed */
	if (gicirq < 16) {
		return VMM_EINVALID;
	}

	if (type != VMM_IRQ_TYPE_LEVEL_HIGH && 
	    type != VMM_IRQ_TYPE_EDGE_RISING) {
		return VMM_EINVALID;
	}

	val = gic_read(base + GIC_DIST_CONFIG + confoff);
	if (type == VMM_IRQ_TYPE_LEVEL_HIGH) {
		val &= ~confmask;
	} else if (type == VMM_IRQ_TYPE_EDGE_RISING) {
		val |= confmask;
	}

	/*
	 * As recommended by the spec, disable the interrupt before changing
	 * the configuration
	 */
	if (gic_read(base + GIC_DIST_ENABLE_SET + enableoff) & enablemask) {
		gic_write(enablemask, base + GIC_DIST_ENABLE_CLEAR + enableoff);
		enabled = TRUE;
	}

	gic_write(val, base + GIC_DIST_CONFIG + confoff);

	if (enabled) {
		gic_write(enablemask, base + GIC_DIST_ENABLE_SET + enableoff);
	}

	return 0;
}

#ifdef CONFIG_SMP
void gic_raise_softirq(const struct vmm_cpumask *mask, u32 irq)
{
	unsigned long map = *vmm_cpumask_bits(mask);

	/*
	 * Ensure that stores to Normal memory are visible to the
	 * other CPUs before issuing the IPI.
	 */
	arch_wmb();

	/* This always happens on GIC0 */
	gic_write(map << 16 | irq, gic_data[0].dist_base + GIC_DIST_SOFTINT);
}

static int gic_set_affinity(struct vmm_host_irq *irq, 
			    const struct vmm_cpumask *mask_val,
			    bool force)
{
	virtual_addr_t reg;
	u32 shift = (irq->num % 4) * 8;
	u32 cpu = vmm_cpumask_first(mask_val);
	u32 val, mask, bit;

	if (cpu >= 8)
		return VMM_EINVALID;

	reg = gic_dist_base(irq) + GIC_DIST_TARGET + (gic_irq(irq) & ~3);
	mask = 0xff << shift;
	bit = 1 << (cpu + shift);

	val = gic_read(reg) & ~mask;
	gic_write(val | bit, reg);

	return 0;
}
#endif

static vmm_irq_return_t gic_handle_cascade_irq(int irq, void *dev)
{
	struct gic_chip_data *gic = dev;
	u32 cascade_irq, gic_irq;

	gic_irq = gic_read(gic->cpu_base + GIC_CPU_INTACK) & 0x3FF;

	if (gic_irq == 1023) {
		return VMM_IRQ_NONE;
	}

	cascade_irq = gic_irq + gic->irq_offset;
	if (likely((32 <= gic_irq) && (gic_irq <= 1020))) {
		vmm_host_generic_irq_exec(cascade_irq);
	}

	return VMM_IRQ_HANDLED;
}

static struct vmm_host_irq_chip gic_chip = {
	.name			= "GIC",
	.irq_mask		= gic_mask_irq,
	.irq_unmask		= gic_unmask_irq,
	.irq_eoi		= gic_eoi_irq,
	.irq_set_type		= gic_set_type,
#ifdef CONFIG_SMP
	.irq_set_affinity	= gic_set_affinity,
#endif
};

void __init gic_cascade_irq(u32 gic_nr, u32 irq)
{
	if (gic_nr >= GIC_MAX_NR)
		BUG();
	if (vmm_host_irq_register(irq, "GIC-CHILD", 
				  gic_handle_cascade_irq, 
				  &gic_data[gic_nr])) {
		BUG();
	}
}

static void __init gic_dist_init(struct gic_chip_data *gic, u32 irq_start)
{
	unsigned int max_irq, irq_limit, i;
	u32 cpumask = 1 << vmm_smp_processor_id();
	virtual_addr_t base = gic->dist_base;

	cpumask |= cpumask << 8;
	cpumask |= cpumask << 16;

	/* Disable IRQ distribution */
	gic_write(0, base + GIC_DIST_CTRL);

	/*
	 * Find out how many interrupts are supported.
	 */
	max_irq = gic_read(base + GIC_DIST_CTR) & 0x1f;
	max_irq = (max_irq + 1) * 32;

	/*
	 * The GIC only supports up to 1020 interrupt sources.
	 * Limit this to either the architected maximum, or the
	 * platform maximum.
	 */
	if (max_irq > 1020) {
		max_irq = 1020;
	}

	/*
	 * Set all global interrupts to be level triggered, active low.
	 */
	for (i = 32; i < max_irq; i += 16) {
		gic_write(0, base + GIC_DIST_CONFIG + i * 4 / 16);
	}

	/*
	 * Set all global interrupts to this CPU only.
	 */
	for (i = 32; i < max_irq; i += 4) {
		gic_write(cpumask, base + GIC_DIST_TARGET + i * 4 / 4);
	}

	/*
	 * Set priority on all interrupts.
	 */
	for (i = 0; i < max_irq; i += 4) {
		gic_write(0xa0a0a0a0, base + GIC_DIST_PRI + i * 4 / 4);
	}

	/*
	 * Disable all interrupts.
	 */
	for (i = 0; i < max_irq; i += 32) {
		gic_write(0xffffffff,
			  base + GIC_DIST_ENABLE_CLEAR + i * 4 / 32);
	}

	/*
	 * Limit number of interrupts registered to the platform maximum
	 */
	irq_limit = gic->irq_offset + max_irq;
	if (WARN_ON(irq_limit > CONFIG_HOST_IRQ_COUNT)) {
		irq_limit = CONFIG_HOST_IRQ_COUNT;
	}

	/*
	 * Setup the Host IRQ subsystem.
	 * Note: We handle all interrupts including SGIs and PPIs via C code.
	 * The Linux kernel handles pheripheral interrupts via C code and 
	 * SGI/PPI via assembly code.
	 */
	for (i = 0; i < irq_limit; i++) {
		vmm_host_irq_set_chip(i, &gic_chip);
		vmm_host_irq_set_chip_data(i, gic);
		vmm_host_irq_set_handler(i, vmm_handle_fast_eoi);
	}

	/* Enable IRQ distribution */
	gic_write(1, base + GIC_DIST_CTRL);
}

static void __cpuinit gic_cpu_init(struct gic_chip_data *gic)
{
	int i;

	/*
	 * Deal with the banked PPI and SGI interrupts - disable all
	 * PPI interrupts, ensure all SGI interrupts are enabled.
	 */
	gic_write(0xffff0000, gic->dist_base + GIC_DIST_ENABLE_CLEAR);
	gic_write(0x0000ffff, gic->dist_base + GIC_DIST_ENABLE_SET);

	/*
	 * Set priority on PPI and SGI interrupts
	 */
	for (i = 0; i < 32; i += 4) {
		gic_write(0xa0a0a0a0, 
			  gic->dist_base + GIC_DIST_PRI + i * 4 / 4);
	}

	gic_write(0xf0, gic->cpu_base + GIC_CPU_PRIMASK);
	gic_write(1, gic->cpu_base + GIC_CPU_CTRL);
}

int __init gic_init_bases(u32 gic_nr, u32 irq_start, 
			  virtual_addr_t cpu_base, 
			  virtual_addr_t dist_base)
{
	u32 gic_irqs;
	struct gic_chip_data *gic;

	BUG_ON(gic_nr >= GIC_MAX_NR);

	gic = &gic_data[gic_nr];
	gic->dist_base = dist_base;
	gic->cpu_base = cpu_base;
	gic->irq_offset = (irq_start - 1) & ~31;

	/*
	 * Find out how many interrupts are supported.
	 * The GIC only supports up to 1020 interrupt sources.
	 */
	gic_irqs = gic_read(gic->dist_base + GIC_DIST_CTR) & 0x1f;
	gic_irqs = (gic_irqs + 1) * 32;
	if (gic_irqs > 1020)
		gic_irqs = 1020;
	gic->gic_irqs = gic_irqs;

	gic_dist_init(gic, irq_start);
	gic_cpu_init(gic);

	return VMM_OK;
}

void __cpuinit gic_secondary_init(u32 gic_nr)
{
	BUG_ON(gic_nr >= GIC_MAX_NR);

	gic_cpu_init(&gic_data[gic_nr]);
}

static int gic_cnt = 0;

int __init gic_devtree_init(struct vmm_devtree_node *node, 
			    struct vmm_devtree_node *parent)
{
	int rc;
	u32 *aval, irq;
	virtual_addr_t cpu_base;
	virtual_addr_t dist_base;

	if (WARN_ON(!node)) {
		return VMM_ENODEV;
	}

	rc = vmm_devtree_regmap(node, &dist_base, 0);
	WARN(rc, "unable to map gic dist registers\n");

	rc = vmm_devtree_regmap(node, &cpu_base, 1);
	WARN(rc, "unable to map gic cpu registers\n");

	aval = vmm_devtree_attrval(node, "irq_start");
	WARN(!aval, "unable to get gic irq_start\n");
	irq = (aval) ? *aval : 0;
	gic_init_bases(gic_cnt, irq, cpu_base, dist_base);

	if (parent) {
		aval = vmm_devtree_attrval(node, "parent_irq");
		irq = (aval) ? *aval : 1020;
		gic_cascade_irq(gic_cnt, irq);
	} else {
		vmm_host_irq_set_active_callback(gic_active_irq);
	}

	gic_cnt++;

	return VMM_OK;
}

static int __cpuinit gic_init(struct vmm_devtree_node *node)
{
	int rc;

	if (vmm_smp_is_bootcpu()) {
		rc = gic_devtree_init(node, NULL);
	} else {
		gic_secondary_init(0);
		rc = VMM_OK;
	}

	return rc;
}
VMM_HOST_IRQ_INIT_DECLARE(rvgic, "arm,realview-gic", gic_init);
VMM_HOST_IRQ_INIT_DECLARE(ca9gic, "arm,cortex-a9-gic", gic_init);
VMM_HOST_IRQ_INIT_DECLARE(ca15gic, "arm,cortex-a15-gic", gic_init);

