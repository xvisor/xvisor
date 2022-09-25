/**
 * Copyright (c) 2022 Anup Patel.
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
 * @file irq-riscv-imsic.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief RISC-V Incoming Message Signaled Interrupt Controller (IMSIC) driver
 */

#include <vmm_error.h>
#include <vmm_limits.h>
#include <vmm_compiler.h>
#include <vmm_stdio.h>
#include <vmm_smp.h>
#include <vmm_cpuhp.h>
#include <vmm_heap.h>
#include <vmm_percpu.h>
#include <vmm_spinlocks.h>
#include <vmm_resource.h>
#include <vmm_modules.h>
#include <vmm_msi.h>
#include <vmm_devtree.h>
#include <vmm_devdrv.h>
#include <vmm_host_io.h>
#include <vmm_host_aspace.h>
#include <vmm_host_irq.h>
#include <vmm_host_irqdomain.h>
#include <drv/irqchip/riscv-imsic.h>
#include <riscv_encoding.h>
#include <cpu_hwcap.h>

#define IMSIC_DISABLE_EIDELIVERY	0
#define IMSIC_ENABLE_EIDELIVERY		1
#define IMSIC_DISABLE_EITHRESHOLD	1
#define IMSIC_ENABLE_EITHRESHOLD	0

#define imsic_csr_write(__c, __v)	\
do {					\
	csr_write(CSR_SISELECT, __c);	\
	csr_write(CSR_SIREG, __v);	\
} while (0)

#define imsic_csr_read(__c)		\
({					\
	unsigned long __v;		\
	csr_write(CSR_SISELECT, __c);	\
	__v = csr_read(CSR_SIREG);	\
	__v;				\
})

#define imsic_csr_set(__c, __v)		\
do {					\
	csr_write(CSR_SISELECT, __c);	\
	csr_set(CSR_SIREG, __v);	\
} while (0)

#define imsic_csr_clear(__c, __v)	\
do {					\
	csr_write(CSR_SISELECT, __c);	\
	csr_clear(CSR_SIREG, __v);	\
} while (0)

struct imsic_mmio {
	physical_addr_t pa;
	void *va;
	physical_addr_t size;
};

struct imsic_priv {
	/* Global configuration common for all HARTs */
	struct imsic_global_config global;

	/* MMIO regions */
	u32 num_mmios;
	struct imsic_mmio *mmios;

	/* Global state of interrupt identities */
	vmm_spinlock_t ids_lock;
	unsigned long *ids_used_bimap;
	unsigned long *ids_enabled_bimap;
	unsigned int *ids_target_cpu;

	/* Mask for connected CPUs */
	struct vmm_cpumask lmask;

	/* IPI domain */
	bool slow_ipi;
	u32 ipi_id;
	u32 ipi_lsync_id;
	struct vmm_host_irqdomain *ipi_domain;

	/* IRQ domains */
	struct vmm_host_irqdomain *base_domain;
	struct vmm_msi_domain *plat_domain;
};

struct imsic_handler {
	/* Local configuration for given HART */
	struct imsic_local_config local;

	/* Pointer to private context */
	struct imsic_priv *priv;
};

static bool imsic_init_done;

static int imsic_parent_irq;
static DEFINE_PER_CPU(struct imsic_handler, imsic_handlers);

const struct imsic_global_config *imsic_get_global_config(void)
{
	struct imsic_handler *handler = &this_cpu(imsic_handlers);

	if (!handler || !handler->priv)
		return NULL;

	return &handler->priv->global;
}
VMM_EXPORT_SYMBOL_GPL(imsic_get_global_config);

const struct imsic_local_config *imsic_get_local_config(unsigned int cpu)
{
	struct imsic_handler *handler = &per_cpu(imsic_handlers, cpu);

	if (!handler || !handler->priv)
		return NULL;

	return &handler->local;
}
VMM_EXPORT_SYMBOL_GPL(imsic_get_local_config);

static int imsic_cpu_page_phys(unsigned int cpu,
			       unsigned int guest_index,
			       physical_addr_t *out_msi_pa)
{
	struct imsic_handler *handler = &per_cpu(imsic_handlers, cpu);
	struct imsic_global_config *global;
	struct imsic_local_config *local;

	if (!handler || !handler->priv)
		return VMM_ENODEV;
	local = &handler->local;
	global = &handler->priv->global;

	if (BIT(global->guest_index_bits) <= guest_index)
		return VMM_EINVALID;

	if (out_msi_pa)
		*out_msi_pa = local->msi_pa +
			      (guest_index * IMSIC_MMIO_PAGE_SZ);

	return 0;
}

