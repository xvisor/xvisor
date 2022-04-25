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
 * @file irq-riscv-aplic.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Advanced Platform Level Interrupt Controller (APLIC) driver
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
#include <vmm_devres.h>
#include <vmm_host_io.h>
#include <vmm_host_aspace.h>
#include <vmm_host_irq.h>
#include <vmm_host_irqdomain.h>
#include <drv/irqchip/riscv-aplic.h>
#include <drv/irqchip/riscv-imsic.h>
#include <cpu_hwcap.h>

#define MODULE_DESC			"RISC-V APLIC Driver"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			aplic_init
#define	MODULE_EXIT			aplic_exit

#define APLIC_DEFAULT_PRIORITY		1
#define APLIC_DISABLE_IDELIVERY		0
#define APLIC_ENABLE_IDELIVERY		1
#define APLIC_DISABLE_ITHRESHOLD	1
#define APLIC_ENABLE_ITHRESHOLD		0

struct aplic_msi {
	unsigned int		hw_irq;
	unsigned int		parent_irq;
	physical_addr_t		msg_addr;
	u32			msg_data;
	struct aplic_priv	*priv;
};

struct aplic_msicfg {
	physical_addr_t		base_ppn;
	u32			hhxs;
	u32			hhxw;
	u32			lhxs;
	u32			lhxw;
};

struct aplic_idc {
	unsigned int		hart_index;
	void			*regs;
	struct aplic_priv	*priv;
};

struct aplic_priv {
	struct vmm_device	*dev;
	u32			nr_irqs;
	u32			nr_idcs;
	void 			*regs;
	struct vmm_host_irqdomain	*irqdomain;
	struct aplic_msi	*msis;
	struct aplic_msicfg	msicfg;
	struct vmm_cpumask	lmask;
};

static unsigned int aplic_idc_parent_irq;
static DEFINE_PER_CPU(struct aplic_idc, aplic_idcs);

static void aplic_irq_unmask(struct vmm_host_irq *d)
{
	struct aplic_priv *priv = vmm_host_irq_get_chip_data(d);

	vmm_writel(d->hwirq, priv->regs + APLIC_SETIENUM);
}

static void aplic_irq_mask(struct vmm_host_irq *d)
{
	struct aplic_priv *priv = vmm_host_irq_get_chip_data(d);

	vmm_writel(d->hwirq, priv->regs + APLIC_CLRIENUM);
}

static int aplic_set_type(struct vmm_host_irq *d, u32 type)
{
	u32 val = 0;
	void *sourcecfg;
	struct aplic_priv *priv = vmm_host_irq_get_chip_data(d);

	switch (type) {
	case VMM_IRQ_TYPE_NONE:
		val = APLIC_SOURCECFG_SM_INACTIVE;
		break;
	case VMM_IRQ_TYPE_LEVEL_LOW:
		val = APLIC_SOURCECFG_SM_LEVEL_LOW;
		break;
	case VMM_IRQ_TYPE_LEVEL_HIGH:
		val = APLIC_SOURCECFG_SM_LEVEL_HIGH;
		break;
	case VMM_IRQ_TYPE_EDGE_FALLING:
		val = APLIC_SOURCECFG_SM_EDGE_FALL;
		break;
	case VMM_IRQ_TYPE_EDGE_RISING:
		val = APLIC_SOURCECFG_SM_EDGE_RISE;
		break;
	default:
		return VMM_EINVALID;
	}

	sourcecfg = priv->regs + APLIC_SOURCECFG_BASE;
	sourcecfg += (d->hwirq - 1) * sizeof(u32);
	vmm_writel(val, sourcecfg);

	return 0;
}

