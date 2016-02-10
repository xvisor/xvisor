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
 * @file irq-gic.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Generic Interrupt Controller Implementation
 *
 * The source has been largely adapted from Linux
 * drivers/irqchip/irq-gic.c
 *
 * The original code is licensed under the GPL.
 *
 *  Copyright (C) 2002 ARM Limited, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Interrupt architecture for the GIC:
 *
 * o There is one Interrupt Distributor, which receives interrupts
 *   from system devices and sends them to the Interrupt Controllers.
 *
 * o There is one CPU Interface per CPU, which sends interrupts sent
 *   by the Distributor, and interrupts generated locally, to the
 *   associated CPU. The base address of the CPU interface is usually
 *   aliased so that the same address points to different chips depending
 *   on the CPU it is accessed from.
 *
 * Note that IRQs 0-31 are special - they are local to each CPU.
 * As such, the enable set/clear, pending set/clear and active bit
 * registers are banked per-cpu for these sources.
 */

#include <vmm_error.h>
#include <vmm_limits.h>
#include <vmm_macros.h>
#include <vmm_smp.h>
#include <vmm_cpumask.h>
#include <vmm_stdio.h>
#include <vmm_host_io.h>
#include <vmm_host_irq.h>
#include <vmm_host_irqdomain.h>
#include <arch_barrier.h>

#define GIC_CPU_CTRL			0x00
#define GIC_CPU_PRIMASK			0x04
#define GIC_CPU_BINPOINT		0x08
#define GIC_CPU_INTACK			0x0c
#define GIC_CPU_EOI			0x10
#define GIC_CPU_RUNNINGPRI		0x14
#define GIC_CPU_HIGHPRI			0x18

#define GIC_CPU2_DIR			0x00

#define GIC_DIST_CTRL			0x000
#define GIC_DIST_CTR			0x004
#define GIC_DIST_ENABLE_SET		0x100
#define GIC_DIST_ENABLE_CLEAR		0x180
#define GIC_DIST_PENDING_SET		0x200
#define GIC_DIST_PENDING_CLEAR		0x280
#define GIC_DIST_ACTIVE_SET		0x300
#define GIC_DIST_ACTIVE_CLEAR		0x380
#define GIC_DIST_PRI			0x400
#define GIC_DIST_TARGET			0x800
#define GIC_DIST_CONFIG			0xc00
#define GIC_DIST_SOFTINT		0xf00

struct gic_chip_data {
	bool eoimode;			/* EOImode state */
	u32 hwirq_base; 		/* Starting physical IRQ number */
	u32 max_irqs;   		/* Total IRQs */
	virtual_addr_t dist_base;
	virtual_addr_t cpu_base;
	virtual_addr_t cpu2_base;
	struct vmm_host_irqdomain *domain;
};

#ifndef GIC_MAX_NR
#define GIC_MAX_NR	2
#endif

static int gic_cnt = 0;
static struct gic_chip_data gic_data[GIC_MAX_NR];

#define gic_write(val, addr)	vmm_writel_relaxed((val), (void *)(addr))
#define gic_read(addr)		vmm_readl_relaxed((void *)(addr))

static void gic_poke_irq(struct gic_chip_data *gic,
			 struct vmm_host_irq *d, u32 offset)
{
	u32 mask = 1 << (d->hwirq % 32);
	gic_write(mask, gic->dist_base + offset + (d->hwirq / 32) * 4);
}

static int gic_peek_irq(struct gic_chip_data *gic,
			struct vmm_host_irq *d, u32 offset)
{
	u32 mask = 1 << (d->hwirq % 32);
	return !!(gic_read(gic->dist_base + offset + (d->hwirq / 32) * 4) & mask);
}

static u32 gic_active_irq(u32 cpu_irq_nr)
{
	u32 ret = gic_read(gic_data[0].cpu_base + GIC_CPU_INTACK) & 0x3FF;

	if (ret < 1021) {
		ret = vmm_host_irqdomain_find_mapping(gic_data[0].domain, ret);
	} else {
		ret = UINT_MAX;
	}

	return ret;
}

static void gic_mask_irq(struct vmm_host_irq *d)
{
	gic_poke_irq(vmm_host_irq_get_chip_data(d), d, GIC_DIST_ENABLE_CLEAR);
}

static void gic_unmask_irq(struct vmm_host_irq *d)
{
	gic_poke_irq(vmm_host_irq_get_chip_data(d), d, GIC_DIST_ENABLE_SET);
}