static int imsic_get_cpu(struct imsic_priv *priv,
			 const struct vmm_cpumask *mask_val, bool force,
			 unsigned int *out_target_cpu)
{
	struct vmm_cpumask amask;
	unsigned int cpu;

	vmm_cpumask_and(&amask, &priv->lmask, mask_val);

	if (force)
		cpu = vmm_cpumask_first(&amask);
	else
		cpu = vmm_cpumask_any_and(&amask, cpu_online_mask);

	if (cpu >= vmm_cpu_count)
		return VMM_EINVALID;

	if (out_target_cpu)
		*out_target_cpu = cpu;

	return 0;
}

static int imsic_get_cpu_msi_msg(unsigned int cpu, unsigned int id,
				 struct vmm_msi_msg *msg)
{
	physical_addr_t msi_addr;
	int err;

	err = imsic_cpu_page_phys(cpu, 0, &msi_addr);
	if (err)
		return err;

	msg->address_hi = ((u64)msi_addr) >> 32;
	msg->address_lo = ((u64)msi_addr) & 0xFFFFFFFF;
	msg->data = id;

	return err;
}

static void imsic_id_set_target(struct imsic_priv *priv,
				 unsigned int id, unsigned int target_cpu)
{
	irq_flags_t flags;

	vmm_spin_lock_irqsave_lite(&priv->ids_lock, flags);
	priv->ids_target_cpu[id] = target_cpu;
	vmm_spin_unlock_irqrestore_lite(&priv->ids_lock, flags);
}

static unsigned int imsic_id_get_target(struct imsic_priv *priv,
					unsigned int id)
{
	unsigned int ret;
	irq_flags_t flags;

	vmm_spin_lock_irqsave_lite(&priv->ids_lock, flags);
	ret = priv->ids_target_cpu[id];
	vmm_spin_unlock_irqrestore_lite(&priv->ids_lock, flags);

	return ret;
}

static inline void __imsic_id_enable(unsigned int id)
{
	csr_write(CSR_SSETEIENUM, id);
}

static inline void __imsic_id_disable(unsigned int id)
{
	csr_write(CSR_SCLREIENUM, id);
}

static void __imsic_eix_update(unsigned long base_id,
			       unsigned long num_id, bool pend, bool val)
{
	irq_flags_t flags;
	unsigned long i, isel, ireg;
	unsigned long id = base_id, last_id = base_id + num_id;

	while (id < last_id) {
		isel = id / BITS_PER_LONG;
		isel *= BITS_PER_LONG / IMSIC_EIPx_BITS;
		isel += (pend) ? IMSIC_EIP0 : IMSIC_EIE0;

		ireg = 0;
		for (i = id & (__riscv_xlen - 1);
		     (id < last_id) && (i < __riscv_xlen); i++) {
			ireg |= BIT(i);
			id++;
		}

		/*
		 * The IMSIC EIEx and EIPx registers are indirectly
		 * accessed via using ISELECT and IREG CSRs so we
		 * save/restore local IRQ to ensure that we don't
		 * get preempted while accessing IMSIC registers.
		 */
		arch_cpu_irq_save(flags);
		if (val)
			imsic_csr_set(isel, ireg);
		else
			imsic_csr_clear(isel, ireg);
		arch_cpu_irq_restore(flags);
	}
}

#define __imsic_id_enable(__id)		\
	__imsic_eix_update((__id), 1, false, true)
#define __imsic_id_disable(__id)	\
	__imsic_eix_update((__id), 1, false, false)

#ifdef CONFIG_SMP
static void __imsic_id_smp_sync(struct imsic_priv *priv)
{
	struct imsic_handler *handler;
	struct vmm_cpumask amask;
	int cpu;

	vmm_cpumask_and(&amask, &priv->lmask, cpu_online_mask);
	for_each_cpu(cpu, &amask) {
		if (cpu == vmm_smp_processor_id())
			continue;

		handler = &per_cpu(imsic_handlers, cpu);
		if (!handler || !handler->priv || !handler->local.msi_va) {
			vmm_lwarning("imsic",
				     "CPU%d: handler not initialized\n", cpu);
			continue;
		}

		vmm_writel(handler->priv->ipi_lsync_id, handler->local.msi_va);
	}
}
#else
#define __imsic_id_smp_sync(__priv)
#endif

static void imsic_id_enable(struct imsic_priv *priv, unsigned int id)
{
	irq_flags_t flags;

	vmm_spin_lock_irqsave_lite(&priv->ids_lock, flags);
	bitmap_set(priv->ids_enabled_bimap, id, 1);
	__imsic_id_enable(id);
	vmm_spin_unlock_irqrestore_lite(&priv->ids_lock, flags);

	__imsic_id_smp_sync(priv);
}

