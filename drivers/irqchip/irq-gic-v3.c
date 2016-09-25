/**
 * Copyright (c) 2016 Anup Patel.
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
 * @file irq-gic-v3.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Generic Interrupt Controller version 3 implementation
 *
 * The source has been largely adapted from Linux
 * drivers/irqchip/irq-gic-v3.c
 *
 * Copyright (C) 2013, 2014 ARM Limited, All Rights Reserved.
 * Author: Marc Zyngier <marc.zyngier@arm.com>
 *
 * The original code is licensed under the GPL.
 */

#include <vmm_error.h>
#include <vmm_limits.h>
#include <vmm_macros.h>
#include <vmm_smp.h>
#include <vmm_cpumask.h>
#include <vmm_delay.h>
#include <vmm_timer.h>
#include <vmm_heap.h>
#include <vmm_stdio.h>
#include <vmm_host_io.h>
#include <vmm_host_irq.h>
#include <vmm_host_irqdomain.h>
#include <arch_gicv3.h>
#include <drv/irqchip/arm-gic-v3.h>

struct rdists {
	struct {
		void *rd_base;
		void *pend_page;
		physical_addr_t phys_base;
	} rdist[CONFIG_CPU_COUNT];
	void *prop_page;
	int id_bits;
	u64 flags;
};

struct redist_region {
	void *redist_base;
	physical_addr_t phys_base;
	bool single_redist;
};

struct gic_chip_data {
	struct vmm_devtree_node *node;
	u32 irq_nr;
	u32 domain_irq_nr;
	void *dist_base;
	struct redist_region *redist_regions;
	u32 nr_redist_regions;
	u64 redist_stride;
	struct rdists rdists;
	struct vmm_host_irqdomain *domain;
};

static struct gic_chip_data gic_data;
static bool supports_deactivate = TRUE;

#define gic_writel(val, addr)	vmm_writel_relaxed((val), (void *)(addr))
#define gic_readl(addr)		vmm_readl_relaxed((void *)(addr))

#define gic_data_rdist()		\
			(&gic_data.rdists.rdist[vmm_smp_processor_id()])
#define gic_data_rdist_rd_base()	(gic_data_rdist()->rd_base)
#define gic_data_rdist_sgi_base()	(gic_data_rdist_rd_base() + 0x10000)

/* Our default, arbitrary priority value. Linux only uses one anyway. */
#define DEFAULT_PMR_VALUE	0xf0

static void gic_udelay(unsigned long usecs)
{
	u32 i;

	if (vmm_timer_started()) {
		vmm_udelay(usecs);
	} else {
		while (usecs--) {
			for (i = 0; i < 1000; i++);
		}
	}
}

static inline bool gic_enable_sre(void)
{
	u32 val;

	val = arch_gic_read_sre();
	if (val & ICC_SRE_EL2_SRE)
		return TRUE;

	val |= ICC_SRE_EL2_SRE;
	val |= ICC_SRE_EL2_ENABLE;
	arch_gic_write_sre(val);
	val = arch_gic_read_sre();

	return !!(val & ICC_SRE_EL2_SRE);
}

static inline unsigned int gic_irq(struct vmm_host_irq *d)
{
	return d->hwirq;
}

static inline int gic_irq_in_rdist(struct vmm_host_irq *d)
{
	return gic_irq(d) < 32;
}

static inline void *gic_dist_base(struct vmm_host_irq *d)
{
	if (gic_irq_in_rdist(d))	/* SGI+PPI -> SGI_base for this CPU */
		return gic_data_rdist_sgi_base();

	if (d->hwirq <= 1023)		/* SPI -> dist_base */
		return gic_data.dist_base;

	return NULL;
}

static void gic_do_wait_for_rwp(void *base)
{
	u32 count = 1000000;	/* 1s! */

	while (gic_readl(base + GICD_CTLR) & GICD_CTLR_RWP) {
		count--;
		if (!count) {
			vmm_lerror("GICv3", "RWP timeout, gone fishing\n");
			return;
		}
		gic_udelay(1);
	};
}

/* Wait for completion of a distributor change */
static void gic_dist_wait_for_rwp(void)
{
	gic_do_wait_for_rwp(gic_data.dist_base);
}

/* Wait for completion of a redistributor change */
static void gic_redist_wait_for_rwp(void)
{
	gic_do_wait_for_rwp(gic_data_rdist_rd_base());
}

