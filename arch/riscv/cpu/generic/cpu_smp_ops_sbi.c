/**
 * Copyright (c) 2020 Western Digital Corporation or its affiliates.
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
 * @file cpu_smp_ops_sbi.c
 * @author Anup Patel (anup.patel@wdc.com)
 * @brief SBI HSM based SMP operations
 */

#include <vmm_error.h>
#include <vmm_smp.h>
#include <vmm_delay.h>
#include <vmm_host_io.h>
#include <vmm_host_irq.h>
#include <vmm_host_aspace.h>
#include <vmm_stdio.h>

#include <cpu_sbi.h>
#include <cpu_smp_ops.h>
#include <riscv_sbi.h>

static int sbi_hart_start(unsigned long hartid, unsigned long saddr,
			  unsigned long priv)
{
	struct sbiret ret;

	ret = sbi_ecall(SBI_EXT_HSM, SBI_EXT_HSM_HART_START,
			hartid, saddr, priv, 0, 0, 0);
	if (ret.error)
		return sbi_err_map_xvisor_errno(ret.error);
	else
		return 0;
}

bool smp_sbi_ops_available(void)
{
	bool ret = FALSE;

	if (sbi_probe_extension(SBI_EXT_HSM) > 0) {
		ret = TRUE;
	}

	return ret;
}

static void __init smp_sbi_ops_init(void)
{
	/* For now nothing to do here. */
}

static int __init smp_sbi_cpu_init(struct vmm_devtree_node *node,
				   unsigned int cpu)
{
	/* For now nothing to do here. */
	return VMM_OK;
}

static int __init smp_sbi_cpu_prepare(unsigned int cpu)
{
	/* For now nothing to do here. */
	return VMM_OK;
}

extern u8 _start_secondary_nopen;

static int __init smp_sbi_cpu_boot(unsigned int cpu)
{
	int rc;
	physical_addr_t _start_secondary_nopen_pa;

	/* Get physical address secondary startup code */
	rc = vmm_host_va2pa((virtual_addr_t)&_start_secondary_nopen,
			    &_start_secondary_nopen_pa);
	if (rc) {
		vmm_printf("%s: failed to get phys addr for entry point\n",
			   __func__);
		return rc;
	}

	/* Do SBI call */
	rc = sbi_hart_start(smp_logical_map(cpu),
			    _start_secondary_nopen_pa, 0);
	if (rc) {
		vmm_printf("%s: failed to start HART\n",
			   __func__);
		return rc;
	}

	/* Wait for some-time */
	vmm_udelay(100000);

	return VMM_OK;
}

static void __cpuinit smp_sbi_cpu_postboot(void)
{
	/* For now nothing to do here. */
}

struct smp_operations smp_sbi_ops = {
	.name = "sbi",
	.ops_init = smp_sbi_ops_init,
	.cpu_init = smp_sbi_cpu_init,
	.cpu_prepare = smp_sbi_cpu_prepare,
	.cpu_boot = smp_sbi_cpu_boot,
	.cpu_postboot = smp_sbi_cpu_postboot,
};