static void imsic_id_disable(struct imsic_priv *priv, unsigned int id)
{
	irq_flags_t flags;

	vmm_spin_lock_irqsave_lite(&priv->ids_lock, flags);
	bitmap_set(priv->ids_enabled_bimap, id, 1);
	__imsic_id_disable(id);
	vmm_spin_unlock_irqrestore_lite(&priv->ids_lock, flags);

	__imsic_id_smp_sync(priv);
}

static void imsic_ids_local_sync(struct imsic_priv *priv)
{
	int i;
	irq_flags_t flags;

	vmm_spin_lock_irqsave_lite(&priv->ids_lock, flags);
	for (i = 1; i <= priv->global.nr_ids; i++) {
		if (priv->ipi_id == i || priv->ipi_lsync_id == i)
			continue;

		if (test_bit(i, priv->ids_enabled_bimap))
			__imsic_id_enable(i);
		else
			__imsic_id_disable(i);
	}
	vmm_spin_unlock_irqrestore_lite(&priv->ids_lock, flags);
}

static void imsic_ids_local_delivery(struct imsic_priv *priv, bool enable)
{
	if (enable) {
		imsic_csr_write(IMSIC_EITHRESHOLD, IMSIC_ENABLE_EITHRESHOLD);
		imsic_csr_write(IMSIC_EIDELIVERY, IMSIC_ENABLE_EIDELIVERY);
	} else {
		imsic_csr_write(IMSIC_EIDELIVERY, IMSIC_DISABLE_EIDELIVERY);
		imsic_csr_write(IMSIC_EITHRESHOLD, IMSIC_DISABLE_EITHRESHOLD);
	}
}

static int imsic_ids_alloc(struct imsic_priv *priv,
			   unsigned int max_id, unsigned int order)
{
	int ret;
	irq_flags_t flags;

	if ((priv->global.nr_ids < max_id) ||
	    (max_id < BIT(order)))
		return VMM_EINVALID;

	vmm_spin_lock_irqsave_lite(&priv->ids_lock, flags);
	ret = bitmap_find_free_region(priv->ids_used_bimap,
				      max_id + 1, order);
	vmm_spin_unlock_irqrestore_lite(&priv->ids_lock, flags);

	return ret;
}

static void imsic_ids_free(struct imsic_priv *priv, unsigned int base_id,
			   unsigned int order)
{
	irq_flags_t flags;

	vmm_spin_lock_irqsave_lite(&priv->ids_lock, flags);
	bitmap_release_region(priv->ids_used_bimap, base_id, order);
	vmm_spin_unlock_irqrestore_lite(&priv->ids_lock, flags);
}

static int __init imsic_ids_init(struct imsic_priv *priv)
{
	int i;
	struct imsic_global_config *global = &priv->global;

	INIT_SPIN_LOCK(&priv->ids_lock);

	/* Allocate used bitmap */
	priv->ids_used_bimap = vmm_calloc(BITS_TO_LONGS(global->nr_ids + 1),
					  sizeof(unsigned long));
	if (!priv->ids_used_bimap) {
		return VMM_ENOMEM;
	}

	/* Allocate enabled bitmap */
	priv->ids_enabled_bimap = vmm_calloc(BITS_TO_LONGS(global->nr_ids + 1),
					     sizeof(unsigned long));
	if (!priv->ids_enabled_bimap) {
		vmm_free(priv->ids_used_bimap);
		return VMM_ENOMEM;
	}

	/* Allocate target CPU array */
	priv->ids_target_cpu = vmm_calloc(global->nr_ids + 1,
					  sizeof(unsigned int));
	if (!priv->ids_target_cpu) {
		vmm_free(priv->ids_enabled_bimap);
		vmm_free(priv->ids_used_bimap);
		return VMM_ENOMEM;
	}
	for (i = 0; i <= global->nr_ids; i++)
		priv->ids_target_cpu[i] = UINT_MAX;

	/* Reserve ID#0 because it is special and never implemented */
	bitmap_set(priv->ids_used_bimap, 0, 1);

	return 0;
}

static void __init imsic_ids_cleanup(struct imsic_priv *priv)
{
	vmm_free(priv->ids_target_cpu);
	vmm_free(priv->ids_enabled_bimap);
	vmm_free(priv->ids_used_bimap);
}

#ifdef CONFIG_SMP
static void imsic_ipi_mask(struct vmm_host_irq *d)
{
	struct imsic_priv *priv = vmm_host_irq_get_chip_data(d);

	__imsic_id_disable(priv->ipi_id);
}

static void imsic_ipi_unmask(struct vmm_host_irq *d)
{
	struct imsic_priv *priv = vmm_host_irq_get_chip_data(d);

	__imsic_id_enable(priv->ipi_id);
}