#if 0 /* TODO: */
#ifdef CONFIG_ARM64
static bool is_cavium_thunderx = FALSE;

static u64 gic_read_iar(void)
{
	if (unlikely(&is_cavium_thunderx))
		return arch_gic_read_iar_cavium_thunderx();
	else
		return arch_gic_read_iar();
}
#endif
#endif

static u64 gic_read_iar(void)
{
	return arch_gic_read_iar();
}

static void gic_enable_redist(bool enable)
{
	void *rbase;
	u32 count = 1000000;	/* 1s! */
	u32 val;

	rbase = gic_data_rdist_rd_base();

	val = gic_readl(rbase + GICR_WAKER);
	if (enable)
		/* Wake up this CPU redistributor */
		val &= ~GICR_WAKER_ProcessorSleep;
	else
		val |= GICR_WAKER_ProcessorSleep;
	gic_writel(val, rbase + GICR_WAKER);

	if (!enable) {		/* Check that GICR_WAKER is writeable */
		val = gic_readl(rbase + GICR_WAKER);
		if (!(val & GICR_WAKER_ProcessorSleep))
			return;	/* No PM support in this redistributor */
	}

	while (count--) {
		val = gic_readl(rbase + GICR_WAKER);
		if (enable ^ (bool)(val & GICR_WAKER_ChildrenAsleep))
			break;
		gic_udelay(1);
	};
	if (!count)
		vmm_lerror("GICv3", "redistributor failed to %s...\n",
				   enable ? "wakeup" : "sleep");
}

/*
 * Routines to disable, enable, EOI and route interrupts
 */
static int gic_peek_irq(struct vmm_host_irq *d, u32 offset)
{
	u32 mask = 1 << (gic_irq(d) % 32);
	void *base;

	if (gic_irq_in_rdist(d))
		base = gic_data_rdist_sgi_base();
	else
		base = gic_data.dist_base;

	return !!(gic_readl(base + offset + (gic_irq(d) / 32) * 4) & mask);
}

static void gic_poke_irq(struct vmm_host_irq *d, u32 offset)
{
	u32 mask = 1 << (gic_irq(d) % 32);
	void (*rwp_wait)(void);
	void *base;

	if (gic_irq_in_rdist(d)) {
		base = gic_data_rdist_sgi_base();
		rwp_wait = gic_redist_wait_for_rwp;
	} else {
		base = gic_data.dist_base;
		rwp_wait = gic_dist_wait_for_rwp;
	}

	gic_writel(mask, base + offset + (gic_irq(d) / 32) * 4);
	rwp_wait();
}

static void gic_mask_irq(struct vmm_host_irq *d)
{
	gic_poke_irq(d, GICD_ICENABLER);
	/*
	 * When masking a forwarded interrupt, make sure it is
	 * deactivated as well.
	 *
	 * This ensures that an interrupt that is getting
	 * disabled/masked will not get "stuck", because there is
	 * noone to deactivate it (guest is being terminated).
	 */
	if (vmm_host_irq_is_routed(d))
		gic_poke_irq(d, GICD_ICACTIVER);
}

static void gic_unmask_irq(struct vmm_host_irq *d)
{
	gic_poke_irq(d, GICD_ISENABLER);
}

static void gic_irq_set_routed_state(struct vmm_host_irq *d,
				     u32 val, u32 mask)
{
	if (gic_irq(d) >= 8192) /* PPI/SPI only */
		return;

	if (mask & VMM_ROUTED_IRQ_STATE_PENDING)
		gic_poke_irq(d, (val & VMM_ROUTED_IRQ_STATE_PENDING) ?
				GICD_ISPENDR : GICD_ICPENDR);
	if (mask & VMM_ROUTED_IRQ_STATE_ACTIVE)
		gic_poke_irq(d, (val & VMM_ROUTED_IRQ_STATE_ACTIVE) ?
				GICD_ISACTIVER : GICD_ICACTIVER);
	if (mask & VMM_ROUTED_IRQ_STATE_MASKED)
		gic_poke_irq(d, (val & VMM_ROUTED_IRQ_STATE_MASKED) ?
				GICD_ICENABLER : GICD_ISENABLER);
}

