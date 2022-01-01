/**
 * Copyright (c) 2018 Oza Pawandeep.
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
 * @file irq-sifive-plic.c
 * @author Oza Pawandeep (oza.pawandeep@gmail.com)
 * @author Anup Patel (anup@brainfault.org)
 * @brief SiFive Platform Interrupt Controller (PLIC) driver
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
#include <vmm_devtree.h>
#include <vmm_host_io.h>
#include <vmm_host_aspace.h>
#include <vmm_host_irq.h>
#include <vmm_host_irqdomain.h>

/*
 * From the RISC-V Privlidged Spec v1.10:
 *
 * Global interrupt sources are assigned small unsigned integer identifiers,
 * beginning at the value 1.  An interrupt ID of 0 is reserved to mean no
 * interrupt.  Interrupt identifiers are also used to break ties when two or
 * more interrupt sources have the same assigned priority. Smaller values of
 * interrupt ID take precedence over larger values of interrupt ID.
 *
 * While the RISC-V supervisor spec doesn't define the maximum number of
 * devices supported by the PLIC, the largest number supported by devices
 * marked as 'riscv,plic0' (which is the only device type this driver supports,
 * and is the only extant PLIC as of now) is 1024.  As mentioned above, device
 * 0 is defined to be non-existant so this device really only supports 1023
 * devices.
 */

#define MAX_DEVICES	1024
#define MAX_CONTEXTS	15872

/*
 * The PLIC consists of memory-mapped control registers, with a memory map as
 * follows:
 *
 * base + 0x000000: Reserved (interrupt source 0 does not exist)
 * base + 0x000004: Interrupt source 1 priority
 * base + 0x000008: Interrupt source 2 priority
 * ...
 * base + 0x000FFC: Interrupt source 1023 priority
 * base + 0x001000: Pending 0
 * base + 0x001FFF: Pending
 * base + 0x002000: Enable bits for sources 0-31 on context 0
 * base + 0x002004: Enable bits for sources 32-63 on context 0
 * ...
 * base + 0x0020FC: Enable bits for sources 992-1023 on context 0
 * base + 0x002080: Enable bits for sources 0-31 on context 1
 * ...
 * base + 0x002100: Enable bits for sources 0-31 on context 2
 * ...
 * base + 0x1F1F80: Enable bits for sources 992-1023 on context 15871
 * base + 0x1F1F84: Reserved
 * ...              (higher context IDs would fit here, but wouldn't fit
 *                   inside the per-context priority vector)
 * base + 0x1FFFFC: Reserved
 * base + 0x200000: Priority threshold for context 0
 * base + 0x200004: Claim/complete for context 0
 * base + 0x200008: Reserved
 * ...
 * base + 0x200FFC: Reserved
 * base + 0x201000: Priority threshold for context 1
 * base + 0x201004: Claim/complete for context 1
 * ...
 * base + 0xFFE000: Priority threshold for context 15871
 * base + 0xFFE004: Claim/complete for context 15871
 * base + 0xFFE008: Reserved
 * ...
 * base + 0xFFFFFC: Reserved
 */

/* Each interrupt source has a priority register associated with it. */
#define PRIORITY_BASE		0
#define PRIORITY_PER_ID		4

/*
 * Each hart context has a vector of interupt enable bits associated with it.
 * There's one bit for each interrupt source.
 */
#define ENABLE_BASE		0x2000
#define ENABLE_PER_HART		0x80

/*
 * Each hart context has a set of control registers associated with it.  Right
 * now there's only two: a source priority threshold over which the hart will
 * take an interrupt, and a register to claim interrupts.
 */
#define CONTEXT_BASE		0x200000
#define CONTEXT_PER_HART	0x1000
#define CONTEXT_THRESHOLD	0
#define CONTEXT_CLAIM		4

struct plic_context {
	struct plic_hw *hw;
	int contextid;
	unsigned long target_hart;
	void *reg_base;
	vmm_spinlock_t reg_enable_lock;
	void *reg_enable_base;
};