static void imsic_ipi_send_mask(struct vmm_host_irq *d,
				const struct vmm_cpumask *mask)
{
	int cpu;
	struct imsic_handler *handler;

	for_each_cpu(cpu, mask) {
		handler = &per_cpu(imsic_handlers, cpu);
		if (!handler || !handler->priv || !handler->local.msi_va) {
			vmm_lwarning("imsic",
				     "CPU%d: handler not initialized\n", cpu);
			continue;
		}

		vmm_writel(handler->priv->ipi_id, handler->local.msi_va);
	}
}

static struct vmm_host_irq_chip imsic_ipi_chip = {
	.name		= "riscv-imsic-ipi",
	.irq_mask	= imsic_ipi_mask,
	.irq_unmask	= imsic_ipi_unmask,
	.irq_raise	= imsic_ipi_send_mask,
};

static int imsic_ipi_domain_map(struct vmm_host_irqdomain *dom,
				unsigned int hirq, unsigned int hwirq)
{
	struct imsic_priv *priv = dom->host_data;

	vmm_host_irq_mark_per_cpu(hirq);
	vmm_host_irq_mark_ipi(hirq);
	vmm_host_irq_set_chip(hirq, &imsic_ipi_chip);
	vmm_host_irq_set_chip_data(hirq, priv);
	vmm_host_irq_set_handler(hirq, vmm_handle_percpu_irq);

	return VMM_OK;
}

static const struct vmm_host_irqdomain_ops imsic_ipi_domain_ops = {
	.map = imsic_ipi_domain_map,
};

static void imsic_ipi_enable(struct imsic_priv *priv)
{
	__imsic_id_enable(priv->ipi_id);
	__imsic_id_enable(priv->ipi_lsync_id);
}

static void imsic_ipi_disable(struct imsic_priv *priv)
{
	__imsic_id_disable(priv->ipi_lsync_id);
	__imsic_id_disable(priv->ipi_id);
}

static int __init imsic_ipi_domain_init(struct imsic_priv *priv)
{
	int virq;

	/* Skip IPI setup if IPIs are slow */
	if (priv->slow_ipi)
		goto skip_ipi;

	/* Allocate interrupt identity for IPIs */
	virq = imsic_ids_alloc(priv, priv->global.nr_ids, get_count_order(1));
	if (virq < 0)
		return virq;
	priv->ipi_id = virq;

	/* Reserve interrupt identity for IPI */
	bitmap_set(priv->ids_used_bimap, priv->ipi_id, 1);

	/* Create IMSIC IPI domain */
	priv->ipi_domain = vmm_host_irqdomain_add(NULL, BITS_PER_LONG * 2,
						  1, &imsic_ipi_domain_ops,
						  priv);
	if (!priv->ipi_domain) {
		imsic_ids_free(priv, priv->ipi_id, get_count_order(1));
		return VMM_ENOMEM;
	}

	/* Pre-create IPI mappings */
	virq = vmm_host_irqdomain_create_mapping(priv->ipi_domain, 0);
	if (virq < 0) {
		vmm_lerror("imsic", "failed to create IPI mapping\n");
		vmm_host_irqdomain_remove(priv->ipi_domain);
		imsic_ids_free(priv, priv->ipi_id, get_count_order(1));
		return virq;
	}

skip_ipi:
	/* Allocate interrupt identity for local enable/disable sync */
	virq = imsic_ids_alloc(priv, priv->global.nr_ids, get_count_order(1));
	if (virq < 0) {
		vmm_host_irqdomain_remove(priv->ipi_domain);
		imsic_ids_free(priv, priv->ipi_id, get_count_order(1));
		return virq;
	}
	priv->ipi_lsync_id = virq;

	return VMM_OK;
}

static void __init imsic_ipi_domain_cleanup(struct imsic_priv *priv)
{
	imsic_ids_free(priv, priv->ipi_lsync_id, get_count_order(1));
	vmm_host_irqdomain_remove(priv->ipi_domain);
	imsic_ids_free(priv, priv->ipi_id, get_count_order(1));
}
#else
static void imsic_ipi_enable(struct imsic_priv *priv)
{
}

static void imsic_ipi_disable(struct imsic_priv *priv)
{
}

static int __init imsic_ipi_domain_init(struct imsic_priv *priv)
{
	/* Clear the IPI ids because we are not using IPIs */
	priv->ipi_id = 0;
	priv->ipi_lsync_id = 0;
	return VMM_OK;
}

static void __init imsic_ipi_domain_cleanup(struct imsic_priv *priv)
{
}
#endif

static void imsic_irq_mask(struct vmm_host_irq *d)
{
	imsic_id_disable(vmm_host_irq_get_chip_data(d), d->hwirq);
}

static void imsic_irq_unmask(struct vmm_host_irq *d)
{
	imsic_id_enable(vmm_host_irq_get_chip_data(d), d->hwirq);
}

static void imsic_irq_compose_msi_msg(struct vmm_host_irq *d,
				      struct vmm_msi_msg *msg)
{
	struct imsic_priv *priv = vmm_host_irq_get_chip_data(d);
	unsigned int cpu;
	int err;