static void gic_eoi_irq(struct vmm_host_irq *d)
{
	struct gic_chip_data *gic = vmm_host_irq_get_chip_data(d);

	gic_write(d->hwirq, gic->cpu_base + GIC_CPU_EOI);
	if (gic->eoimode && !vmm_host_irq_is_routed(d)) {
		gic_write(d->hwirq, gic->cpu2_base + GIC_CPU2_DIR);
	}
}

static int gic_set_type(struct vmm_host_irq *d, u32 type)
{
	struct gic_chip_data *gic = vmm_host_irq_get_chip_data(d);
	virtual_addr_t base = gic->dist_base;
	u32 enablemask = 1 << (d->hwirq % 32);
	u32 enableoff = (d->hwirq / 32) * 4;
	u32 confmask = 0x2 << ((d->hwirq % 16) * 2);
	u32 confoff = (d->hwirq / 16) * 4;
	bool enabled = FALSE;
	u32 val;

	/* Interrupt configuration for SGIs can't be changed */
	if (d->hwirq < 16) {
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
static void gic_raise(struct vmm_host_irq *d,
		      const struct vmm_cpumask *mask)
{
	unsigned long map = *vmm_cpumask_bits(mask);

	/*
	 * Ensure that stores to Normal memory are visible to the
	 * other CPUs before issuing the IPI.
	 */
	arch_wmb();

	/* This always happens on GIC0 */
	gic_write(map << 16 | d->hwirq,
		  gic_data[0].dist_base + GIC_DIST_SOFTINT);
}

static int gic_set_affinity(struct vmm_host_irq *d,
			    const struct vmm_cpumask *mask_val,
			    bool force)
{
	virtual_addr_t reg;
	u32 shift = (d->hwirq % 4) * 8;
	u32 cpu = vmm_cpumask_first(mask_val);
	u32 val, mask, bit;
	struct gic_chip_data *gic = vmm_host_irq_get_chip_data(d);

	if (cpu >= 8)
		return VMM_EINVALID;

	reg = gic->dist_base + GIC_DIST_TARGET + (d->hwirq & ~3);
	mask = 0xff << shift;
	bit = 1 << (cpu + shift);

	val = gic_read(reg) & ~mask;
	gic_write(val | bit, reg);

	return 0;
}
#endif

static u32 gic_irq_get_routed_state(struct vmm_host_irq *d, u32 mask)
{
	u32 val = 0;
	struct gic_chip_data *gic = vmm_host_irq_get_chip_data(d);

	if ((mask & VMM_ROUTED_IRQ_STATE_PENDING) &&
	    gic_peek_irq(gic, d, GIC_DIST_ENABLE_SET))
		val |= VMM_ROUTED_IRQ_STATE_PENDING;
	if ((mask & VMM_ROUTED_IRQ_STATE_ACTIVE) &&
	    gic_peek_irq(gic, d, GIC_DIST_ACTIVE_SET))
		val |= VMM_ROUTED_IRQ_STATE_ACTIVE;
	if ((mask & VMM_ROUTED_IRQ_STATE_MASKED) &&
	    !gic_peek_irq(gic, d, GIC_DIST_ENABLE_SET))
		val |= VMM_ROUTED_IRQ_STATE_MASKED;

	return val;
}

static void gic_irq_set_routed_state(struct vmm_host_irq *d,
				     u32 val, u32 mask)
{
	struct gic_chip_data *gic = vmm_host_irq_get_chip_data(d);

	if (mask & VMM_ROUTED_IRQ_STATE_PENDING)
		gic_poke_irq(gic, d, (val & VMM_ROUTED_IRQ_STATE_PENDING) ?
				GIC_DIST_ENABLE_SET : GIC_DIST_ENABLE_CLEAR);
	if (mask & VMM_ROUTED_IRQ_STATE_ACTIVE)
		gic_poke_irq(gic, d, (val & VMM_ROUTED_IRQ_STATE_ACTIVE) ?
				GIC_DIST_ACTIVE_SET : GIC_DIST_ACTIVE_CLEAR);
	if (mask & VMM_ROUTED_IRQ_STATE_MASKED)
		gic_poke_irq(gic, d, (val & VMM_ROUTED_IRQ_STATE_MASKED) ?
				GIC_DIST_ENABLE_CLEAR : GIC_DIST_ENABLE_SET);
}

static vmm_irq_return_t gic_handle_cascade_irq(int irq, void *dev)
{
	struct gic_chip_data *gic = dev;
	u32 cascade_irq, gic_irq;

	gic_irq = gic_read(gic->cpu_base + GIC_CPU_INTACK) & 0x3FF;
	if (gic_irq == 1023) {
		return VMM_IRQ_NONE;
	}

	cascade_irq = vmm_host_irqdomain_find_mapping(gic->domain, gic_irq);
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
	.irq_raise		= gic_raise,
#endif
	.irq_get_routed_state	= gic_irq_get_routed_state,
	.irq_set_routed_state	= gic_irq_set_routed_state,
};

static void __init gic_cascade_irq(u32 gic_nr, u32 irq)
{
	if (gic_nr >= GIC_MAX_NR)
		BUG();
	if (vmm_host_irq_register(irq, "GIC-CHILD", 
				  gic_handle_cascade_irq, 
				  &gic_data[gic_nr])) {
		BUG();
	}
}

static void __init gic_dist_init(struct gic_chip_data *gic)
{
	int hirq;
	unsigned int i;
	u32 cpumask = 1 << vmm_smp_processor_id();
	virtual_addr_t base = gic->dist_base;

	cpumask |= cpumask << 8;
	cpumask |= cpumask << 16;

	/* Disable IRQ distribution */
	gic_write(0, base + GIC_DIST_CTRL);

	/*
	 * Set all global interrupts to be level triggered, active low.
	 */
	for (i = 32; i < gic->max_irqs; i += 16) {
		gic_write(0, base + GIC_DIST_CONFIG + i * 4 / 16);
	}

	/*
	 * Set all global interrupts to this CPU only.
	 */
	for (i = 32; i < gic->max_irqs; i += 4) {
		gic_write(cpumask, base + GIC_DIST_TARGET + i * 4 / 4);
	}

	/*
	 * Set priority on all interrupts.
	 */
	for (i = 0; i < gic->max_irqs; i += 4) {
		gic_write(0xa0a0a0a0, base + GIC_DIST_PRI + i * 4 / 4);
	}

	/*
	 * Disable all interrupts.
	 */
	for (i = 0; i < gic->max_irqs; i += 32) {
		gic_write(0xffffffff,
			  base + GIC_DIST_ENABLE_CLEAR + i * 4 / 32);
	}

	/*
	 * Setup the Host IRQ subsystem.
	 * Note: We handle all interrupts including SGIs and PPIs via C code.
	 * The Linux kernel handles pheripheral interrupts via C code and 
	 * SGI/PPI via assembly code.
	 */
	for (i = 0; i < gic->max_irqs; i++) {
		hirq = vmm_host_irqdomain_create_mapping(gic->domain, i);
		BUG_ON(hirq < 0);
		vmm_host_irq_set_chip(hirq, &gic_chip);
		vmm_host_irq_set_chip_data(hirq, gic);
		if (hirq < 32) {
			vmm_host_irq_set_handler(hirq, vmm_handle_percpu_irq);
			if (hirq < 16) {
				/* Mark SGIs as IPIs */
				vmm_host_irq_mark_ipi(hirq);
			}
			/* Mark SGIs and PPIs as per-CPU IRQs */
			vmm_host_irq_mark_per_cpu(hirq);
		} else {
			vmm_host_irq_set_handler(hirq, vmm_handle_fast_eoi);
		}
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
	if (gic->eoimode) {
		gic_write(1|(1<<9), gic->cpu_base + GIC_CPU_CTRL);
	} else {
		gic_write(1, gic->cpu_base + GIC_CPU_CTRL);
	}
}

static int gic_of_xlate(struct vmm_host_irqdomain *d,
			struct vmm_devtree_node *controller,
			const u32 *intspec, unsigned int intsize,
			unsigned long *out_hwirq, unsigned int *out_type)
{
	if (d->of_node != controller)
		return VMM_EINVALID;
	if (intsize < 3)
		return VMM_EINVALID;

	/* Get the interrupt number and add 16 to skip over SGIs */
	*out_hwirq = intspec[1] + 16;

	/* For SPIs, we need to add 16 more to get the GIC irq ID number */
	if (!intspec[0])
		*out_hwirq += 16;

	*out_type = intspec[2] & VMM_IRQ_TYPE_SENSE_MASK;

	return VMM_OK;
}

static struct vmm_host_irqdomain_ops gic_ops = {
	.xlate = gic_of_xlate,
};

static int __init gic_init_bases(struct vmm_devtree_node *node,
				 u32 gic_nr,
				 bool eoimode,
				 u32 irq_start,
				 virtual_addr_t cpu_base,
				 virtual_addr_t cpu2_base,
				 virtual_addr_t dist_base)
{
	u32 max_irqs;
	struct gic_chip_data *gic;

	BUG_ON(gic_nr >= GIC_MAX_NR);

	gic = &gic_data[gic_nr];

	gic->eoimode = eoimode;
	/* For primary GICs, skip over SGIs.
	 * For secondary GICs, skip over PPIs, too.
	 */
	gic->hwirq_base = (gic_nr == 0) ? 16 : 32;
	gic->dist_base = dist_base;
	gic->cpu_base = cpu_base;
	gic->cpu2_base = cpu2_base;

	/*
	 * Find out how many interrupts are supported.
	 * The GIC only supports up to 1020 interrupt sources.
	 */
	max_irqs = gic_read(gic->dist_base + GIC_DIST_CTR) & 0x1f;
	max_irqs = (max_irqs + 1) * 32;
	if (max_irqs > 1020)
		max_irqs = 1020;

	gic->max_irqs = max_irqs;

	gic->domain = vmm_host_irqdomain_add(node, (int)irq_start, max_irqs,
					     &gic_ops, gic);
	if (!gic->domain) {
		return VMM_EFAIL;
	}

	gic_dist_init(gic);
	gic_cpu_init(gic);

	return VMM_OK;
}

static void __cpuinit gic_secondary_init(u32 gic_nr)
{
	BUG_ON(gic_nr >= GIC_MAX_NR);

	gic_cpu_init(&gic_data[gic_nr]);
}

static int __init gic_devtree_init(struct vmm_devtree_node *node,
				   struct vmm_devtree_node *parent,
				   bool eoimode)
{
	int rc;
	u32 irq, irq_start = 0;
	physical_size_t cpu_sz;
	virtual_addr_t cpu_base;
	virtual_addr_t cpu2_base;
	virtual_addr_t dist_base;

	if (WARN_ON(!node)) {
		return VMM_ENODEV;
	}

	rc = vmm_devtree_request_regmap(node, &dist_base, 0, "GIC Dist");
	WARN(rc, "unable to map gic dist registers\n");

	rc = vmm_devtree_request_regmap(node, &cpu_base, 1, "GIC CPU");
	WARN(rc, "unable to map gic cpu registers\n");

	rc = vmm_devtree_request_regmap(node, &cpu2_base, 4, "GIC CPU2");
	if (rc) {
		rc = vmm_devtree_regsize(node, &cpu_sz, 1);
		if (rc) {
			return rc;
		}
		if (cpu_sz >= 0x20000) {
			cpu2_base = cpu_base + 0x10000;
		} else if (cpu_sz >= 0x2000) {
			cpu2_base = cpu_base + 0x1000;
		} else {
			cpu2_base = 0x0;
		}
	}

	if (vmm_devtree_read_u32(node, "irq_start", &irq_start)) {
		irq_start = 0;
	}

	rc = gic_init_bases(node, gic_cnt, eoimode, irq_start,
			    cpu_base, cpu2_base, dist_base);
	if (rc) {
		return rc;
	}

	if (parent) {
		if (vmm_devtree_read_u32(node, "parent_irq", &irq)) {
			irq = 1020;
		}
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
		rc = gic_devtree_init(node, NULL, FALSE);
	} else {
		gic_secondary_init(0);
		rc = VMM_OK;
	}

	return rc;
}

static int __cpuinit gic_eoimode_init(struct vmm_devtree_node *node)
{
	int rc;

	if (vmm_smp_is_bootcpu()) {
		rc = gic_devtree_init(node, NULL, TRUE);
	} else {
		gic_secondary_init(0);
		rc = VMM_OK;
	}

	return rc;
}

VMM_HOST_IRQ_INIT_DECLARE(rvgic, "arm,realview-gic", gic_init);
VMM_HOST_IRQ_INIT_DECLARE(ca9gic, "arm,cortex-a9-gic", gic_init);
VMM_HOST_IRQ_INIT_DECLARE(ca15gic, "arm,cortex-a15-gic", gic_eoimode_init);

