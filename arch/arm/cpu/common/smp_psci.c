/**
 * Copyright (c) 2014 Jean-Christophe Dubois.
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
 * @file smp_psci.c
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @brief PSCI CPU managment support
 *
 * Adapted from arch/arm/kernel/psci.c
 *
 * Copyright (C) 2012 ARM Limited
 *
 * Author: Will Deacon <will.deacon@arm.com>
 */

#include <vmm_error.h>
#include <vmm_compiler.h>
#include <vmm_host_aspace.h>
#include <vmm_stdio.h>

#include <arm_psci.h>
#include <smp_ops.h>

static void __init psci_smp_ops_init(void)
{
	/* Nothing to do here. */
}

static int __init psci_smp_init(struct vmm_devtree_node *node,
				unsigned int cpu)
{
	/* Nothing to do here. */
	return VMM_OK;
}

static int __init psci_smp_prepare(unsigned int cpu)
{
	/* Nothing to do here. */
	return VMM_OK;
}

extern u8 _start_secondary_nopen;

static int __init psci_smp_boot(unsigned int cpu)
{
	int rc;
	physical_addr_t _start_secondary_pa;

	/* Get physical address secondary startup code */
	rc = vmm_host_va2pa((virtual_addr_t) & _start_secondary_nopen,
			    &_start_secondary_pa);
	if (rc) {
		vmm_printf("%s: failed to get phys addr for entry point\n",
			   __func__);
		return rc;
	}

	return psci_cpu_on(smp_logical_map(cpu), _start_secondary_pa);
}

static struct smp_operations psci_smp_ops = {
	.name = "psci",
	.ops_init = psci_smp_ops_init,
	.cpu_init = psci_smp_init,
	.cpu_prepare = psci_smp_prepare,
	.cpu_boot = psci_smp_boot,
};
SMP_OPS_DECLARE(psci_smp, &psci_smp_ops);