	cpu = imsic_id_get_target(priv, d->hwirq);
	WARN_ON(cpu == UINT_MAX);

	err = imsic_get_cpu_msi_msg(cpu, d->hwirq, msg);
	WARN_ON(err);
}

#ifdef CONFIG_SMP
static int imsic_irq_set_affinity(struct vmm_host_irq *d,
				  const struct vmm_cpumask *mask_val,
				  bool force)
{
	struct imsic_priv *priv = vmm_host_irq_get_chip_data(d);
	unsigned int target_cpu;
	int rc;

	rc = imsic_get_cpu(priv, mask_val, force, &target_cpu);
	if (rc)
		return rc;

	imsic_id_set_target(priv, d->hwirq, target_cpu);

	vmm_msi_domain_write_msg(d);

	return VMM_OK;
}
#endif

static struct vmm_host_irq_chip imsic_irq_base_chip = {
	.name			= "riscv-imsic",
	.irq_mask		= imsic_irq_mask,
	.irq_unmask		= imsic_irq_unmask,
#ifdef CONFIG_SMP
	.irq_set_affinity	= imsic_irq_set_affinity,
#endif
	.irq_compose_msi_msg	= imsic_irq_compose_msi_msg,
};

static int imsic_irq_domain_map(struct vmm_host_irqdomain *dom,
				unsigned int hirq, unsigned int hwirq)
{
	struct imsic_priv *priv = dom->host_data;

	vmm_host_irq_set_chip(hirq, &imsic_irq_base_chip);
	vmm_host_irq_set_chip_data(hirq, priv);
	vmm_host_irq_set_handler(hirq, vmm_handle_simple_irq);

	return VMM_OK;
}

static int imsic_irq_domain_alloc(struct vmm_host_irqdomain *dom,
				  unsigned int nr_irqs, void *arg)
{
	struct imsic_priv *priv = dom->host_data;
	physical_addr_t msi_addr;
	int i, hwirq, err = 0;
	unsigned int cpu;

	err = imsic_get_cpu(priv, &priv->lmask, FALSE, &cpu);
	if (err)
		return err;

	err = imsic_cpu_page_phys(cpu, 0, &msi_addr);
	if (err)
		return err;

	hwirq = imsic_ids_alloc(priv, priv->global.nr_ids,
				get_count_order(nr_irqs));
	if (hwirq < 0)
		return hwirq;

	/* TODO: Notify IOMMU ?? */

	for (i = 0; i < nr_irqs; i++) {
		imsic_id_set_target(priv, hwirq + i, cpu);
	}

	return hwirq;
}

static void imsic_irq_domain_free(struct vmm_host_irqdomain *dom,
				  unsigned int hwirq, unsigned int nr_irqs)
{
	struct imsic_priv *priv = dom->host_data;

	imsic_ids_free(priv, hwirq, get_count_order(nr_irqs));
}

static const struct vmm_host_irqdomain_ops imsic_base_domain_ops = {
	.map		= imsic_irq_domain_map,
	.alloc		= imsic_irq_domain_alloc,
	.free		= imsic_irq_domain_free,
};

static struct vmm_msi_domain_ops imsic_plat_domain_ops = {
};

static int __init imsic_irq_domains_init(struct imsic_priv *priv,
					 struct vmm_devtree_node *node)
{
	/* Create Base IRQ domain */
	priv->base_domain = vmm_host_irqdomain_add(node, -1,
						   priv->global.nr_ids + 1,
						   &imsic_base_domain_ops,
						   priv);
	if (!priv->base_domain) {
		vmm_lerror("imsic", "Failed to create IMSIC base domain\n");
		return VMM_ENOMEM;
	}

	priv->plat_domain = vmm_platform_msi_create_domain(node,
						&imsic_plat_domain_ops,
						priv->base_domain,
						VMM_MSI_FLAG_USE_DEF_DOM_OPS,
						priv);
	if (!priv->plat_domain) {
		vmm_lerror("imsic",
			   "Failed to create IMSIC platform MSI domain\n");
		vmm_host_irqdomain_remove(priv->base_domain);
		return VMM_ENOMEM;
	}

	/* TODO: Create PCI MSI domain */

	return VMM_OK;
}

/*
 * To handle an interrupt, we read the TOPEI CSR and write zero in one
 * instruction. If TOPEI CSR is non-zero then we translate TOPEI.ID to
 * Xvisor interrupt number and let Xvisor IRQ subsystem handle it.
 */