struct plic_hw {
	u32 ndev;
	u32 ncontexts;
	u32 ncontexts_avail;
	struct vmm_host_irqdomain *domain;
	struct plic_context *contexts;
	physical_addr_t reg_phys;
	physical_size_t reg_size;
	virtual_addr_t reg_virt;
	struct vmm_cpumask lmask;
	void *reg_base;
	void *reg_priority_base;
};

static bool plic_cpuhp_setup_done;
static u32 plic_parent_irq;
static DEFINE_PER_CPU(struct plic_context *, handlers);

static void plic_context_disable_irq(struct plic_context *cntx, int hwirq)
{
	u32 mask, *reg;
	irq_flags_t flags;

	if (!cntx->hw)
		return;

	reg = cntx->reg_enable_base + (hwirq / 32) * sizeof(u32);
	mask = ~(1 << (hwirq % 32));

	vmm_spin_lock_irqsave_lite(&cntx->reg_enable_lock, flags);
	vmm_writel(vmm_readl(reg) & mask, reg);
	vmm_spin_unlock_irqrestore_lite(&cntx->reg_enable_lock, flags);
}

static void plic_context_enable_irq(struct plic_context *cntx, int hwirq)
{
	u32 bit, *reg;
	irq_flags_t flags;

	if (!cntx->hw)
		return;

	reg = cntx->reg_enable_base + (hwirq / 32) * sizeof(u32);
	bit = 1 << (hwirq % 32);

	vmm_spin_lock_irqsave_lite(&cntx->reg_enable_lock, flags);
	vmm_writel(vmm_readl(reg) | bit, reg);
	vmm_spin_unlock_irqrestore_lite(&cntx->reg_enable_lock, flags);
}

static int plic_irq_enable_with_mask(struct vmm_host_irq *d,
				     const struct vmm_cpumask *mask)
{
	int cpu;
	struct vmm_cpumask amask;
	struct plic_context *cntx;
	struct plic_hw *hw = vmm_host_irq_get_chip_data(d);

	vmm_cpumask_and(&amask, &hw->lmask, cpu_online_mask);
	cpu = vmm_cpumask_any_and(mask, &amask);
	cntx = per_cpu(handlers, cpu);

	vmm_writel(1, cntx->hw->reg_priority_base +
		      d->hwirq * PRIORITY_PER_ID);
	plic_context_enable_irq(cntx, d->hwirq);

	return VMM_OK;
}

static void plic_irq_enable(struct vmm_host_irq *d)
{
	plic_irq_enable_with_mask(d, vmm_host_irq_get_affinity(d));
}

static void plic_irq_disable(struct vmm_host_irq *d)
{
	int i;
	struct plic_hw *hw = vmm_host_irq_get_chip_data(d);

	vmm_writel(0, hw->reg_priority_base + d->hwirq * PRIORITY_PER_ID);
	for (i = 0; i < hw->ncontexts; ++i) {
		plic_context_disable_irq(&hw->contexts[i], d->hwirq);
	}
}

static int plic_irq_set_affinity(struct vmm_host_irq *d,
				 const struct vmm_cpumask *mask,
				 bool force)
{
	/* Disable IRQ for all HARTs */
	plic_irq_disable(d);

	/* Re-enable IRQ using new affinity */
	return plic_irq_enable_with_mask(d, mask);
}

static struct vmm_host_irq_chip plic_chip = {
	.name		= "riscv-plic",
	.irq_enable	= plic_irq_enable,
	.irq_disable	= plic_irq_disable,
	.irq_set_affinity = plic_irq_set_affinity,
};

