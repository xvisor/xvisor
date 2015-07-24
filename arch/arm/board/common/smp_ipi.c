/**
 * Copyright (c) 2013 Anup Patel.
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
 * @file smp_ipi.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Common IPI implementation
 */

#include <vmm_error.h>
#include <vmm_types.h>
#include <vmm_smp.h>
#include <vmm_compiler.h>
#include <vmm_host_irq.h>

static bool smp_ipi_available = FALSE;
static u32 smp_ipi_irq = 0;

static vmm_irq_return_t smp_ipi_handler(int irq_no, void *dev)
{
	/* Call core code to handle IPI */
	vmm_smp_ipi_exec();

	return VMM_IRQ_HANDLED;
}

void arch_smp_ipi_trigger(const struct vmm_cpumask *dest)
{
	/* Raise IPI to other cores */
	if (smp_ipi_available) {
		vmm_host_irq_raise(smp_ipi_irq, dest);
	}
}

int __cpuinit arch_smp_ipi_init(void)
{
	int rc;

	/* Find host irq which is marked as per-CPU and IPI */
	rc = vmm_host_irq_find(0, VMM_IRQ_STATE_IPI|VMM_IRQ_STATE_PER_CPU,
			       &smp_ipi_irq);
	if (rc) {
		/* If no IPI found then we don't have IPIs
		 * on underlying host hence do nothing.
		 */
		smp_ipi_available = FALSE;
		smp_ipi_irq = 0;
		return VMM_OK;
	}

	/* Register IPI interrupt handler */
	rc = vmm_host_irq_register(smp_ipi_irq, "IPI",
				   &smp_ipi_handler, NULL);
	if (rc) {
		return rc;
	}

	/* We are all set for IPIs so mark this. */
	smp_ipi_available = TRUE;

	return VMM_OK;
}