static vmm_irq_return_t imsic_handle_irq(int irq, void *dev)
{
	struct imsic_handler *handler = dev;
	struct imsic_priv *priv = handler->priv;
	struct vmm_host_irqdomain *domain;
	u32 hwirq, base_hwirq, hirq;
	bool have_irq = FALSE;

	WARN_ON(!handler->priv);

	while ((hwirq = csr_swap(CSR_STOPEI, 0))) {
		hwirq = hwirq >> TOPEI_ID_SHIFT;
		domain = priv->base_domain;
		base_hwirq = 0;

		if (hwirq == priv->ipi_id) {
			domain = priv->ipi_domain;
			base_hwirq = hwirq;
		} else if (hwirq == priv->ipi_lsync_id) {
			imsic_ids_local_sync(priv);
			continue;
		}

		hirq = vmm_host_irqdomain_find_mapping(domain,
						       hwirq - base_hwirq);
		vmm_host_generic_irq_exec(hirq);
		have_irq = TRUE;
	}

	return (have_irq) ? VMM_IRQ_HANDLED : VMM_IRQ_NONE;
}

static int imsic_dying_cpu(struct vmm_cpuhp_notify *cpuhp, u32 cpu)
{
	struct imsic_handler *handler = &this_cpu(imsic_handlers);
	struct imsic_priv *priv = handler->priv;

	/* No need to disable per-CPU parent interrupt */

	/* Locally disable interrupt delivery */
	imsic_ids_local_delivery(priv, false);

	/* Disable IPIs */
	imsic_ipi_disable(priv);

	return VMM_OK;
}

static int imsic_starting_cpu(struct vmm_cpuhp_notify *cpuhp, u32 cpu)
{
	struct imsic_handler *handler = &this_cpu(imsic_handlers);
	struct imsic_priv *priv = handler->priv;

	/* Enable per-CPU parent interrupt */
	if (imsic_parent_irq)
		vmm_host_irq_register(imsic_parent_irq, "riscv-imsic",
				      imsic_handle_irq, handler);
	else {
		vmm_lwarning("imsic",
			     "CPU%d: parent irq not available\n", cpu);
	}

	/* Enable IPIs */
	imsic_ipi_enable(priv);

	/*
	 * Interrupts identities might have been enabled/disabled while
	 * this CPU was not running so sync-up local enable/disable state.
	 */
	imsic_ids_local_sync(priv);

	/* Locally enable interrupt delivery */
	imsic_ids_local_delivery(priv, true);

	return VMM_OK;
}

static struct vmm_cpuhp_notify imsic_cpuhp = {
	.name = "IMSIC",
	.state = VMM_CPUHP_STATE_HOST_IRQ,
	.startup = imsic_starting_cpu,
	.teardown = imsic_dying_cpu,
};

