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
	bool present;
	int contextid;
	unsigned long target_hart;
	u32 parent_irq;
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
	void *reg_base;
	void *reg_priority_base;
};

static struct plic_hw plic;

static void plic_context_disable_irq(struct plic_context *cntx, int hwirq)
{
	u32 mask, *reg;
	irq_flags_t flags;

	if (!cntx->present)
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

	if (!cntx->present)
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
	int i, rc, cpu;
	unsigned long hart = UINT_MAX;
	bool found_hart = FALSE;
	struct plic_context *cntx;

	for_each_cpu(cpu, mask) {
		rc = vmm_smp_map_hwid(cpu, &hart);
		if (rc)
			return rc;
		for (i = 0; i < plic.ncontexts; ++i) {
			cntx = &plic.contexts[i];
			if (!cntx->present)
				continue;
			if (cntx->target_hart == hart) {
				found_hart = TRUE;
				break;
			}
		}
		if (found_hart) {
			break;
		}
	}
	if (!found_hart) {
		return VMM_EINVALID;
	}

 	vmm_writel(1, plic.reg_priority_base + d->hwirq * PRIORITY_PER_ID);
	for (i = 0; i < plic.ncontexts; ++i) {
		cntx = &plic.contexts[i];
		if (cntx->target_hart == hart) {
			plic_context_enable_irq(cntx, d->hwirq);
		}
	}

	return VMM_OK;
}

static void plic_irq_enable(struct vmm_host_irq *d)
{
	plic_irq_enable_with_mask(d, vmm_host_irq_get_affinity(d));
}

static void plic_irq_disable(struct vmm_host_irq *d)
{
	int i;

	vmm_writel(0, plic.reg_priority_base + d->hwirq * PRIORITY_PER_ID);
	for (i = 0; i < plic.ncontexts; ++i) {
		plic_context_disable_irq(&plic.contexts[i], d->hwirq);
	}
}

static int plic_irq_set_affinity(struct vmm_host_irq *d,
				 const struct vmm_cpumask *mask,
				 bool force)
{
	int rc = VMM_OK;

	/* If priority is non-zero then IRQ is enabled */
	if (vmm_readl(plic.reg_priority_base + d->hwirq * PRIORITY_PER_ID)) {
		/* Disable IRQ for all HARTs */
		plic_irq_disable(d);
		/* Re-enable IRQ using new affinity */
		rc = plic_irq_enable_with_mask(d, mask);
	}

	return rc;
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
		hirq = vmm_host_irqdomain_find_mapping(plic.domain, hwirq);
		vmm_host_generic_irq_exec(hirq);
		vmm_writel(hwirq, cntx->reg_base + CONTEXT_CLAIM);
		have_irq = TRUE;
	}

	return (have_irq) ? VMM_IRQ_HANDLED : VMM_IRQ_NONE;
}

static struct vmm_host_irqdomain_ops plic_ops = {
	.xlate = vmm_host_irqdomain_xlate_onecell,
};

static void plic_context_init(struct plic_context *cntx)
{
	/* Check if context is present */
	if (!cntx->present) {
		return;
	}

	/* Register parent IRQ for this context */
	if (vmm_host_irq_register(cntx->parent_irq, "riscv-plic",
				  plic_chained_handle_irq, cntx)) {
		return;
	}

	/* HWIRQ prio must be > this to trigger an interrupt */
	vmm_writel(0, cntx->reg_base + CONTEXT_THRESHOLD);
}

static int plic_cpu_init(struct vmm_cpuhp_notify *cpuhp, u32 cpu)
{
	int i, rc;
	struct plic_context *cntx;
	unsigned long hart;

	rc = vmm_smp_map_hwid(cpu, &hart);
	if (rc)
		return rc;

	for (i = 0; i < plic.ncontexts; ++i) {
		cntx = &plic.contexts[i];
		if (cntx->target_hart != hart)
			continue;
		plic_context_init(cntx);
	}

	return VMM_OK;
}

static struct vmm_cpuhp_notify plic_cpuhp = {
	.name = "PLIC",
	.state = VMM_CPUHP_STATE_HOST_IRQ,
	.startup = plic_cpu_init,
};