#ifdef CONFIG_SMP
static int aplic_set_affinity(struct vmm_host_irq *d,
			      const struct vmm_cpumask *mask_val, bool force)
{
	struct aplic_priv *priv = vmm_host_irq_get_chip_data(d);
	struct aplic_idc *idc;
	struct aplic_msi *msi;
	unsigned int cpu, val;
	struct vmm_cpumask amask;
	void *target;

	vmm_cpumask_and(&amask, &priv->lmask, mask_val);

	if (force)
		cpu = vmm_cpumask_first(&amask);
	else
		cpu = vmm_cpumask_any_and(&amask, cpu_online_mask);

	if (cpu >= vmm_cpu_count)
		return VMM_EINVALID;

	if (priv->nr_idcs) {
		idc = &per_cpu(aplic_idcs, cpu);
		target = priv->regs + APLIC_TARGET_BASE;
		target += (d->hwirq - 1) * sizeof(u32);
		val = idc->hart_index & APLIC_TARGET_HART_IDX_MASK;
		val <<= APLIC_TARGET_HART_IDX_SHIFT;
		val |= APLIC_DEFAULT_PRIORITY;
		vmm_writel(val, target);
	} else {
		msi = &priv->msis[d->hwirq];
		return vmm_host_irq_set_affinity(msi->parent_irq,
						 vmm_cpumask_of(cpu), force);
	}

	return 0;
}
#endif

static struct vmm_host_irq_chip aplic_chip = {
	.name		= "riscv-aplic",
	.irq_mask	= aplic_irq_mask,
	.irq_unmask	= aplic_irq_unmask,
	.irq_set_type	= aplic_set_type,
#ifdef CONFIG_SMP
	.irq_set_affinity = aplic_set_affinity,
#endif
};

static int aplic_irqdomain_map(struct vmm_host_irqdomain *dom,
			       unsigned int hirq, unsigned int hwirq)
{
	struct aplic_priv *priv = dom->host_data;

	vmm_host_irq_set_chip(hirq, &aplic_chip);
	vmm_host_irq_set_chip_data(hirq, priv);
	vmm_host_irq_set_handler(hirq, vmm_handle_simple_irq);

	return VMM_OK;
}

static const struct vmm_host_irqdomain_ops aplic_irqdomain_ops = {
	.xlate = vmm_host_irqdomain_xlate_twocells,
	.map = aplic_irqdomain_map,
};

static void aplic_init_hw_irqs(struct aplic_priv *priv)
{
	int i;

	/* Disable all interrupts */
	for (i = 0; i <= priv->nr_irqs; i += 32) {
		vmm_writel(-1U, priv->regs + APLIC_CLRIE_BASE +
			   (i / 32) * sizeof(u32));
	}

	/* Set interrupt type and default priority for all interrupts */
	for (i = 1; i <= priv->nr_irqs; i++) {
		vmm_writel(0, priv->regs + APLIC_SOURCECFG_BASE +
			   (i - 1) * sizeof(u32));
		vmm_writel(APLIC_DEFAULT_PRIORITY,
			   priv->regs + APLIC_TARGET_BASE +
			   (i - 1) * sizeof(u32));
	}

	/* Clear APLIC domaincfg */
	vmm_writel(0, priv->regs + APLIC_DOMAINCFG);
}

static void aplic_init_hw_global(struct aplic_priv *priv)
{
	u32 val;

	/* Setup APLIC domaincfg register */
	val = vmm_readl(priv->regs + APLIC_DOMAINCFG);
	val |= APLIC_DOMAINCFG_IE;
	if (!priv->nr_idcs)
		val |= APLIC_DOMAINCFG_DM;
	vmm_writel(val, priv->regs + APLIC_DOMAINCFG);
	if (vmm_readl(priv->regs + APLIC_DOMAINCFG) != val)
		vmm_lwarning(priv->dev->name,
			     "unable to write 0x%x in domaincfg\n", val);
}

/*
 * To handle an APLIC MSI interrupts, we just find logical IRQ mapped to
 * the corresponding HW IRQ line and let Linux IRQ subsystem handle the
 * logical IRQ.
 */