static vmm_irq_return_t plic_chained_handle_irq(int irq, void *dev)
{
	int hwirq, hirq;
	bool have_irq = FALSE;
	struct plic_context *cntx = dev;

	/*
	 * Handling an interrupt is a two-step process: first you claim
	 * the interrupt by reading the claim register, then you complete
	 * the interrupt by writing that source ID back to the same claim
	 * register.  This automatically enables and disables the interrupt,
	 * so there's nothing else to do.
	 */
	while ((hwirq = vmm_readl(cntx->reg_base + CONTEXT_CLAIM))) {
		hirq = vmm_host_irqdomain_find_mapping(cntx->hw->domain,
							hwirq);
		vmm_host_generic_irq_exec(hirq);
		vmm_writel(hwirq, cntx->reg_base + CONTEXT_CLAIM);
		have_irq = TRUE;
	}

	return (have_irq) ? VMM_IRQ_HANDLED : VMM_IRQ_NONE;
}

static int plic_irqdomain_map(struct vmm_host_irqdomain *dom,
			      unsigned int hirq, unsigned int hwirq)
{
	struct plic_hw *hw = dom->host_data;

	vmm_host_irq_set_chip(hirq, &plic_chip);
	vmm_host_irq_set_chip_data(hirq, hw);
	vmm_host_irq_set_handler(hirq, vmm_handle_simple_irq);

	return VMM_OK;
}

static struct vmm_host_irqdomain_ops plic_ops = {
	.xlate = vmm_host_irqdomain_xlate_onecell,
	.map = plic_irqdomain_map,
};

static void plic_context_init(struct plic_context *cntx)
{
	/* Check if context has parent IRQ or parent HW device */
	if (!plic_parent_irq || !cntx->hw) {
		return;
	}

	/* Register parent IRQ for this context */
	if (vmm_host_irq_register(plic_parent_irq, "riscv-plic",
				  plic_chained_handle_irq, cntx)) {
		return;
	}

	/* HWIRQ prio must be > this to trigger an interrupt */
	vmm_writel(0, cntx->reg_base + CONTEXT_THRESHOLD);
}

static int plic_cpu_init(struct vmm_cpuhp_notify *cpuhp, u32 cpu)
{
	struct plic_context *cntx = per_cpu(handlers, cpu);

	if (!cntx || !cntx->hw) {
		vmm_lerror("plic", "No context for CPU%d\n", cpu);
		return VMM_EINVALID;
	}

	plic_context_init(cntx);
	return VMM_OK;
}

static struct vmm_cpuhp_notify plic_cpuhp = {
	.name = "PLIC",
	.state = VMM_CPUHP_STATE_HOST_IRQ,
	.startup = plic_cpu_init,
};

