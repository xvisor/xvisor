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
 * @file smp_ops_default.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Default SMP operations
 */

#include <vmm_error.h>
#include <vmm_smp.h>
#include <vmm_delay.h>
#include <vmm_host_io.h>
#include <vmm_host_irq.h>
#include <vmm_host_aspace.h>
#include <arch_barrier.h>

#include <smp_ops.h>

static void __init smp_default_ops_init(void)
{
	/* For now nothing to do here. */
}

static int __init smp_default_cpu_init(struct vmm_devtree_node *node,
					unsigned int cpu)
{
	/* For now nothing to do here. */
	return VMM_OK;
}

static int __init smp_default_cpu_prepare(unsigned int cpu)
{
	/* For now nothing to do here. */
	return VMM_OK;
}

static int __init smp_default_cpu_boot(unsigned int cpu)
{
	/* Update the pen release flag. */
	smp_write_pen_release(smp_logical_map(cpu));

	/* Wait for some-time */
	vmm_udelay(100000);

	/* Check pen value */
	if (smp_read_pen_release() != HARTID_INVALID) {
		return VMM_ENOSYS;
	}

	return VMM_OK;
}

static void __cpuinit smp_default_cpu_postboot(void)
{
	/* Let the primary processor know we're out of the pen. */
	smp_write_pen_release(HARTID_INVALID);
}

struct smp_operations smp_default_ops = {
	.name = "default",
	.ops_init = smp_default_ops_init,
	.cpu_init = smp_default_cpu_init,
	.cpu_prepare = smp_default_cpu_prepare,
	.cpu_boot = smp_default_cpu_boot,
	.cpu_postboot = smp_default_cpu_postboot,
};
SMP_OPS_DECLARE(smp_default, &smp_default_ops);