static vmm_irq_return_t aplic_msi_handle_irq(int irq, void *dev)
{
	struct aplic_msi *msi = dev;
	struct aplic_priv *priv = msi->priv;

	irq = vmm_host_irqdomain_find_mapping(priv->irqdomain, msi->hw_irq);
	if (unlikely(irq <= 0))
		vmm_lwarning(priv->dev->name,
			     "can't find mapping for hwirq %u\n", msi->hw_irq);
	else
		vmm_host_generic_irq_exec(irq);

	/*
	 * We don't need to explicitly clear APLIC IRQ pending bit
	 * because as-per RISC-V AIA specification the APLIC hardware
	 * state machine will auto-clear the IRQ pending bit after
	 * MSI write has been sent-out.
	 */

	return VMM_IRQ_HANDLED;
}

static void aplic_msi_free(void *data)
{
	struct vmm_device *dev = data;

	vmm_platform_msi_domain_free_irqs(dev);
}

static void aplic_msi_write_msg(struct vmm_msi_desc *desc,
				struct vmm_msi_msg *msg)
{
	unsigned int group_index, hart_index, guest_index, val;
	struct vmm_device *dev = msi_desc_to_dev(desc);
	struct aplic_priv *priv = dev->priv;
	struct aplic_msi *msi = &priv->msis[desc->msi_index + 1];
	struct aplic_msicfg *mc = &priv->msicfg;
	physical_addr_t tppn, tbppn;
	void *target;

	/* Save the MSI address and data */
	msi->msg_addr = (((u64)msg->address_hi) << 32) | msg->address_lo;
	msi->msg_data = msg->data;
	WARN_ON(msi->msg_data > APLIC_TARGET_EIID_MASK);

	/* Compute target HART PPN */
	tppn = msi->msg_addr >> APLIC_xMSICFGADDR_PPN_SHIFT;

	/* Compute target HART Base PPN */
	tbppn = tppn;
	tbppn &= ~APLIC_xMSICFGADDR_PPN_HART(mc->lhxs);
	tbppn &= ~APLIC_xMSICFGADDR_PPN_LHX(mc->lhxw, mc->lhxs);
	tbppn &= ~APLIC_xMSICFGADDR_PPN_HHX(mc->hhxw, mc->hhxs);
	WARN_ON(tbppn != mc->base_ppn);

	/* Compute target group and hart indexes */
	group_index = (tppn >> APLIC_xMSICFGADDR_PPN_HHX_SHIFT(mc->hhxs)) &
		     APLIC_xMSICFGADDR_PPN_HHX_MASK(mc->hhxw);
	hart_index = (tppn >> APLIC_xMSICFGADDR_PPN_LHX_SHIFT(mc->lhxs)) &
		     APLIC_xMSICFGADDR_PPN_LHX_MASK(mc->lhxw);
	hart_index |= (group_index << mc->lhxw);
	WARN_ON(hart_index > APLIC_TARGET_HART_IDX_MASK);

	/* Compute target guest index */
	guest_index = tppn & APLIC_xMSICFGADDR_PPN_HART(mc->lhxs);
	WARN_ON(guest_index > APLIC_TARGET_GUEST_IDX_MASK);

	/* Update IRQ TARGET register */
	target = priv->regs + APLIC_TARGET_BASE;
	target += (msi->hw_irq - 1) * sizeof(u32);
	val = (hart_index & APLIC_TARGET_HART_IDX_MASK)
				<< APLIC_TARGET_HART_IDX_SHIFT;
	val |= (guest_index & APLIC_TARGET_GUEST_IDX_MASK)
				<< APLIC_TARGET_GUEST_IDX_SHIFT;
	val |= (msi->msg_data & APLIC_TARGET_EIID_MASK);
	vmm_writel(val, target);
}

