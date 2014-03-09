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

static vmm_irq_return_t smp_ipi_handler(int irq_no, void *dev)
{
	/* Call core code to handle IPI1 */
	vmm_smp_ipi_exec();	

	return VMM_IRQ_HANDLED;
}

void arch_smp_ipi_trigger(const struct vmm_cpumask *dest)
{
	/* Raise IPI1 to other cores */
	vmm_host_irq_raise(1, dest);
}

int __cpuinit arch_smp_ipi_init(void)
{
	int rc;

	/* Ignore IPI initialization if IRQ1 is not per-CPU interrupt */
	if (!vmm_host_irq_is_per_cpu(vmm_host_irq_get(1))) {
		return VMM_OK;
	}

	/* Register IPI1 interrupt handler */
	rc = vmm_host_irq_register(1, "IPI1", &smp_ipi_handler, NULL);
	if (rc) {
		return rc;
	}

	return VMM_OK;
}