static int __init plic_init(struct vmm_devtree_node *node)
{
	int i, j, rc, hwirq, hirq;
	physical_addr_t hart_id;
	struct plic_context *cntx;
	struct vmm_devtree_phandle_args oirq;

	/* Find number of devices */
	if (vmm_devtree_read_u32(node, "riscv,ndev", &plic.ndev)) {
		plic.ndev = MAX_DEVICES;
	}
	plic.ndev = plic.ndev + 1;

	/* Find number of contexts */
	plic.ncontexts = vmm_devtree_irq_count(node);
	plic.ncontexts_avail = 0;

	/* Allocate contexts */
	plic.contexts = vmm_zalloc(sizeof(*plic.contexts) * plic.ncontexts);
	if (!plic.contexts) {
		vmm_lerror("plic", "Failed to allocate contexts memory\n");
		return VMM_ENOMEM;
	}

	/* Setup contexts */
	for (i = 0; i < plic.ncontexts; ++i) {
		cntx = &plic.contexts[i];
		cntx->present = FALSE;
		cntx->contextid = i;
		cntx->reg_base = NULL;
		INIT_SPIN_LOCK(&cntx->reg_enable_lock);
		cntx->reg_enable_base = NULL;

		rc = vmm_devtree_irq_parse_one(node, i, &oirq);
		if (rc || !oirq.np || !oirq.np->parent || !oirq.args_count) {
			vmm_lerror("plic",
				   "Failed to parse irq for context=%d\n", i);
			continue;
		}

		rc = vmm_devtree_regaddr(oirq.np->parent, &hart_id, 0);
		vmm_devtree_dref_node(oirq.np);
		if (rc) {
			vmm_lerror("plic", "Failed to get target hart for "
				   "context=%d\n", i);
			continue;
		}
		cntx->target_hart = hart_id;

		cntx->parent_irq = vmm_devtree_irq_parse_map(node, i);
		if (cntx->parent_irq) {
			cntx->present = TRUE;
		}

		for (j = 0; j < i; j++) {
			if (plic.contexts[j].present &&
			    plic.contexts[j].target_hart == cntx->target_hart) {
				vmm_lerror("plic", "context=%d already "
					   "mapped to target_hart=%ld so "
					   "context=%d not present\n",
					   j, cntx->target_hart, i);
				cntx->present = FALSE;
			}
		}

		if (cntx->present) {
			plic.ncontexts_avail++;
		}
	}

	/* Create IRQ domain */
	plic.domain = vmm_host_irqdomain_add(node, __riscv_xlen,
					     plic.ndev, &plic_ops, NULL);
	if (!plic.domain) {
		vmm_lerror("plic", "Failed to add irqdomain\n");
		vmm_free(plic.contexts);
		return VMM_EFAIL;
	}

	/*
	 * Create IRQ domain mappings
	 * Note: Interrupt 0 is no device/interrupt.
	 */
	for (hwirq = 1; hwirq < plic.ndev; ++hwirq) {
		hirq = vmm_host_irqdomain_create_mapping(plic.domain, hwirq);
		vmm_host_irq_set_chip(hirq, &plic_chip);
		vmm_host_irq_set_handler(hirq, vmm_handle_simple_irq);
	}

	/* Find register base and size */
	rc = vmm_devtree_regaddr(node, &plic.reg_phys, 0);
	if (rc) {
		vmm_lerror("plic", "Failed to get register base\n");
		vmm_host_irqdomain_remove(plic.domain);
		vmm_free(plic.contexts);
		return rc;
	}
	plic.reg_size = CONTEXT_BASE + plic.ncontexts * CONTEXT_PER_HART;

	/* Map registers */
	vmm_request_mem_region(plic.reg_phys, plic.reg_size, "RISCV PLIC");
	plic.reg_virt = vmm_host_iomap(plic.reg_phys, plic.reg_size);
	plic.reg_base = (void *)plic.reg_virt;
	plic.reg_priority_base = plic.reg_base + PRIORITY_BASE;
	for (i = 0; i < plic.ncontexts; ++i) {
		cntx = &plic.contexts[i];
		cntx->reg_base = plic.reg_base + CONTEXT_BASE +
				    CONTEXT_PER_HART * cntx->contextid;
		cntx->reg_enable_base = plic.reg_base + ENABLE_BASE +
					ENABLE_PER_HART * cntx->contextid;
	}

	/* Disable all interrupts */
	for (i = 0; i < plic.ncontexts; ++i) {
		cntx = &plic.contexts[i];
		if (!cntx->present) {
			continue;
		}

		for (hwirq = 1; hwirq <= plic.ndev; ++hwirq) {
			plic_context_disable_irq(cntx, hwirq);
		}
	}

	/* Print details */
	vmm_init_printf("plic: base=0x%"PRIPADDR" size=%"PRIPSIZE"\n",
			plic.reg_phys, plic.reg_size);
	vmm_init_printf("plic: devices=%d contexts=%d/%d\n",
			plic.ndev, plic.ncontexts_avail, plic.ncontexts);

	return vmm_cpuhp_register(&plic_cpuhp, TRUE);
}

VMM_HOST_IRQ_INIT_DECLARE(riscvplic, "riscv,plic0", plic_init);