static int aplic_setup_lmask_msis(struct aplic_priv *priv)
{
	int i, rc;
	struct aplic_msi *msi;
	struct vmm_msi_desc *desc;
	struct vmm_device *dev = priv->dev;
	struct aplic_msicfg *mc = &priv->msicfg;
	const struct imsic_global_config *imsic_global;

	/*
	 * The APLIC outgoing MSI config registers assume target MSI
	 * controller to be RISC-V AIA IMSIC controller.
	 */
	imsic_global = imsic_get_global_config();
	if (!imsic_global) {
		vmm_lerror(dev->name,
			   "IMSIC global config not found\n");
		return VMM_ENODEV;
	}

	/* Find number of guest index bits (LHXS) */
	mc->lhxs = imsic_global->guest_index_bits;
	if (APLIC_xMSICFGADDRH_LHXS_MASK < mc->lhxs) {
		vmm_lerror(dev->name,
			   "IMSIC guest index bits big for APLIC LHXS\n");
		return VMM_EINVALID;
	}

	/* Find number of HART index bits (LHXW) */
	mc->lhxw = imsic_global->hart_index_bits;
	if (APLIC_xMSICFGADDRH_LHXW_MASK < mc->lhxw) {
		vmm_lerror(dev->name,
			   "IMSIC hart index bits big for APLIC LHXW\n");
		return VMM_EINVALID;
	}

	/* Find number of group index bits (HHXW) */
	mc->hhxw = imsic_global->group_index_bits;
	if (APLIC_xMSICFGADDRH_HHXW_MASK < mc->hhxw) {
		vmm_lerror(dev->name,
			   "IMSIC group index bits big for APLIC HHXW\n");
		return VMM_EINVALID;
	}

	/* Find first bit position of group index (HHXS) */
	mc->hhxs = imsic_global->group_index_shift;
	if (mc->hhxs < (2 * APLIC_xMSICFGADDR_PPN_SHIFT)) {
		vmm_lerror(dev->name,
			   "IMSIC group index shift should be >= %d\n",
			   (2 * APLIC_xMSICFGADDR_PPN_SHIFT));
		return VMM_EINVALID;
	}
	mc->hhxs -= (2 * APLIC_xMSICFGADDR_PPN_SHIFT);
	if (APLIC_xMSICFGADDRH_HHXS_MASK < mc->hhxs) {
		vmm_lerror(dev->name,
			   "IMSIC group index shift big for APLIC HHXS\n");
		return VMM_EINVALID;
	}

	/* Compute PPN base */
	mc->base_ppn = imsic_global->base_addr >> APLIC_xMSICFGADDR_PPN_SHIFT;
	mc->base_ppn &= ~APLIC_xMSICFGADDR_PPN_HART(mc->lhxs);
	mc->base_ppn &= ~APLIC_xMSICFGADDR_PPN_LHX(mc->lhxw, mc->lhxs);
	mc->base_ppn &= ~APLIC_xMSICFGADDR_PPN_HHX(mc->hhxw, mc->hhxs);

	/* Use all possible CPUs as lmask */
	vmm_cpumask_copy(&priv->lmask, cpu_possible_mask);

	/* Allocate one APLIC MSI for every IRQ line */
	priv->msis = vmm_devm_calloc(dev, priv->nr_irqs + 1, sizeof(*msi));
	if (!priv->msis)
		return VMM_ENOMEM;
	for (i = 0; i <= priv->nr_irqs; i++) {
		priv->msis[i].hw_irq = i;
		priv->msis[i].priv = priv;
	}

	/* Allocate platform MSIs from parent */
	rc = vmm_platform_msi_domain_alloc_irqs(dev, priv->nr_irqs,
						aplic_msi_write_msg);
	if (rc) {
		vmm_lerror(dev->name, "failed to allocate MSIs\n");
		return rc;
	}

	/* Register callback to free-up MSIs */
	vmm_devm_add_action(dev, aplic_msi_free, dev);

	/* Configure chained handler for each APLIC MSI */
	for_each_msi_entry(desc, dev) {
		msi = &priv->msis[desc->msi_index + 1];
		msi->parent_irq = desc->hirq;

		vmm_host_irq_mark_chained(msi->parent_irq);
		vmm_host_irq_register(msi->parent_irq, "riscv-aplic",
				      aplic_msi_handle_irq, msi);
	}

	return VMM_OK;
}

