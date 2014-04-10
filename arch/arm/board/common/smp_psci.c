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
#include <vmm_host_aspace.h>
#include <smp_ops.h>
#include <vmm_stdio.h>
#include <libs/stringlib.h>

enum psci_function {
	PSCI_FN_CPU_SUSPEND,
	PSCI_FN_CPU_ON,
	PSCI_FN_CPU_OFF,
	PSCI_FN_MIGRATE,
	PSCI_FN_MAX,
};

static u32 psci_function_id[PSCI_FN_MAX];

#define PSCI_RET_SUCCESS		0
#define PSCI_RET_EOPNOTSUPP		-1
#define PSCI_RET_EINVAL			-2
#define PSCI_RET_EPERM			-3

static int psci_to_xvisor_errno(int errno)
{
	switch (errno) {
	case PSCI_RET_SUCCESS:
		return VMM_OK;
	case PSCI_RET_EOPNOTSUPP:
		return VMM_EOPNOTSUPP;
	case PSCI_RET_EPERM:
		return VMM_EACCESS;
	case PSCI_RET_EINVAL:
		return VMM_EINVALID;
	default:
		return VMM_EFAIL;
	};
}

extern int psci_smc(u32 function_id, u32 arg0, u32 arg1, u32 arg2);

static int invoke_psci_fn(u32 function_id, u32 arg0, u32 arg1, u32 arg2)
{
	return psci_to_xvisor_errno(psci_smc(function_id, arg0, arg1, arg2));
}

int psci_cpu_suspend(unsigned long power_state, unsigned long entry_point)
{
	return invoke_psci_fn(psci_function_id[PSCI_FN_CPU_SUSPEND],
			      power_state, entry_point, 0);
}

int psci_cpu_off(unsigned long power_state)
{
	return invoke_psci_fn(psci_function_id[PSCI_FN_CPU_OFF], power_state, 0,
			      0);
}

int psci_cpu_on(unsigned long cpuid, unsigned long entry_point)
{
	return invoke_psci_fn(psci_function_id[PSCI_FN_CPU_ON], cpuid,
			      entry_point, 0);
}

int psci_migrate(unsigned long cpuid)
{
	return invoke_psci_fn(psci_function_id[PSCI_FN_MIGRATE], cpuid, 0, 0);
}

static struct vmm_devtree_nodeid psci_matches[] = {
	{.compatible = "arm,psci"},
	{ /* end of list */ },
};

static int __init psci_smp_init(struct vmm_devtree_node *node,
				unsigned int cpu)
{
	static struct vmm_devtree_node *psci = NULL;

	if (!psci) {
		/* look for the /psci node */
		psci = vmm_devtree_find_matching(NULL, psci_matches);
		if (!psci) {
			vmm_printf("%s: Failed to find psci node\n", __func__);
			return VMM_ENOTAVAIL;
		}

		/* it should have a "compatible" attibute containing "arm,psci" */
		/* it should have a "method" attibute equal to "smc" */

		/* retrieve the "cpu_on" attribute */
		if (vmm_devtree_read_u32
		    (psci, "cpu_on",
		     &psci_function_id[PSCI_FN_CPU_ON]) != VMM_OK) {
			vmm_printf("%s: Can't find PSCI 'on' method\n",
				   __func__);
		}

		/* retrieve the "cpu_suspend" attribute */
		if (vmm_devtree_read_u32
		    (psci, "cpu_suspend",
		     &psci_function_id[PSCI_FN_CPU_SUSPEND]) != VMM_OK) {
			vmm_printf("%s: Can't find PSCI 'suspend' method\n",
				   __func__);
		}

		/* retrieve the "cpu_off" attribute */
		if (vmm_devtree_read_u32
		    (psci, "cpu_off",
		     &psci_function_id[PSCI_FN_CPU_OFF]) != VMM_OK) {
			vmm_printf("%s: Can't find PSCI 'off' method\n",
				   __func__);
		}

		/* retrieve the "migrate" attribute */
		if (vmm_devtree_read_u32
		    (psci, "migrate",
		     &psci_function_id[PSCI_FN_MIGRATE]) != VMM_OK) {
			vmm_printf("%s: Can't find PSCI 'migrate' method\n",
				   __func__);
		}
	}

	return VMM_OK;
}

static int __init psci_smp_prepare(unsigned int cpu)
{
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

	return psci_cpu_on(cpu, _start_secondary_pa);
}

struct smp_operations psci_smp_ops = {
	.name = "psci",
	.cpu_init = psci_smp_init,
	.cpu_prepare = psci_smp_prepare,
	.cpu_boot = psci_smp_boot,
};

SMP_OPS_DECLARE(psci_smp, &psci_smp_ops);
