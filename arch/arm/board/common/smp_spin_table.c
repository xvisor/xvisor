/**
 * Copyright (c) 2013 Sukanto Ghosh.
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
 * @file smp_spin_table.c
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @author Anup Patel (anup@brainfault.org)
 * @brief Spin Table SMP operations
 *
 * Adapted from linux/arch/arm64/kernel/smp_spin_table.c
 *
 * Copyright (C) 2013 ARM Ltd.
 *
 * The original source is licensed under GPL.
 */

#include <vmm_error.h>
#include <vmm_smp.h>
#include <vmm_delay.h>
#include <vmm_host_io.h>
#include <vmm_host_aspace.h>
#include <drv/gic.h>

#include <smp_ops.h>

static virtual_addr_t clear_addr[CONFIG_CPU_COUNT];
static virtual_addr_t release_addr[CONFIG_CPU_COUNT];

static int __init smp_spin_table_cpu_init(struct vmm_devtree_node *node,
				unsigned int cpu)
{
	int rc;
	physical_addr_t pa;

	/* Map release address */
	rc = vmm_devtree_read_physaddr(node,
			VMM_DEVTREE_CPU_RELEASE_ADDR_ATTR_NAME, &pa);
	if (rc) {
		release_addr[cpu] = 0x0;
	} else {
		release_addr[cpu] = vmm_host_iomap(pa, VMM_PAGE_SIZE);
	}

	/* Map clear address */
	rc = vmm_devtree_read_physaddr(node,
			VMM_DEVTREE_CPU_CLEAR_ADDR_ATTR_NAME, &pa);
	if (rc) {
		clear_addr[cpu] = 0x0;
	} else {
		clear_addr[cpu] = vmm_host_iomap(pa, VMM_PAGE_SIZE);
	}

	return VMM_OK;
}

extern u8 _start_secondary;

static int __init smp_spin_table_cpu_prepare(unsigned int cpu)
{
	int rc;
	physical_addr_t _start_secondary_pa;
#ifndef CONFIG_ARM64
	const struct vmm_cpumask *mask = get_cpu_mask(cpu);
#endif

	/* Get physical address secondary startup code */
	rc = vmm_host_va2pa((virtual_addr_t)&_start_secondary, 
			    &_start_secondary_pa);
	if (rc) {
		return rc;
	}

	/* Write to clear address */
	if (clear_addr[cpu]) {
		vmm_writel(~0x0, (void *)clear_addr[cpu]);
	}

	/* Write to release address */
	if (release_addr[cpu]) {
		vmm_writel((u32)_start_secondary_pa,
					(void *)release_addr[cpu]);
	}

#ifdef CONFIG_ARM64
	/* Send an event to wake up the secondary CPU. */
	asm volatile ("sev");
#else
	/* Wakeup target cpu from wfe/wfi by sending an IPI */
	gic_raise_softirq(mask, 0);
#endif

	return VMM_OK;
}

static int __init smp_spin_table_cpu_boot(unsigned int cpu)
{
	/* Update the pen release flag. */
	smp_write_pen_release(smp_logical_map(cpu));

	/* Send an event to wake up the secondary CPU. */
	asm volatile ("sev");

	/* Wait for some-time */
	vmm_udelay(100000);

	/* Check pen value */
	if (smp_read_pen_release() != INVALID_HWID) {
		return VMM_ENOSYS;
	}

	return VMM_OK;
}

static void __cpuinit smp_spin_table_cpu_postboot(void)
{
	/* Let the primary processor know we're out of the pen. */
	smp_write_pen_release(INVALID_HWID);
}

struct smp_operations smp_spin_table_ops = {
	.name = "spin-table",
	.cpu_init = smp_spin_table_cpu_init,
	.cpu_prepare = smp_spin_table_cpu_prepare,
	.cpu_boot = smp_spin_table_cpu_boot,
	.cpu_postboot = smp_spin_table_cpu_postboot,
};

SMP_OPS_DECLARE(smp_spin_table, &smp_spin_table_ops);