static vmm_irq_return_t aplic_idc_handle_irq(int irq, void *dev)
{
	u32 hw_irq, hirq;
	bool have_irq = FALSE;
	struct aplic_idc *idc = dev;

	while ((hw_irq = vmm_readl(idc->regs + APLIC_IDC_CLAIMI))) {
		hw_irq = hw_irq >> APLIC_IDC_TOPI_ID_SHIFT;
		hirq = vmm_host_irqdomain_find_mapping(idc->priv->irqdomain,
						       hw_irq);
		vmm_host_generic_irq_exec(hirq);
		have_irq = TRUE;
	}

	return (have_irq) ? VMM_IRQ_HANDLED : VMM_IRQ_NONE;
}

static void aplic_idc_set_delivery(struct aplic_idc *idc, bool en)
{
	u32 de = (en) ? APLIC_ENABLE_IDELIVERY : APLIC_DISABLE_IDELIVERY;
	u32 th = (en) ? APLIC_ENABLE_ITHRESHOLD : APLIC_DISABLE_ITHRESHOLD;

	/* Priority must be less than threshold for interrupt triggering */
	vmm_writel(th, idc->regs + APLIC_IDC_ITHRESHOLD);

	/* Delivery must be set to 1 for interrupt triggering */
	vmm_writel(de, idc->regs + APLIC_IDC_IDELIVERY);
}

static int aplic_idc_dying_cpu(struct vmm_cpuhp_notify *cpuhp, u32 cpu)
{
	return VMM_OK;
}

static int aplic_idc_starting_cpu(struct vmm_cpuhp_notify *cpuhp, u32 cpu)
{
	struct aplic_idc *idc = &per_cpu(aplic_idcs, cpu);

	if (aplic_idc_parent_irq) {
		vmm_host_irq_register(aplic_idc_parent_irq, "riscv-aplic",
				      aplic_idc_handle_irq, idc);
	}

	return VMM_OK;
}

static struct vmm_cpuhp_notify aplic_cpuhp = {
	.name = "APLIC",
	.state = VMM_CPUHP_STATE_HOST_IRQ,
	.startup = aplic_idc_starting_cpu,
	.teardown = aplic_idc_dying_cpu,
};

static int aplic_setup_lmask_idcs(struct aplic_priv *priv)
{
	struct vmm_devtree_node *node = priv->dev->of_node;
	struct vmm_devtree_phandle_args parent;
	struct vmm_device *dev = priv->dev;
	u32 i, hartid, cpu, setup_count = 0;
	struct aplic_idc *idc;
	int rc;

	/* Setup per-CPU IDC and target CPU mask */
	for (i = 0; i < priv->nr_idcs; i++) {
		rc = vmm_devtree_irq_parse_one(node, i, &parent);
		if (rc || !parent.np || !parent.np->parent) {
			vmm_lerror(dev->name,
				   "failed to parse irq for IDC%d\n", i);
			return rc;
		}

		rc = riscv_node_to_hartid(parent.np->parent, &hartid);
		if (rc) {
			vmm_lerror(dev->name,
				   "failed to parse hart ID for IDC%d.\n", i);
			return rc;
		}

		rc = vmm_smp_map_cpuid(hartid, &cpu);
		if (rc) {
			vmm_lerror(dev->name, "invalid cpuid for IDC%d\n", i);
			return rc;
		}

		/* Find parent domain and register chained handler */
		if (!aplic_idc_parent_irq &&
		    vmm_devtree_irqdomain_find(parent.np)) {
			aplic_idc_parent_irq =
				vmm_devtree_irq_parse_map(node, i);
			if (aplic_idc_parent_irq) {
				vmm_cpuhp_register(&aplic_cpuhp, TRUE);
			}
		}

		vmm_cpumask_set_cpu(cpu, &priv->lmask);

		idc = &per_cpu(aplic_idcs, cpu);
		WARN_ON(idc->priv);

		idc->hart_index = i;
		idc->regs = priv->regs + APLIC_IDC_BASE + i * APLIC_IDC_SIZE;
		idc->priv = priv;

		aplic_idc_set_delivery(idc, true);

		setup_count++;
	}

	/* Fail if we were not able to setup IDC for any CPU */
	return (setup_count) ? 0 : VMM_ENODEV;
}

