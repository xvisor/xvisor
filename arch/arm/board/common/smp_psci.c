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

static int psci_to_xvisor_errno(long errno)
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

static int __noinline invoke_psci_fn_smc(unsigned long func,
					 unsigned long arg0,
					 unsigned long arg1,
					 unsigned long arg2)
{
	long ret;

#ifdef CONFIG_ARM64
	asm volatile(
		"mov	x0, %1\n\t"
		"mov	x1, %2\n\t"
		"mov	x2, %3\n\t"
		"mov	x3, %4\n\t"
		"smc	#0    \n\t"
		"mov	%0, x0\n\t"
	: "=r" (ret)
	: "r" (func), "r" (arg0), "r" (arg1), "r" (arg2)
	: "x0", "x1", "x2", "x3", "cc", "memory");
#else
	asm volatile(
		".arch_extension sec\n\t"
		"mov	r0, %1\n\t"
		"mov	r1, %2\n\t"
		"mov	r2, %3\n\t"
		"mov	r3, %4\n\t"
		"smc	#0    \n\t"
		"mov	%0, r0\n\t"
	: "=r" (ret)
	: "r" (func), "r" (arg0), "r" (arg1), "r" (arg2)
	: "r0", "r1", "r2", "r3", "cc", "memory");
#endif

	return psci_to_xvisor_errno(ret);
}

int psci_cpu_suspend(unsigned long power_state, unsigned long entry_point)
{
	return invoke_psci_fn_smc(psci_function_id[PSCI_FN_CPU_SUSPEND],
				  power_state, entry_point, 0);
}

int psci_cpu_off(unsigned long power_state)
{
	return invoke_psci_fn_smc(psci_function_id[PSCI_FN_CPU_OFF],
				  power_state, 0, 0);
}

int psci_cpu_on(unsigned long cpuid, unsigned long entry_point)
{
	return invoke_psci_fn_smc(psci_function_id[PSCI_FN_CPU_ON],
				  cpuid, entry_point, 0);
}

int psci_migrate(unsigned long cpuid)
{
	return invoke_psci_fn_smc(psci_function_id[PSCI_FN_MIGRATE],
				  cpuid, 0, 0);
}

static struct vmm_devtree_nodeid psci_matches[] = {
	{.compatible = "arm,psci"},
	{ /* end of list */ },
};

static int __init psci_smp_init(struct vmm_devtree_node *node,
				unsigned int cpu)
{
	int rc;
	const char *method;
	static struct vmm_devtree_node *psci = NULL;

	if (psci) {
		return VMM_OK;
	}

	/* look for node with PSCI compatible string */
	psci = vmm_devtree_find_matching(NULL, psci_matches);
	if (!psci) {
		vmm_printf("%s: Failed to find psci node\n", __func__);
		return VMM_ENOTAVAIL;
	}

	/* it should have a "method" attibute equal to "smc" */
	rc = vmm_devtree_read_string(psci, "method", &method);
	if (rc) {
		vmm_printf("%s: Can't find 'method' attribute\n",
			   __func__);
		return rc;
	}
#if 0
	/* For some reason, this does not work for now. */
	/* To be investigated. */
	if (strcmp(method, "smc")) {
		vmm_printf("%s: 'method' is not 'smc'\n",
			   __func__);
		return VMM_EINVALID;
	}
#endif

	/* retrieve the "cpu_on" attribute */
	rc = vmm_devtree_read_u32(psci, "cpu_on",
			&psci_function_id[PSCI_FN_CPU_ON]);
	if (rc) {
		vmm_printf("%s: Can't find 'cpu_on' attribute\n",
			   __func__);
		return rc;
	}

	/* retrieve the "cpu_suspend" attribute */
	rc = vmm_devtree_read_u32(psci, "cpu_suspend",
			&psci_function_id[PSCI_FN_CPU_SUSPEND]);
	if (rc) {
		vmm_printf("%s: Can't find 'cpu_suspend' attribute\n",
			   __func__);
		return rc;
	}

	/* retrieve the "cpu_off" attribute */
	rc = vmm_devtree_read_u32(psci, "cpu_off",
			&psci_function_id[PSCI_FN_CPU_OFF]);
	if (rc) {
		vmm_printf("%s: Can't find 'cpu_off' attribute\n",
			   __func__);
		return rc;
	}

	/* retrieve the "migrate" attribute */
	rc = vmm_devtree_read_u32(psci, "migrate",
			&psci_function_id[PSCI_FN_MIGRATE]);
	if (rc) {
		vmm_printf("%s: Can't find 'migrate' attribute\n",
			   __func__);
		return rc;
	}

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

	return psci_cpu_on(cpu, _start_secondary_pa);
}

struct smp_operations psci_smp_ops = {
	.name = "psci",
	.cpu_init = psci_smp_init,
	.cpu_prepare = psci_smp_prepare,
	.cpu_boot = psci_smp_boot,
};

SMP_OPS_DECLARE(psci_smp, &psci_smp_ops);