static int __init imsic_init(struct vmm_devtree_node *node)
{
	int rc;
	struct imsic_mmio *mmio;
	struct imsic_priv *priv;
	virtual_addr_t base_virt;
	physical_addr_t base_addr;
	struct imsic_handler *handler;
	struct imsic_global_config *global;
	u32 i, tmp, nr_parent_irqs, nr_handlers = 0;

	if (imsic_init_done) {
		vmm_lerror(node->name,
			   "already initialized hence ignoring\n");
		return VMM_ENODEV;
	}

	if (!riscv_isa_extension_available(NULL, SxAIA)) {
		vmm_lerror(node->name, "AIA support not available\n");
		return VMM_ENODEV;
	}

	priv = vmm_zalloc(sizeof(*priv));
	if (!priv) {
		return VMM_ENOMEM;
	}
	global = &priv->global;

	/* Find number of parent interrupts */
	nr_parent_irqs = vmm_devtree_irq_count(node);
	if (!nr_parent_irqs) {
		vmm_lerror(node->name, "no parent irqs available\n");
		return VMM_EINVALID;
	}

	/* Find number of guest index bits in MSI address */
	rc = vmm_devtree_read_u32(node, "riscv,guest-index-bits",
				  &global->guest_index_bits);
	if (rc)
		global->guest_index_bits = 0;
	tmp = BITS_PER_LONG - IMSIC_MMIO_PAGE_SHIFT;
	if (tmp < global->guest_index_bits) {
		vmm_lerror(node->name, "guest index bits too big\n");
		return VMM_EINVALID;
	}

	/* Find number of HART index bits */
	rc = vmm_devtree_read_u32(node, "riscv,hart-index-bits",
				  &global->hart_index_bits);
	if (rc) {
		/* Assume default value */
		global->hart_index_bits = __fls(nr_parent_irqs);
		if (BIT(global->hart_index_bits) < nr_parent_irqs)
			global->hart_index_bits++;
	}
	tmp = BITS_PER_LONG - IMSIC_MMIO_PAGE_SHIFT -
	      global->guest_index_bits;
	if (tmp < global->hart_index_bits) {
		vmm_lerror(node->name, "HART index bits too big\n");
		return VMM_EINVALID;
	}

	/* Find number of group index bits */
	rc = vmm_devtree_read_u32(node, "riscv,group-index-bits",
				  &global->group_index_bits);
	if (rc)
		global->group_index_bits = 0;
	tmp = BITS_PER_LONG - IMSIC_MMIO_PAGE_SHIFT -
	      global->guest_index_bits - global->hart_index_bits;
	if (tmp < global->group_index_bits) {
		vmm_lerror(node->name, "group index bits too big\n");
		return VMM_EINVALID;
	}

	/* Find first bit position of group index */
	tmp = IMSIC_MMIO_PAGE_SHIFT * 2;
	rc = vmm_devtree_read_u32(node, "riscv,group-index-shift",
				  &global->group_index_shift);
	if (rc)
		global->group_index_shift = tmp;
	if (global->group_index_shift < tmp) {
		vmm_lerror(node->name, "group index shift too small\n");
		return VMM_EINVALID;
	}
	tmp = global->group_index_bits + global->group_index_shift - 1;
	if (tmp >= BITS_PER_LONG) {
		vmm_lerror(node->name, "group index shift too big\n");
		return VMM_EINVALID;
	}

	/* Find number of interrupt identities */
	rc = vmm_devtree_read_u32(node, "riscv,num-ids", &global->nr_ids);
	if (rc) {
		vmm_lerror(node->name,
			   "number of interrupt identities not found\n");
		return rc;
	}
	if ((global->nr_ids < IMSIC_MIN_ID) ||
	    (global->nr_ids >= IMSIC_MAX_ID) ||
	    ((global->nr_ids & IMSIC_MIN_ID) != IMSIC_MIN_ID)) {
		vmm_lerror(node->name,
			   "invalid number of interrupt identities\n");
		return VMM_EINVALID;
	}

	/* Check if IPIs are slow */
	priv->slow_ipi = vmm_devtree_getattr(node, "riscv,slow-ipi") ?
			 TRUE : FALSE;

	/* Compute base address */
	rc = vmm_devtree_regaddr(node, &global->base_addr, 0);
	if (rc) {
		vmm_lerror(node->name, "first MMIO resource not found\n");
		return rc;
	}
	global->base_addr &= ~(BIT(global->guest_index_bits +
				   global->hart_index_bits +
				   IMSIC_MMIO_PAGE_SHIFT) - 1);
	global->base_addr &= ~((BIT(global->group_index_bits) - 1) <<
			       global->group_index_shift);

	/* Find number of MMIO register sets */
	while (!vmm_devtree_regaddr(node, &base_addr, priv->num_mmios))
		priv->num_mmios++;

	/* Allocate MMIO register sets */
	priv->mmios = vmm_calloc(priv->num_mmios, sizeof(*mmio));
	if (!priv->mmios) {
		rc = VMM_ENOMEM;
		goto out_free_priv;
	}

	/* Parse and map MMIO register sets */
	for (i = 0; i < priv->num_mmios; i++) {
		mmio = &priv->mmios[i];

		rc = vmm_devtree_regaddr(node, &mmio->pa, i);
		if (rc) {
			vmm_lerror(node->name,
				   "unable to parse MMIO addr of regset %d\n",
				   i);
			goto out_iounmap;
		}

		rc = vmm_devtree_regsize(node, &mmio->size, i);
		if (rc) {
			vmm_lerror(node->name,
				   "unable to parse MMIO size of regset %d\n",
				   i);
			goto out_iounmap;
		}

		base_addr = mmio->pa;
		base_addr &= ~(BIT(global->guest_index_bits +
				   global->hart_index_bits +
				   IMSIC_MMIO_PAGE_SHIFT) - 1);
		base_addr &= ~((BIT(global->group_index_bits) - 1) <<
			       global->group_index_shift);
		if (base_addr != global->base_addr) {
			rc = VMM_EINVALID;
			vmm_lerror(node->name,
				   "address mismatch for regset %d\n",
				   i);
			goto out_iounmap;
		}

		tmp = BIT(global->guest_index_bits) - 1;
		if ((mmio->size / IMSIC_MMIO_PAGE_SZ) & tmp) {
			rc = VMM_EINVALID;
			vmm_lerror(node->name,
				   "size mismatch for regset %d\n",
				   i);
			goto out_iounmap;
		}

		rc = vmm_devtree_request_regmap(node, &base_virt,
						i, "RISC-V IMSIC");
		if (rc) {
			vmm_lerror(node->name,
				   "unable to map MMIO regset %d\n",
				   i);
			goto out_iounmap;
		}
		mmio->va = (void *)base_virt;
	}

	/* Initialize interrupt identity management */
	rc = imsic_ids_init(priv);
	if (rc) {
		vmm_lerror(node->name,
			   "failed to initialize interrupt management\n");
		goto out_iounmap;
	}

	/* Configure handlers for target CPUs */
	for (i = 0; i < nr_parent_irqs; i++) {
		struct vmm_devtree_phandle_args parent;
		unsigned long reloff;
		u32 j, cpu, hartid;

		if (vmm_devtree_irq_parse_one(node, i, &parent)) {
			vmm_lwarning(node->name,
				     "failed to parse parent irq%d\n", i);
			continue;
		}

		/*
		 * Skip interrupt pages other than external interrupts for
		 * out privilege level.
		 */
		if (parent.args[0] != IRQ_S_EXT) {
			vmm_lwarning(node->name,
				     "invalid hwirq for parent irq%d\n", i);
			continue;
		}

		rc = riscv_node_to_hartid(parent.np->parent, &hartid);
		if (rc) {
			vmm_lwarning(node->name,
				     "hart ID for parent irq%d not found\n",
				     i);
			continue;
		}

		rc = vmm_smp_map_cpuid(hartid, &cpu);
		if (rc) {
			vmm_lwarning(node->name,
				     "invalid cpuid for parent irq%d\n", i);
			continue;
		}

		/* Find parent domain and map interrupt */
		if (!imsic_parent_irq &&
		    vmm_devtree_irqdomain_find(parent.np)) {
			imsic_parent_irq = vmm_devtree_irq_parse_map(node, i);
		}

		/* Find MMIO location of MSI page */
		mmio = NULL;
		reloff = i * BIT(global->guest_index_bits) *
			 IMSIC_MMIO_PAGE_SZ;
		for (j = 0; priv->num_mmios; j++) {
			if (reloff < priv->mmios[j].size) {
				mmio = &priv->mmios[j];
				break;
			}

			reloff -= priv->mmios[j].size;
		}
		if (!mmio) {
			vmm_lwarning(node->name,
				     "MMIO not found for parent irq%d\n", i);
			continue;
		}

		handler = &per_cpu(imsic_handlers, cpu);
		if (handler->priv) {
			vmm_lwarning(node->name,
				     "CPU%d handler already configured.\n",
				     cpu);
			goto done;
		}

		vmm_cpumask_set_cpu(cpu, &priv->lmask);
		handler->local.msi_pa = mmio->pa + reloff;
		handler->local.msi_va = mmio->va + reloff;
		handler->priv = priv;

done:
		nr_handlers++;
	}

	/* Initialize IPI domain */
	rc = imsic_ipi_domain_init(priv);
	if (rc) {
		vmm_lerror(node->name, "Failed to initialize IPI domain\n");
		goto out_ids_cleanup;
	}

	/* Initialize IRQ and MSI domains */
	rc = imsic_irq_domains_init(priv, node);
	if (rc) {
		vmm_lerror(node->name,
			   "Failed to initialize IRQ and MSI domains\n");
		goto out_ipi_domain_cleanup;
	}

	/* Setup cpuhp state */
	vmm_cpuhp_register(&imsic_cpuhp, TRUE);

	/*
	 * Only one IMSIC instance allowed in a platform for clean
	 * implementation of SMP IRQ affinity and per-CPU IPIs.
	 *
	 * This means on a multi-socket (or multi-die) platform we
	 * will have multiple MMIO regions for one IMSIC instance.
	 */
	imsic_init_done = true;

	vmm_init_printf("%s:  hart-index-bits: %d,  guest-index-bits: %d\n",
			node->name, global->hart_index_bits,
			global->guest_index_bits);
	vmm_init_printf("%s: group-index-bits: %d, group-index-shift: %d\n",
			node->name, global->group_index_bits,
			global->group_index_shift);
	vmm_init_printf("%s: mapped %d interrupts for %d CPUs at 0x%"PRIPADDR"\n",
			node->name, global->nr_ids, nr_handlers,
			global->base_addr);
	if (priv->ipi_lsync_id)
		vmm_init_printf("%s: enable/disable sync using interrupt %d\n",
				node->name, priv->ipi_lsync_id);
	if (priv->ipi_id)
		vmm_init_printf("%s: providing IPIs using interrupt %d\n",
				node->name, priv->ipi_id);

	return VMM_OK;

out_ipi_domain_cleanup:
	imsic_ipi_domain_cleanup(priv);
out_ids_cleanup:
	imsic_ids_cleanup(priv);
out_iounmap:
	for (i = 0; i < priv->num_mmios; i++) {
		if (priv->mmios[i].va) {
			vmm_devtree_regunmap_release(node,
				(virtual_addr_t)priv->mmios[i].va, i);
		}
	}
	vmm_free(priv->mmios);
out_free_priv:
	vmm_free(priv);
	return rc;
}

VMM_HOST_IRQ_INIT_DECLARE(riscvimsic, "riscv,imsics", imsic_init);
