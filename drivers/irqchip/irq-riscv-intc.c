/**
 * Copyright (c) 2018 Anup Patel.
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
 * @file irq-riscv-intc.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief RISC-V local intc driver
 *
 * The source has been largely adapted from Linux
 * drivers/irqchip/irq-riscv-intc.c
 *
 * Copyright (C) 2012 Regents of the University of California
 * Copyright (C) 2017 SiFive
 *
 * The original code is licensed under the GPL.
 */

#include <vmm_error.h>
#include <vmm_limits.h>
#include <vmm_compiler.h>
#include <vmm_stdio.h>
#include <vmm_smp.h>
#include <vmm_cpuhp.h>
#include <vmm_host_io.h>
#include <vmm_host_irq.h>
#include <vmm_host_irqdomain.h>

#include <riscv_encoding.h>
#include <riscv_csr.h>

#define RISCV_IRQ_COUNT __riscv_xlen

static struct vmm_host_irqdomain *intc_domain __read_mostly;

static void riscv_irqchip_mask_irq(struct vmm_host_irq *d)
{
	csr_clear(sie, 1UL << d->hwirq);
}

static void riscv_irqchip_unmask_irq(struct vmm_host_irq *d)
{
	csr_set(sie, 1UL << d->hwirq);
}

static struct vmm_host_irq_chip riscv_irqchip = {
	.name = "riscv-intc",
	.irq_mask = riscv_irqchip_mask_irq,
	.irq_unmask = riscv_irqchip_unmask_irq,
};

static u32 riscv_intc_active_irq(u32 cpu_irq_no)
{
	return (cpu_irq_no < RISCV_IRQ_COUNT) ? cpu_irq_no : UINT_MAX;
}

static struct vmm_host_irqdomain_ops riscv_intc_ops = {
	.xlate = vmm_host_irqdomain_xlate_onecell,
};

static int riscv_hart_of_timer(struct vmm_devtree_node *node, u32 *hart_id)
{
	int rc;

	if (!node)
		return VMM_EINVALID;
	if (!vmm_devtree_is_compatible(node, "riscv"))
		return VMM_ENODEV;

	if (hart_id) {
		rc = vmm_devtree_read_u32(node, "reg", hart_id);
		if (rc)
			return rc;
	}

	return VMM_OK;
}

static int riscv_intc_startup(struct vmm_cpuhp_notify *cpuhp, u32 cpu)
{
	/* Disable and clear all per-CPU interupts */
	csr_write(sie, 0UL);
	csr_write(sip, 0UL);

	return VMM_OK;
}

static struct vmm_cpuhp_notify riscv_intc_cpuhp = {
	.name = "RISCV_INTC",
	.state = VMM_CPUHP_STATE_HOST_IRQ,
	.startup = riscv_intc_startup,
};

static int __init riscv_intc_init(struct vmm_devtree_node *node)
{
	int i, irq, rc;
	u32 hart_id = 0;

	/* Get hart_id of associated HART */
	rc = riscv_hart_of_timer(node->parent, &hart_id);
	if (rc) {
		vmm_lerror("riscv-intc",
			   "can't find hart_id of asociated HART\n");
		return rc;
	}

	/* Do nothing if associated HART is not boot HART */
	if (vmm_smp_processor_id() != hart_id) {
		return VMM_OK;
	}

	/* Register IRQ domain */
	intc_domain = vmm_host_irqdomain_add(node, 0, RISCV_IRQ_COUNT,
					     &riscv_intc_ops, NULL);
	if (!intc_domain) {
		vmm_lerror("riscv-intc", "failed to add irq domain\n");
		return VMM_EFAIL;
	}

	/* Create IRQ mappings */
	for (i = 0; i < RISCV_IRQ_COUNT; i++) {
		irq = vmm_host_irqdomain_create_mapping(intc_domain, i);
		if (irq < 0) {
			continue;
		}

		vmm_host_irq_mark_per_cpu(irq);
		vmm_host_irq_set_chip(irq, &riscv_irqchip);
		vmm_host_irq_set_handler(irq, vmm_handle_percpu_irq);
	}

	/* Register CPU hotplug notifier */
	rc = vmm_cpuhp_register(&riscv_intc_cpuhp, TRUE);
	if (rc) {
		vmm_lerror("riscv-intc", "failed to register cpuhp\n");
		vmm_host_irqdomain_remove(intc_domain);
		intc_domain = NULL;
		return rc;
	}

	/* Register active IRQ callback */
	vmm_host_irq_set_active_callback(riscv_intc_active_irq);

	/* Announce RISC-V INTC */
	vmm_init_printf("riscv-intc: registered %d local interrupts\n",
			RISCV_IRQ_COUNT);
	return VMM_OK;
}

VMM_HOST_IRQ_INIT_DECLARE(riscvintc, "riscv,cpu-intc", riscv_intc_init);
