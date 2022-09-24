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

#include <cpu_hwcap.h>
#include <riscv_encoding.h>
#include <riscv_csr.h>

#define RISCV_IRQ_COUNT __riscv_xlen

static struct vmm_host_irqdomain *intc_domain __read_mostly;

static void riscv_irqchip_mask_irq(struct vmm_host_irq *d)
{
	if (d->hwirq < BITS_PER_LONG) {
		csr_clear(CSR_SIE, BIT(d->hwirq));
	} else {
		csr_clear(CSR_SIEH, BIT(d->hwirq - BITS_PER_LONG));
	}
}

static void riscv_irqchip_unmask_irq(struct vmm_host_irq *d)
{
	if (d->hwirq < BITS_PER_LONG) {
		csr_set(CSR_SIE, BIT(d->hwirq));
	} else {
		csr_set(CSR_SIEH, BIT(d->hwirq - BITS_PER_LONG));
	}
}

static struct vmm_host_irq_chip riscv_irqchip = {
	.name = "riscv-intc",
	.irq_mask = riscv_irqchip_mask_irq,
	.irq_unmask = riscv_irqchip_unmask_irq,
};

static u32 riscv_intc_aia_active_irq(u32 cpu_irq_no, u32 prev_irq)
{
	unsigned long topi = csr_read(CSR_STOPI);
	return (topi) ? topi >> TOPI_IID_SHIFT : UINT_MAX;
}

static u32 riscv_intc_active_irq(u32 cpu_irq_no, u32 prev_irq)
{
	if (cpu_irq_no != prev_irq)
		return (cpu_irq_no < RISCV_IRQ_COUNT) ? cpu_irq_no : UINT_MAX;
	return UINT_MAX;
}

static struct vmm_host_irqdomain_ops riscv_intc_ops = {
	.xlate = vmm_host_irqdomain_xlate_onecell,
};

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
	u32 nr_irqs, hart_id = 0;

	/* Get hart_id of associated HART */
	rc = riscv_node_to_hartid(node->parent, &hart_id);
	if (rc) {
		vmm_lerror("riscv-intc",
			   "can't find hart_id of asociated HART\n");
		return rc;
	}

	/* Do nothing if associated HART is not boot HART */
	if (vmm_smp_processor_id() != hart_id) {
		return VMM_OK;
	}

	/* Determine number of IRQs */
	nr_irqs = BITS_PER_LONG;
	if (riscv_isa_extension_available(NULL, SxAIA) && BITS_PER_LONG == 32)
		nr_irqs = nr_irqs * 2;

	/* Register IRQ domain */
	intc_domain = vmm_host_irqdomain_add(node, 0, nr_irqs,
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
	if (riscv_isa_extension_available(NULL, SxAIA)) {
		vmm_host_irq_set_active_callback(riscv_intc_aia_active_irq);
	} else {
		vmm_host_irq_set_active_callback(riscv_intc_active_irq);
	}

	/* Announce RISC-V INTC */
	vmm_init_printf("riscv-intc: registered %d local interrupts%s\n",
			nr_irqs,
			(riscv_isa_extension_available(NULL, SxAIA)) ?
			" with AIA" : "");
	return VMM_OK;
}

VMM_HOST_IRQ_INIT_DECLARE(riscvintc, "riscv,cpu-intc", riscv_intc_init);
