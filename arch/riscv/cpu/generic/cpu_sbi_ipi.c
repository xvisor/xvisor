/**
 * Copyright (c) 2021 Western Digital Corporation or its affiliates.
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
 * @file cpu_sbi_ipi.c
 * @author Anup Patel (anup.patel@wdc.com)
 * @brief Supervisor binary interface (SBI) based IPI driver
 */

#include <vmm_error.h>
#include <vmm_compiler.h>
#include <vmm_smp.h>
#include <vmm_cpumask.h>
#include <vmm_cpuhp.h>
#include <vmm_stdio.h>
#include <vmm_host_irq.h>
#include <vmm_host_irqdomain.h>

#include <cpu_sbi.h>
#include <riscv_encoding.h>
#include <riscv_csr.h>
#include <riscv_sbi.h>

static struct vmm_host_irqdomain *sbi_ipi_domain;

static void sbi_ipi_dummy(struct vmm_host_irq *d)
{
}

static void sbi_ipi_raise(struct vmm_host_irq *d,
			  const struct vmm_cpumask *mask)
{
	struct vmm_cpumask tmask;

	sbi_cpumask_to_hartmask(mask, &tmask);
	sbi_send_ipi(vmm_cpumask_bits(&tmask));
}

static struct vmm_host_irq_chip sbi_ipi_irqchip = {
	.name = "riscv-sbi-ipi",
	.irq_mask = sbi_ipi_dummy,
	.irq_unmask = sbi_ipi_dummy,
	.irq_raise = sbi_ipi_raise
};

static vmm_irq_return_t sbi_ipi_handler(int irq, void *dev)
{
	int hirq;

	csr_clear(sip, SIP_SSIP);
	hirq = vmm_host_irqdomain_find_mapping(sbi_ipi_domain, 0);
	vmm_host_generic_irq_exec(hirq);

	return VMM_IRQ_HANDLED;
}

static int sbi_ipi_startup(struct vmm_cpuhp_notify *cpuhp, u32 cpu)
{
	/* Register irq handler for RISC-V SBI IPI */
	return vmm_host_irq_register(IRQ_S_SOFT, "riscv-sbi-ipi",
				     &sbi_ipi_handler, NULL);
}

static struct vmm_cpuhp_notify sbi_ipi_cpuhp = {
	.name = "RISCV_SBI_IPI",
	.state = VMM_CPUHP_STATE_SMP_SYNC_IPI,
	.startup = sbi_ipi_startup,
};

int __init sbi_ipi_init(void)
{
	int rc, hirq;
	u32 ipi_irq;

	/* Do nothing if IPI already registered */
	rc = vmm_host_irq_find(0, VMM_IRQ_STATE_IPI|VMM_IRQ_STATE_PER_CPU,
			       &ipi_irq);
	if (!rc) {
		return VMM_OK;
	}

	/* Register IPI domain */
	sbi_ipi_domain = vmm_host_irqdomain_add(NULL, BITS_PER_LONG * 2, 1,
						&irqdomain_simple_ops, NULL);
	if (!sbi_ipi_domain) {
		vmm_lerror("riscv-sbi-ipi",
			   "failed to add irq domain\n");
		return VMM_ENOMEM;
	}

	/* Setup IPI domain interrupts */
	hirq = vmm_host_irqdomain_create_mapping(sbi_ipi_domain, 0);
	if (hirq < 0) {
		vmm_lerror("riscv-sbi-ipi",
			   "failed to create irq mapping\n");
		vmm_host_irqdomain_remove(sbi_ipi_domain);
		sbi_ipi_domain = NULL;
		return hirq;
	}
	vmm_host_irq_mark_per_cpu(hirq);
	vmm_host_irq_mark_ipi(hirq);
	vmm_host_irq_set_chip(hirq, &sbi_ipi_irqchip);
	vmm_host_irq_set_handler(hirq, vmm_handle_percpu_irq);

	/* Setup CPUHP notifier */
	rc = vmm_cpuhp_register(&sbi_ipi_cpuhp, TRUE);
	if (rc) {
		vmm_lerror("riscv-sbi-ipi",
			   "failed to register cpuhp\n");
		vmm_host_irqdomain_remove(sbi_ipi_domain);
		sbi_ipi_domain = NULL;
		return rc;
	}

	/* Announce SBI IPI support */
	vmm_init_printf("riscv-sbi-ipi: registered IPI domain\n");
	return VMM_OK;
}