static int __init plic_init(struct vmm_devtree_node *node)
{
	u32 cpu;
	int i, rc, hwirq;
	struct plic_hw *hw = NULL;
	struct plic_context *cntx;
	physical_addr_t hart_id;
	struct vmm_devtree_phandle_args oirq;

	/* Allocate PLIC HW */
	hw = vmm_zalloc(sizeof(*hw));
	if (!hw) {
		vmm_lerror("plic", "%s: failed to HW instance memory\n",
			   node->name);
		return VMM_ENOMEM;
	}

	/* Find number of devices */
	if (vmm_devtree_read_u32(node, "riscv,ndev", &hw->ndev)) {
		hw->ndev = MAX_DEVICES;
	}
	hw->ndev = hw->ndev + 1;

	/* Find number of contexts */
	hw->ncontexts = vmm_devtree_irq_count(node);
	hw->ncontexts_avail = 0;

	/* Allocate contexts */
	hw->contexts = vmm_zalloc(sizeof(*hw->contexts) * hw->ncontexts);
	if (!hw->contexts) {
		vmm_lerror("plic", "%s: failed to allocate contexts memory\n",
			   node->name);
		vmm_free(hw);
		return VMM_ENOMEM;
	}

	/* Find register base and size */
	rc = vmm_devtree_regaddr(node, &hw->reg_phys, 0);
	if (rc) {
		vmm_lerror("plic", "%s: failed to get register base\n",
			   node->name);
		vmm_free(hw->contexts);
		vmm_free(hw);
		return rc;
	}
	hw->reg_size = CONTEXT_BASE + hw->ncontexts * CONTEXT_PER_HART;

	/* Map registers */
	vmm_request_mem_region(hw->reg_phys, hw->reg_size, "RISCV PLIC");
	hw->reg_virt = vmm_host_iomap(hw->reg_phys, hw->reg_size);
	if (!hw->reg_virt) {
		vmm_lerror("plic", "%s: failed to map registers\n",
			   node->name);
		vmm_free(hw->contexts);
		vmm_free(hw);
		return VMM_EIO;
	}
	hw->reg_base = (void *)hw->reg_virt;
	hw->reg_priority_base = hw->reg_base + PRIORITY_BASE;

	/* Setup contexts */
	for (i = 0; i < hw->ncontexts; ++i) {
		cntx = &hw->contexts[i];
		cntx->hw = NULL;
		cntx->contextid = i;
		INIT_SPIN_LOCK(&cntx->reg_enable_lock);
		cntx->reg_base = hw->reg_base + CONTEXT_BASE +
				    CONTEXT_PER_HART * cntx->contextid;
		cntx->reg_enable_base = hw->reg_base + ENABLE_BASE +
					ENABLE_PER_HART * cntx->contextid;

		/* Parse interrupt entry */
		rc = vmm_devtree_irq_parse_one(node, i, &oirq);
		if (rc || !oirq.np || !oirq.np->parent || !oirq.args_count) {
			vmm_lerror("plic",
				   "%s: failed to parse irq for context=%d\n",
				   node->name, i);
			continue;
		}

		/* Find target HART id */
		rc = vmm_devtree_regaddr(oirq.np->parent, &hart_id, 0);
		vmm_devtree_dref_node(oirq.np);
		if (rc) {
			vmm_lerror("plic", "%s: failed to get target hart "
				   "for context=%d\n", node->name, i);
			continue;
		}
		cntx->target_hart = hart_id;

		/* Find target CPU id */
		rc = vmm_smp_map_cpuid(hart_id, &cpu);
		if (rc) {
			vmm_lerror("plic", "%s: failed to get target CPU "
				   "for context=%d\n", node->name, i);
			continue;
		}

		/* Check parent Hardware IRQ number */
		if (oirq.args[0] != IRQ_S_EXT) {
			continue;
		}
		cntx->hw = hw;

		/* Map parent IRQ if not available */
		if (!plic_parent_irq) {
			plic_parent_irq = vmm_devtree_irq_parse_map(node, i);
		}

		/* Update per-CPU handler pointer */
		per_cpu(handlers, cpu) = cntx;
		vmm_cpumask_set_cpu(cpu, &hw->lmask);

		/* Disable all interrupts */
		for (hwirq = 1; hwirq <= cntx->hw->ndev; ++hwirq) {
			plic_context_disable_irq(cntx, hwirq);
		}

		hw->ncontexts_avail++;
	}

	/* Create IRQ domain */
	hw->domain = vmm_host_irqdomain_add(node, -1, hw->ndev,
					    &plic_ops, hw);
	if (!hw->domain) {
		vmm_lerror("plic", "%s: failed to add irqdomain\n",
			   node->name);
		vmm_host_iounmap(hw->reg_virt);
		vmm_free(hw->contexts);
		vmm_free(hw);
		return VMM_EFAIL;
	}

	/* Setup CPU hotplug notifier */
	if (this_cpu(handlers) && !plic_cpuhp_setup_done) {
		rc = vmm_cpuhp_register(&plic_cpuhp, TRUE);
		if (rc) {
			vmm_lerror("plic", "%s: failed to setup cpuhp\n",
				   node->name);
			vmm_host_irqdomain_remove(hw->domain);
			vmm_host_iounmap(hw->reg_virt);
			vmm_free(hw->contexts);
			vmm_free(hw);
			return rc;
		}
		plic_cpuhp_setup_done = TRUE;
	}

	/* Print details */
	vmm_init_printf("plic: %s: devices=%d contexts=%d/%d\n",
			node->name, hw->ndev, hw->ncontexts_avail,
			hw->ncontexts);

	return VMM_OK;
}

VMM_HOST_IRQ_INIT_DECLARE(riscvplic, "riscv,plic0", plic_init);
