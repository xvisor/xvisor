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

#include <cpu_sbi.h>
#include <riscv_encoding.h>
#include <riscv_csr.h>

#define RISCV_IRQ_COUNT __riscv_xlen

struct riscv_irqchip_intc {
	struct vmm_host_irqdomain *domain;
};

static struct riscv_irqchip_intc intc __read_mostly;

static void riscv_irqchip_mask_irq(struct vmm_host_irq *d)
{
	csr_clear(sie, 1UL << d->hwirq);
}

static void riscv_irqchip_unmask_irq(struct vmm_host_irq *d)
{
	csr_set(sie, 1UL << d->hwirq);
}

static void riscv_irqchip_ack_irq(struct vmm_host_irq *d)
{
	csr_clear(sip, 1UL << d->hwirq);
}

#ifdef CONFIG_SMP
static void riscv_irqchip_raise(struct vmm_host_irq *d,
				const struct vmm_cpumask *mask)
{
	struct vmm_cpumask tmask;

	if (d->hwirq != IRQ_S_SOFT)
		return;

	sbi_cpumask_to_hartmask(mask, &tmask);
	sbi_send_ipi(vmm_cpumask_bits(&tmask));
}
#endif

static struct vmm_host_irq_chip riscv_irqchip = {
	.name = "riscv-intc",
	.irq_mask = riscv_irqchip_mask_irq,
	.irq_unmask = riscv_irqchip_unmask_irq,
	.irq_ack = riscv_irqchip_ack_irq,
#ifdef CONFIG_SMP
	.irq_raise = riscv_irqchip_raise
#endif
};

static void riscv_irqchip_register_irq(u32 hwirq, bool is_ipi,
					struct vmm_host_irq_chip *chip)
{
	int irq = vmm_host_irqdomain_create_mapping(intc.domain, hwirq);

	BUG_ON(irq < 0);

	vmm_host_irq_mark_per_cpu(irq);
	if (is_ipi) {
		vmm_host_irq_mark_ipi(irq);
	}
	vmm_host_irq_set_chip(irq, chip);
	vmm_host_irq_set_handler(irq, vmm_handle_percpu_irq);
}

static u32 riscv_intc_active_irq(u32 cpu_irq_no)
{
	if (RISCV_IRQ_COUNT <= cpu_irq_no) {
		return UINT_MAX;
	}

	if (csr_read(sip) & (1UL << cpu_irq_no)) {
		return cpu_irq_no;
	}

	return UINT_MAX;
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
	int i, rc;
	u32 hart_id = 0;

	rc = riscv_hart_of_timer(node->parent, &hart_id);
	if (rc) {
		return rc;
	}

	if (vmm_smp_processor_id() != hart_id) {
		return VMM_OK;
	}

	intc.domain = vmm_host_irqdomain_add(node, 0, RISCV_IRQ_COUNT,
					     &riscv_intc_ops, NULL);
	if (!intc.domain) {
		return VMM_EFAIL;
	}

	/* Setup up per-CPU interrupts */
	for (i = 0; i < RISCV_IRQ_COUNT; i++) {
		riscv_irqchip_register_irq(i, (i == IRQ_S_SOFT) ? TRUE : FALSE,
					   &riscv_irqchip);
	}

	vmm_host_irq_set_active_callback(riscv_intc_active_irq);

	return vmm_cpuhp_register(&riscv_intc_cpuhp, TRUE);
}

VMM_HOST_IRQ_INIT_DECLARE(riscvintc, "riscv,cpu-intc", riscv_intc_init);