static int aplic_probe(struct vmm_device *dev)
{
	struct vmm_devtree_node *node = dev->of_node;
	struct aplic_priv *priv;
	virtual_addr_t base;
	physical_addr_t pa;
	int rc;

	priv = vmm_zalloc(sizeof(*priv));
	if (!priv) {
		return VMM_ENOMEM;
	}
	dev->priv = priv;
	priv->dev = dev;

	rc = vmm_devtree_read_u32(node, "riscv,num-sources", &priv->nr_irqs);
	if (rc) {
		vmm_lerror(dev->name,
			   "failed to get number of interrupt sources\n");
		goto free_priv;
	}

	rc = vmm_devtree_request_regmap(node, &base, 0, "RISC-V APLIC");
	if (rc) {
		vmm_lerror(dev->name, "failed map registers\n");
		goto free_priv;
	}
	priv->regs = (void *)base;

	/* Setup initial state APLIC interrupts */
	aplic_init_hw_irqs(priv);

	/* Setup IDCs or MSIs based on parent interrupts in DT node */
	priv->nr_idcs = vmm_devtree_irq_count(node);
	if (priv->nr_idcs)
		rc = aplic_setup_lmask_idcs(priv);
	else
		rc = aplic_setup_lmask_msis(priv);
	if (rc) {
		vmm_lerror(dev->name, "failed to setup lmask and %s\n",
			   (priv->nr_idcs) ? "idcs" : "msis");
		goto free_regmap;
	}

	/* Setup global config and interrupt delivery */
	aplic_init_hw_global(priv);

	/* Add irq domain instance for the APLIC */
	priv->irqdomain = vmm_host_irqdomain_add(node, -1, priv->nr_irqs + 1,
						 &aplic_irqdomain_ops, priv);
	if (!priv->irqdomain) {
		vmm_lerror(dev->name, "failed to add irqdomain\n");
		rc = VMM_ENOMEM;
		goto free_regmap;
	}

	if (priv->nr_idcs) {
		vmm_linfo(dev->name,
			  "%d interrupts directly connected to %d CPUs\n",
			  priv->nr_irqs, priv->nr_idcs);
	} else {
		pa = priv->msicfg.base_ppn << APLIC_xMSICFGADDR_PPN_SHIFT;
		vmm_linfo(dev->name,
			  "%d interrupts forwared to MSI base 0x%"PRIPADDR"\n",
			  priv->nr_irqs, pa);
	}

	return VMM_OK;

free_regmap:
	vmm_devtree_regunmap_release(dev->of_node,
				     (virtual_addr_t)priv->regs, 0);
free_priv:
	vmm_free(priv);
	return rc;
}

static int aplic_remove(struct vmm_device *dev)
{
	struct aplic_priv *priv = dev->priv;

	if (!priv) {
		return VMM_EFAIL;
	}

	vmm_host_irqdomain_remove(priv->irqdomain);
	vmm_devtree_regunmap_release(dev->of_node,
				     (virtual_addr_t)priv->regs, 0);
	vmm_free(priv);
	dev->priv = NULL;

	return VMM_OK;
}

static struct vmm_devtree_nodeid aplic_devid_table[] = {
	{ .compatible = "riscv,aplic" },
	{ /* end of list */ },
};

static struct vmm_driver aplic_driver = {
	.name = "riscv_aplic",
	.match_table = aplic_devid_table,
	.probe = aplic_probe,
	.remove = aplic_remove,
};

static int __init aplic_init(void)
{
	return vmm_devdrv_register_driver(&aplic_driver);
}

static void __exit aplic_exit(void)
{
	vmm_devdrv_unregister_driver(&aplic_driver);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
