/**
 * Copyright (c) 2021 Anup Patel.
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
 * @file irq-riscv-aclint-swi.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief RISC-V advanced core local interruptor driver
 *
 * The source has been largely adapted from Linux
 * drivers/irqchip/irq-riscv-aclint-swi.c
 *
 * Copyright (C) 2021 Western Digital Corporation or its affiliates.
 *
 * The original code is licensed under the GPL.
 */

#include <vmm_error.h>
#include <vmm_limits.h>
#include <vmm_compiler.h>
#include <vmm_stdio.h>
#include <vmm_smp.h>
#include <vmm_cpuhp.h>
#include <vmm_percpu.h>
#include <vmm_host_io.h>
#include <vmm_host_irq.h>
#include <vmm_host_irqdomain.h>

#include <riscv_encoding.h>
#include <riscv_csr.h>

static DEFINE_PER_CPU(void *, aclint_swi_reg);
static struct vmm_host_irqdomain *aclint_swi_domain __read_mostly;

static void aclint_swi_dummy(struct vmm_host_irq *d)
{
}

static void aclint_swi_raise(struct vmm_host_irq *d,
			     const struct vmm_cpumask *mask)
{
	u32 cpu;
	void *swi_reg;

	for_each_cpu(cpu, mask) {
		swi_reg = per_cpu(aclint_swi_reg, cpu);
		vmm_writel(1, swi_reg);
	}
}

static struct vmm_host_irq_chip aclint_swi_irqchip = {
	.name = "riscv-aclint-swi",
	.irq_mask = aclint_swi_dummy,
	.irq_unmask = aclint_swi_dummy,
	.irq_raise = aclint_swi_raise
};

static vmm_irq_return_t aclint_swi_handler(int irq, void *dev)
{
	int hirq;

	csr_clear(sip, SIP_SSIP);
	hirq = vmm_host_irqdomain_find_mapping(aclint_swi_domain, 0);
	vmm_host_generic_irq_exec(hirq);

	return VMM_IRQ_HANDLED;
}

static int aclint_swi_startup(struct vmm_cpuhp_notify *cpuhp, u32 cpu)
{
	/* Register irq handler for RISC-V ACLINT SWI */
	return vmm_host_irq_register(IRQ_S_SOFT, "riscv-aclint-swi",
				     &aclint_swi_handler, NULL);
}

static struct vmm_cpuhp_notify aclint_swi_cpuhp = {
	.name = "RISCV_ACLINT_SWI",
	.state = VMM_CPUHP_STATE_HOST_IRQ,
	.startup = aclint_swi_startup,
};

static int __init aclint_swi_init(struct vmm_devtree_node *node)
{
	int hirq, rc;
	virtual_addr_t va;
	u32 i, cpu, nr_irqs, nr_cpus = 0;
	physical_addr_t hart_id, thart_id;
	struct vmm_devtree_phandle_args oirq;

	/* Map ACLINT SWI registers */
	rc = vmm_devtree_request_regmap(node, &va, 0, "RISC-V ACLINT SWI");
	if (rc) {
		vmm_lerror("aclint-swi", "%s: failed to map registers\n",
			   node->name);
		return rc;
	}

	/* Update per-CPU ACLINT SWI base addresses */
	nr_irqs = vmm_devtree_irq_count(node);
	for (i = 0; i < nr_irqs; i++) {
		rc = vmm_devtree_irq_parse_one(node, i, &oirq);
		if (rc || !oirq.np || !oirq.np->parent || !oirq.args_count) {
			vmm_lerror("aclint-swi",
				   "%s: failed to parse irq%d\n",
				   node->name, i);
			continue;
		}

		rc = vmm_devtree_regaddr(oirq.np->parent, &hart_id, 0);
		vmm_devtree_dref_node(oirq.np);
		if (rc) {
			vmm_lerror("aclint-swi",
				   "%s: failed to get hart_id for irq%d\n",
				   node->name, i);
			continue;
		}

		for_each_possible_cpu(cpu) {
			vmm_smp_map_hwid(cpu, &thart_id);
			if (thart_id != hart_id) {
				continue;
			}

			per_cpu(aclint_swi_reg, cpu) =
					(void *)(va + sizeof(u32) * i);
			nr_cpus++;
			break;
		}
	}

	/* If ACLINT SWI domain already registered then skip */
	if (aclint_swi_domain) {
		goto done;
	}

	/* Register ACLINT_SWI domain */
	aclint_swi_domain = vmm_host_irqdomain_add(NULL, BITS_PER_LONG * 2, 1,
						   &irqdomain_simple_ops,
						   NULL);
	if (!aclint_swi_domain) {
		vmm_lerror("aclint-swi",
			   "%s: failed to add irq domain\n",
			   node->name);
		vmm_devtree_regunmap_release(node, va, 0);
		return VMM_ENOMEM;
	}

	/* Setup ACLINT SWI domain interrupts */
	hirq = vmm_host_irqdomain_create_mapping(aclint_swi_domain, 0);
	if (hirq < 0) {
		vmm_lerror("aclint-swi",
			   "%s: failed to create irq mapping\n",
			   node->name);
		vmm_host_irqdomain_remove(aclint_swi_domain);
		aclint_swi_domain = NULL;
		vmm_devtree_regunmap_release(node, va, 0);
		return hirq;
	}
	vmm_host_irq_mark_per_cpu(hirq);
	vmm_host_irq_mark_ipi(hirq);
	vmm_host_irq_set_chip(hirq, &aclint_swi_irqchip);
	vmm_host_irq_set_handler(hirq, vmm_handle_percpu_irq);

	/* Setup CPUHP notifier */
	rc = vmm_cpuhp_register(&aclint_swi_cpuhp, TRUE);
	if (rc) {
		vmm_lerror("aclint-swi",
			   "%s: failed to register cpuhp\n",
			   node->name);
		vmm_host_irqdomain_remove(aclint_swi_domain);
		aclint_swi_domain = NULL;
		vmm_devtree_regunmap_release(node, va, 0);
		return rc;
	}

done:
	/* Announce the ACLINT SWI device */
	vmm_init_printf("aclint-swi: %s: providing IPIs for %d CPUs\n",
			node->name, nr_cpus);
	return VMM_OK;
}

VMM_HOST_IRQ_INIT_DECLARE(aclint_swi, "riscv,aclint-sswi", aclint_swi_init);