static u32 gic_irq_get_routed_state(struct vmm_host_irq *d, u32 mask)
{
	u32 val = 0;

	if (gic_irq(d) >= 8192) /* PPI/SPI only */
		return 0x0;

	if ((mask & VMM_ROUTED_IRQ_STATE_PENDING) &&
	    gic_peek_irq(d, GICD_ISPENDR))
		val |= VMM_ROUTED_IRQ_STATE_PENDING;
	if ((mask & VMM_ROUTED_IRQ_STATE_ACTIVE) &&
	    gic_peek_irq(d, GICD_ISACTIVER))
		val |= VMM_ROUTED_IRQ_STATE_ACTIVE;
	if ((mask & VMM_ROUTED_IRQ_STATE_MASKED) &&
	    !gic_peek_irq(d, GICD_ISENABLER))
		val |= VMM_ROUTED_IRQ_STATE_MASKED;

	return val;
}

static u32 gic_active_irq(u32 cpu_irq_nr)
{
	u32 irqnr = gic_read_iar();

	if (irqnr == ICC_IAR1_EL1_SPURIOUS)
		return UINT_MAX;

	return vmm_host_irqdomain_find_mapping(gic_data.domain, irqnr);
}

static void gic_eoi_irq(struct vmm_host_irq *d)
{
	arch_gic_write_eoir(gic_irq(d));

	/*
	 * No need to deactivate an LPI, or an interrupt that
	 * is is getting forwarded to a vcpu.
	 */
	if (gic_irq(d) >= 8192 || vmm_host_irq_is_routed(d))
		return;
	arch_gic_write_dir(gic_irq(d));
}

static int gic_configure_irq(unsigned int irq, unsigned int type,
			     void *base, void (*sync_access)(void))
{
	u32 confmask = 0x2 << ((irq % 16) * 2);
	u32 confoff = (irq / 16) * 4;
	u32 val, oldval;
	int ret = 0;

	/*
	 * Read current configuration register, and insert the config
	 * for "irq", depending on "type".
	 */
	val = oldval = gic_readl(base + GICD_ICFGR + confoff);
	if (type & VMM_IRQ_TYPE_LEVEL_MASK)
		val &= ~confmask;
	else if (type & VMM_IRQ_TYPE_EDGE_BOTH)
		val |= confmask;

	/* If the current configuration is the same, then we are done */
	if (val == oldval)
		return 0;

	/*
	 * Write back the new configuration, and possibly re-enable
	 * the interrupt. If we fail to write a new configuration for
	 * an SPI then WARN and return an error. If we fail to write the
	 * configuration for a PPI this is most likely because the GIC
	 * does not allow us to set the configuration or we are in a
	 * non-secure mode, and hence it may not be catastrophic.
	 */
	gic_writel(val, base + GICD_ICFGR + confoff);
	if (gic_readl(base + GICD_ICFGR + confoff) != val) {
		if (WARN_ON(irq >= 32))
			ret = VMM_EINVALID;
		else
			vmm_lwarning("GICv3",
				     "PPI%d is secure or misconfigured\n",
				     irq - 16);
	}

	if (sync_access)
		sync_access();

	return ret;
}

static int gic_set_type(struct vmm_host_irq *d, u32 type)
{
	unsigned int irq = gic_irq(d);
	void (*rwp_wait)(void);
	void *base;

	/* Interrupt configuration for SGIs can't be changed */
	if (irq < 16) {
		return VMM_EINVALID;
	}

	if (irq >= 32 && type != VMM_IRQ_TYPE_LEVEL_HIGH &&
			 type != VMM_IRQ_TYPE_EDGE_RISING) {
		return VMM_EINVALID;
	}

	if (gic_irq_in_rdist(d)) {
		base = gic_data_rdist_sgi_base();
		rwp_wait = gic_redist_wait_for_rwp;
	} else {
		base = gic_data.dist_base;
		rwp_wait = gic_dist_wait_for_rwp;
	}

	return gic_configure_irq(irq, type, base, rwp_wait);
}

static u64 gic_mpidr_to_affinity(unsigned long mpidr)
{
	u64 aff;

	aff = ((u64)MPIDR_AFFINITY_LEVEL(mpidr, 3) << 32 |
	       MPIDR_AFFINITY_LEVEL(mpidr, 2) << 16 |
	       MPIDR_AFFINITY_LEVEL(mpidr, 1) << 8  |
	       MPIDR_AFFINITY_LEVEL(mpidr, 0));

	return aff;
}

#ifdef CONFIG_SMP

static u16 gic_compute_target_list(int *base_cpu,
				   const struct vmm_cpumask *mask,
				   unsigned long cluster_id)
{
	int cpu = *base_cpu;
	unsigned long mpidr = arch_gic_cpu_logical_map(cpu);
	u16 tlist = 0;

	while (cpu < vmm_cpu_count) {
		/*
		 * If we ever get a cluster of more than 16 CPUs, just
		 * scream and skip that CPU.
		 */
		if (WARN_ON((mpidr & 0xff) >= 16))
			goto out;

		tlist |= 1 << (mpidr & 0xf);

		cpu = vmm_cpumask_next(cpu, mask);
		if (cpu >= vmm_cpu_count)
			goto out;

		mpidr = arch_gic_cpu_logical_map(cpu);

		if (cluster_id != (mpidr & ~0xffUL)) {
			cpu--;
			goto out;
		}
	}
out:
	*base_cpu = cpu;
	return tlist;
}

#define MPIDR_TO_SGI_AFFINITY(cluster_id, level) \
	(MPIDR_AFFINITY_LEVEL(cluster_id, level) \
		<< ICC_SGI1R_AFFINITY_## level ##_SHIFT)

static void gic_send_sgi(u64 cluster_id, u16 tlist, unsigned int irq)
{
	u64 val;

	val = (MPIDR_TO_SGI_AFFINITY(cluster_id, 3)	|
	       MPIDR_TO_SGI_AFFINITY(cluster_id, 2)	|
	       irq << ICC_SGI1R_SGI_ID_SHIFT		|
	       MPIDR_TO_SGI_AFFINITY(cluster_id, 1)	|
	       tlist << ICC_SGI1R_TARGET_LIST_SHIFT);

	arch_gic_write_sgi1r(val);
}

static void gic_raise(struct vmm_host_irq *d,
		      const struct vmm_cpumask *mask)
{
	int cpu;
	unsigned int irq = d->hwirq;

	if (WARN_ON(irq >= 16))
		return;

	/*
	 * Ensure that stores to Normal memory are visible to the
	 * other CPUs before issuing the IPI.
	 */
	arch_smp_wmb();

	for_each_cpu(cpu, mask) {
		unsigned long cluster_id;
		u16 tlist;

		cluster_id = arch_gic_cpu_logical_map(cpu) & ~0xffUL;
		tlist = gic_compute_target_list(&cpu, mask, cluster_id);
		gic_send_sgi(cluster_id, tlist, irq);
	}
}

static int gic_set_affinity(struct vmm_host_irq *d,
			    const struct vmm_cpumask *mask_val,
			    bool force)
{
	u64 val;
	void *reg;
	int enabled;
	unsigned int cpu = vmm_cpumask_any_and(mask_val, cpu_online_mask);

	if (gic_irq_in_rdist(d))
		return VMM_EINVALID;

	/* If interrupt was enabled, disable it first */
	enabled = gic_peek_irq(d, GICD_ISENABLER);
	if (enabled)
		gic_mask_irq(d);

	reg = gic_dist_base(d) + GICD_IROUTER + (gic_irq(d) * 8);
	val = gic_mpidr_to_affinity(arch_gic_cpu_logical_map(cpu));

	arch_gic_write_irouter(val, reg);

	/*
	 * If the interrupt was enabled, enabled it again. Otherwise,
	 * just wait for the distributor to have digested our changes.
	 */
	if (enabled)
		gic_unmask_irq(d);
	else
		gic_dist_wait_for_rwp();

	return 0;
}

#endif

static struct vmm_host_irq_chip gic_chip = {
	.name			= "GICv3",
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

static void gic_dist_config(void *base, int gic_irqs,
			    void (*sync_access)(void))
{
	unsigned int i;

	/*
	 * Set all global interrupts to be level triggered, active low.
	 */
	for (i = 32; i < gic_irqs; i += 16)
		gic_writel(GICD_INT_ACTLOW_LVLTRIG,
			   base + GICD_ICFGR + i / 4);

	/*
	 * Set priority on all global interrupts.
	 */
	for (i = 32; i < gic_irqs; i += 4)
		gic_writel(GICD_INT_DEF_PRI_X4, base + GICD_IPRIORITYR + i);

	/*
	 * Deactivate and disable all SPIs. Leave the PPI and SGIs
	 * alone as they are in the redistributor registers on GICv3.
	 */
	for (i = 32; i < gic_irqs; i += 32) {
		gic_writel(GICD_INT_EN_CLR_X32,
			   base + GICD_ICACTIVER + i / 8);
		gic_writel(GICD_INT_EN_CLR_X32,
			   base + GICD_ICENABLER + i / 8);
	}

	if (sync_access)
		sync_access();
}

static void __init gic_dist_init(void)
{
	int hirq;
	u64 affinity;
	unsigned int i;
	void *base = gic_data.dist_base;

	/* Disable the distributor */
	gic_writel(0, base + GICD_CTLR);
	gic_dist_wait_for_rwp();

	/*
	 * Configure SPIs as non-secure Group-1. This will only matter
	 * if the GIC only has a single security state. This will not
	 * do the right thing if the kernel is running in secure mode,
	 * but that's not the intended use case anyway.
	 */
	for (i = 32; i < gic_data.irq_nr; i += 32)
		gic_writel(~0, base + GICD_IGROUPR + i / 8);

	gic_dist_config(base, gic_data.irq_nr, gic_dist_wait_for_rwp);

	/*
	 * Setup the Host IRQ subsystem.
	 * Note: We handle all interrupts including SGIs and PPIs via C code.
	 */
	for (i = 0; i < gic_data.domain_irq_nr; i++) {
		hirq = vmm_host_irqdomain_create_mapping(gic_data.domain, i);
		BUG_ON(hirq < 0);
		vmm_host_irq_set_chip(hirq, &gic_chip);
		vmm_host_irq_set_chip_data(hirq, &gic_data);
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

	/* Enable distributor with ARE, Group1 */
	gic_writel(GICD_CTLR_ARE_NS | GICD_CTLR_ENABLE_G1A | GICD_CTLR_ENABLE_G1,
		   base + GICD_CTLR);

	/*
	 * Set all global interrupts to the boot CPU only. ARE must be
	 * enabled.
	 */
	affinity = arch_gic_cpu_logical_map(vmm_smp_processor_id());
	affinity = gic_mpidr_to_affinity(affinity);
	for (i = 32; i < gic_data.irq_nr; i++)
		arch_gic_write_irouter(affinity, base + GICD_IROUTER + i * 8);
}

static int gic_populate_rdist(void)
{
	int i;
	u32 aff;
	u64 typer;
	unsigned long mpidr;

	mpidr = arch_gic_cpu_logical_map(vmm_smp_processor_id());

	/*
	 * Convert affinity to a 32bit value that can be matched to
	 * GICR_TYPER bits [63:32].
	 */
	aff = (MPIDR_AFFINITY_LEVEL(mpidr, 3) << 24 |
	       MPIDR_AFFINITY_LEVEL(mpidr, 2) << 16 |
	       MPIDR_AFFINITY_LEVEL(mpidr, 1) << 8 |
	       MPIDR_AFFINITY_LEVEL(mpidr, 0));

	for (i = 0; i < gic_data.nr_redist_regions; i++) {
		void *ptr = gic_data.redist_regions[i].redist_base;
		u32 reg;

		reg = gic_readl(ptr + GICR_PIDR2) & GIC_PIDR2_ARCH_MASK;
		if (reg != GIC_PIDR2_ARCH_GICv3 &&
		    reg != GIC_PIDR2_ARCH_GICv4) { /* We're in trouble... */
			vmm_lwarning("GICv3",
				     "No redistributor present @%p\n", ptr);
			break;
		}

		do {
			typer = arch_gic_read_typer(ptr + GICR_TYPER);
			if ((typer >> 32) == aff) {
				u64 offset = ptr - gic_data.redist_regions[i].redist_base;
				gic_data_rdist_rd_base() = ptr;
				gic_data_rdist()->phys_base = gic_data.redist_regions[i].phys_base + offset;
				vmm_printf("CPU%d: found redistributor %lx region %d:%pa\n",
					vmm_smp_processor_id(), mpidr, i,
					&gic_data_rdist()->phys_base);
				return 0;
			}

			if (gic_data.redist_regions[i].single_redist)
				break;

			if (gic_data.redist_stride) {
				ptr += gic_data.redist_stride;
			} else {
				ptr += 0x10000 * 2; /* Skip RD_base + SGI_base */
				if (typer & GICR_TYPER_VLPIS)
					ptr += 0x10000 * 2; /* Skip VLPI_base + reserved page */
			}
		} while (!(typer & GICR_TYPER_LAST));
	}

	/* We couldn't even deal with ourselves... */
	vmm_printf("CPU%d: mpidr %lx has no re-distributor!\n",
		   vmm_smp_processor_id(), mpidr);

	return VMM_ENODEV;
}

static void gic_cpu_sys_reg_init(void)
{
	/*
	 * Need to check that the SRE bit has actually been set. If
	 * not, it means that SRE is disabled at EL2. We're going to
	 * die painfully, and there is nothing we can do about it.
	 *
	 * Kindly inform the luser.
	 */
	if (!gic_enable_sre())
		vmm_lerror("GICv3",
			   "unable to set SRE (disabled at EL2), panic ahead\n");

	/* Set priority mask register */
	arch_gic_write_pmr(DEFAULT_PMR_VALUE);

	if (supports_deactivate) {
		/* EOI drops priority only (mode 1) */
		arch_gic_write_ctlr(ICC_CTLR_EL1_EOImode_drop);
	} else {
		/* EOI deactivates interrupt too (mode 0) */
		arch_gic_write_ctlr(ICC_CTLR_EL1_EOImode_drop_dir);
	}

	/* ... and let's hit the road... */
	arch_gic_write_grpen1(1);
}

#if 0 /* TODO: */
static int gic_dist_supports_lpis(void)
{
	return !!(gic_readl(gic_data.dist_base + GICD_TYPER) & GICD_TYPER_LPIS);
}
#endif

static void gic_cpu_config(void *base, void (*sync_access)(void))
{
	int i;

	/*
	 * Deal with the banked PPI and SGI interrupts - disable all
	 * PPI interrupts, ensure all SGI interrupts are enabled.
	 * Make sure everything is deactivated.
	 */
	gic_writel(GICD_INT_EN_CLR_X32, base + GICD_ICACTIVER);
	gic_writel(GICD_INT_EN_CLR_PPI, base + GICD_ICENABLER);
	gic_writel(GICD_INT_EN_SET_SGI, base + GICD_ISENABLER);

	/*
	 * Set priority on PPI and SGI interrupts
	 */
	for (i = 0; i < 32; i += 4)
		gic_writel(GICD_INT_DEF_PRI_X4,
			   base + GICD_IPRIORITYR + i * 4 / 4);

	if (sync_access)
		sync_access();
}

static void gic_cpu_init(void)
{
	void *rbase;

	/* Register ourselves with the rest of the world */
	if (gic_populate_rdist())
		return;

	gic_enable_redist(TRUE);

	rbase = gic_data_rdist_sgi_base();

	/* Configure SGIs/PPIs as non-secure Group-1 */
	gic_writel(~0, rbase + GICR_IGROUPR0);

	gic_cpu_config(rbase, gic_redist_wait_for_rwp);

#if 0 /* TODO: */
	/* Give LPIs a spin */
	if (IS_ENABLED(CONFIG_ARM_GIC_V3_ITS) && gic_dist_supports_lpis())
		its_cpu_init();
#endif

	/* initialise system registers */
	gic_cpu_sys_reg_init();
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

static void gicv3_enable_quirks(void)
{
#if 0 /* TODO: */
#ifdef CONFIG_ARM64
	if (cpus_have_cap(ARM64_WORKAROUND_CAVIUM_23154))
		static_branch_enable(&is_cavium_thunderx);
#endif
#endif
}

static int __init gic_init_bases(void *dist_base,
				 struct redist_region *rdist_regs,
				 u32 nr_redist_regions,
				 u64 redist_stride,
				 struct vmm_devtree_node *node)
{
	u32 typer;
	int gic_irqs;

	/* Hyp-mode always available */
	supports_deactivate = TRUE;
	if (supports_deactivate)
		vmm_linfo("GICv3", "Using split EOI/Deactivate mode\n");

	gic_data.node = node;
	gic_data.dist_base = dist_base;
	gic_data.redist_regions = rdist_regs;
	gic_data.nr_redist_regions = nr_redist_regions;
	gic_data.redist_stride = redist_stride;

	gicv3_enable_quirks();

	/*
	 * Find out how many interrupts are supported.
	 * The GIC only supports up to 1020 interrupt sources (SGI+PPI+SPI)
	 */
	typer = gic_readl(gic_data.dist_base + GICD_TYPER);
	gic_data.rdists.id_bits = GICD_TYPER_ID_BITS(typer);
	gic_irqs = GICD_TYPER_IRQS(typer);
	if (gic_irqs > 1020)
		gic_irqs = 1020;
	gic_data.irq_nr = gic_irqs;
	gic_data.domain_irq_nr = (gic_data.irq_nr < CONFIG_HOST_IRQ_COUNT) ?
				 gic_data.irq_nr : CONFIG_HOST_IRQ_COUNT;


	gic_data.domain = vmm_host_irqdomain_add(node, 0x0,
						 gic_data.domain_irq_nr,
						 &gic_ops, &gic_data);
	if (!gic_data.domain) {
		return VMM_EFAIL;
	}

#if 0 /* TODO: */
	if (IS_ENABLED(CONFIG_ARM_GIC_V3_ITS) && gic_dist_supports_lpis() &&
	    node) /* Temp hack to prevent ITS init for ACPI */
		its_init(node, &gic_data.rdists, gic_data.domain);
#endif

	vmm_host_irq_set_active_callback(gic_active_irq);

	gic_dist_init();
	gic_cpu_init();

	return VMM_OK;
}

static int __init gic_validate_dist_version(void *dist_base)
{
	u32 reg = gic_readl(dist_base + GICD_PIDR2) & GIC_PIDR2_ARCH_MASK;

	if (reg != GIC_PIDR2_ARCH_GICv3 && reg != GIC_PIDR2_ARCH_GICv4)
		return VMM_ENODEV;

	return 0;
}

static int __cpuinit gic_of_init(struct vmm_devtree_node *node)
{
	virtual_addr_t va;
	void *dist_base;
	struct redist_region *rdist_regs;
	u64 redist_stride;
	u32 nr_redist_regions;
	int err, i;

	if (!vmm_smp_is_bootcpu()) {
		gic_cpu_init();
		return VMM_OK;
	}

	if (WARN_ON(!node)) {
		return VMM_ENODEV;
	}

	err = vmm_devtree_request_regmap(node, &va, 0, "GICv3 Dist");
	if (err) {
		vmm_lerror("GICv3", "%s: unable to map gic dist regs\n",
			   node->name);
		return err;
	}
	dist_base = (void *)va;

	err = gic_validate_dist_version(dist_base);
	if (err) {
		vmm_lerror("GICv3", "%s: no distributor detected\n",
			   node->name);
		goto out_unmap_dist;
	}

	if (vmm_devtree_read_u32(node, "#redistributor-regions",
				 &nr_redist_regions))
		nr_redist_regions = 1;

	rdist_regs = vmm_zalloc(sizeof(*rdist_regs) * nr_redist_regions);
	if (!rdist_regs) {
		err = VMM_ENOMEM;
		goto out_unmap_dist;
	}

	for (i = 0; i < nr_redist_regions; i++) {
		char str[16];
		physical_addr_t pa;

		vmm_snprintf(str, sizeof(str), "GICv3 Redist%d", i);

		err = vmm_devtree_regaddr(node, &pa, 1 + i);
		if (err) {
			vmm_lerror("GICv3",
				   "%s: unable to get address of %s regs\n",
				   node->name, str);
			goto out_unmap_rdist;
		}

		err = vmm_devtree_request_regmap(node, &va, 1 + i, str);
		if (err) {
			vmm_lerror("GICv3",
				   "%s: unable to map %s regs\n",
				   node->name, str);
			goto out_unmap_rdist;
		}

		rdist_regs[i].redist_base = (void *)va;
		rdist_regs[i].phys_base = pa;
	}

	if (vmm_devtree_read_u64(node, "redistributor-stride",
				 &redist_stride))
		redist_stride = 0;

	err = gic_init_bases(dist_base, rdist_regs, nr_redist_regions,
			     redist_stride, node);
	if (err)
		goto out_unmap_rdist;

	return VMM_OK;

out_unmap_rdist:
	for (i = 0; i < nr_redist_regions; i++) {
		if (!rdist_regs[i].redist_base)
			continue;
		vmm_devtree_regunmap_release(node,
			(virtual_addr_t)rdist_regs[i].redist_base, 1 + i);
		rdist_regs[i].redist_base = NULL;
	}
out_unmap_dist:
	vmm_devtree_regunmap_release(node, (virtual_addr_t)dist_base, 0);
	return err;
}

VMM_HOST_IRQ_INIT_DECLARE(gic_v3, "arm,gic-v3", gic_of_init);
