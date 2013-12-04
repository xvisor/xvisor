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
 * @file smp_ops.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Common SMP operations
 *
 *
 * Adapted from linux/arch/arm64/kernel/cpu_ops.c
 *
 * Copyright (C) 2013 ARM Ltd.
 *
 * The original source is licensed under GPL.
 *
 *
 * Adapted from linux/arch/arm64/kernel/smp.c
 *
 * Copyright (C) 2012 ARM Ltd.
 *
 * The original source is licensed under GPL.
 */

#include <vmm_error.h>
#include <vmm_types.h>
#include <vmm_limits.h>
#include <vmm_smp.h>
#include <vmm_compiler.h>
#include <vmm_cache.h>
#include <vmm_stdio.h>
#include <libs/stringlib.h>

#include <cpu_inline_asm.h>
#include <smp_ops.h>

#undef DEBUG

#ifdef DEBUG
#define DPRINTF(msg...)		vmm_printf(msg)
#else
#define DPRINTF(msg...)
#endif

volatile unsigned long start_secondary_pen_release = INVALID_HWID;
u64 __smp_logical_map[CONFIG_CPU_COUNT] = { [0 ... CONFIG_CPU_COUNT-1] = INVALID_HWID };

static LIST_HEAD(smp_ops_list);
static const struct smp_operations *smp_cpu_ops[CONFIG_CPU_COUNT];

/*
 * Write secondary_holding_pen_release in a way that is guaranteed to be
 * visible to all observers, irrespective of whether they're taking part
 * in coherency or not.  This is necessary for the hotplug code to work
 * reliably.
 */
void smp_write_pen_release(unsigned long val)
{
	virtual_addr_t start = (virtual_addr_t)&start_secondary_pen_release;
	unsigned long size = sizeof(start_secondary_pen_release);

	start_secondary_pen_release = val;
	vmm_flush_dcache_range(start, start + size);
}

unsigned long smp_read_pen_release(void)
{
	return start_secondary_pen_release;
}

static const struct smp_operations * __init smp_get_ops(const char *name)
{
	u32 i, count;
	const struct smp_operations *ops;
	struct vmm_devtree_nidtbl_entry *nide;

	count = vmm_devtree_nidtbl_count();
	for (i = 0; i < count; i++) {
		nide = vmm_devtree_nidtbl_get(i);
		if (strcmp(nide->subsys, "smp_ops")) {
			continue;
		}
		ops = nide->nodeid.data;
		if (!strcmp(name, ops->name)) {
			return ops;
		}
	}
	
	return NULL;
}

/*
 * Read a cpu's enable method from the device tree and 
 * record it in smp_cpu_ops.
 */
static int __init smp_read_ops(struct vmm_devtree_node *dn, int cpu)
{
	const char *enable_method = vmm_devtree_attrval(dn,
					VMM_DEVTREE_ENABLE_METHOD_ATTR_NAME);

	if (!enable_method) {
		/*
		 * The boot CPU may not have an enable method (e.g. when
		 * spin-table is used for secondaries). Don't warn spuriously.
		 */
		if (cpu != 0) {
			vmm_printf("%s: missing enable-method property\n",
				   dn->name);
		}
		return VMM_ENOENT;
	}

	smp_cpu_ops[cpu] = smp_get_ops(enable_method);
	if (!smp_cpu_ops[cpu]) {
		vmm_printf("%s: unsupported enable-method property: %s\n",
			   dn->name, enable_method);
		return VMM_ENOTAVAIL;
	}

	return 0;
}

int __init arch_smp_init_cpus(void)
{
	physical_addr_t *reg;
	unsigned int i, cpu = 1;
	bool bootcpu_valid = false;
	struct vmm_devtree_node *dn, *cpus;

	cpus = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPARATOR_STRING "cpus");
	if (!cpus) {
		vmm_printf("%s: Failed to find cpus node\n",
			   __func__);
		return VMM_ENOTAVAIL;
	}

	dn = NULL;
	devtree_for_each_node(dn, cpus) {
		break;
	}
	if (!dn) {
		vmm_printf("%s: Failed to find node for boot cpu\n",
			   __func__);
		return VMM_ENODEV;
	}

	reg = vmm_devtree_attrval(dn, VMM_DEVTREE_REG_ATTR_NAME);
	if (!reg) {
		vmm_printf("%s: Failed to find reg property for boot cpu\n",
			   __func__);
		return VMM_ENODEV;
	}
	smp_logical_map(0) = reg[0];
	smp_read_ops(dn, 0);

	dn = NULL;
	devtree_for_each_node(dn, cpus) {
		u64 hwid;

		/*
		 * A cpu node with missing "reg" property is
		 * considered invalid to build a smp_logical_map
		 * entry.
		 */
		reg = vmm_devtree_attrval(dn, VMM_DEVTREE_REG_ATTR_NAME);
		if (!reg) {
			vmm_printf("%s: missing reg property\n", dn->name);
			goto next;
		}
		hwid = reg[0];

		/*
		 * Non affinity bits must be set to 0 in the DT
		 */
		if (hwid & ~MPIDR_HWID_BITMASK) {
			vmm_printf("%s: invalid reg property\n", dn->name);
			goto next;
		}

		/*
		 * Duplicate MPIDRs are a recipe for disaster. Scan
		 * all initialized entries and check for
		 * duplicates. If any is found just ignore the cpu.
		 * smp_logical_map was initialized to INVALID_HWID to
		 * avoid matching valid MPIDR values.
		 */
		for (i = 1; (i < cpu) && (i < CONFIG_CPU_COUNT); i++) {
			if (smp_logical_map(i) == hwid) {
				vmm_printf("%s: duplicate cpu reg properties"
					   " in the DT\n", dn->name);
				goto next;
			}
		}

		/*
		 * The numbering scheme requires that the boot CPU
		 * must be assigned logical id 0. Record it so that
		 * the logical map built from DT is validated and can
		 * be used.
		 */
		if (hwid == smp_logical_map(0)) {
			if (bootcpu_valid) {
				vmm_printf("%s: duplicate boot cpu reg property"
					   " in DT\n", dn->name);
				goto next;
			}

			bootcpu_valid = TRUE;

			/*
			 * smp_logical_map has already been
			 * initialized and the boot cpu doesn't need
			 * the enable-method so continue without
			 * incrementing cpu.
			 */
			continue;
		}

		if (cpu >= CONFIG_CPU_COUNT)
			goto next;

		if (smp_read_ops(dn, cpu) != 0)
			goto next;

		if (smp_cpu_ops[cpu]->cpu_init(dn, cpu))
			goto next;

		DPRINTF("%s: smp logical map CPU%0 -> HWID 0x%llx\n",
			__func__, cpu, hwid);
		smp_logical_map(cpu) = hwid;
next:
		cpu++;
	}

	/* sanity check */
	if (cpu > CONFIG_CPU_COUNT) {
		vmm_printf("%s: no. of cores (%d) greater than configured"
			   " maximum of %d - clipping\n",
			   __func__, cpu, CONFIG_CPU_COUNT);
	}

	if (!bootcpu_valid) {
		vmm_printf("%s: DT missing boot CPU MPIDR, not enabling"
			   " secondaries\n", __func__);
		return VMM_ENODEV;
	}

	/*
	 * All the cpus that made it to the smp_logical_map have been
	 * validated so set them as possible cpus.
	 */
	for (i = 0; i < CONFIG_CPU_COUNT; i++) {
		if (smp_logical_map(i) != INVALID_HWID)
			vmm_set_cpu_possible(i, TRUE);
	}

	return VMM_OK;
}

int __init arch_smp_prepare_cpus(unsigned int max_cpus)
{
	int err;
	unsigned int cpu, ncores = vmm_num_possible_cpus();

	/*
	 * are we trying to boot more cores than exist?
	 */
	if (max_cpus > ncores)
		max_cpus = ncores;

	/* Don't bother if we're effectively UP */
	if (max_cpus <= 1) {
		return VMM_OK;
	}

	/*
	 * Initialise the present map (which describes the set of CPUs
	 * actually populated at the present time) and release the
	 * secondaries from the bootloader.
	 *
	 * Make sure we online at most (max_cpus - 1) additional CPUs.
	 */
	max_cpus--;
	for_each_possible_cpu(cpu) {
		if (max_cpus == 0)
			break;

		if (cpu == vmm_smp_processor_id())
			continue;

		if (!smp_cpu_ops[cpu])
			continue;

		err = smp_cpu_ops[cpu]->cpu_prepare(cpu);
		if (err)
			continue;

		vmm_set_cpu_present(cpu, TRUE);
		max_cpus--;
	}

	return VMM_OK;
}

int __init arch_smp_start_cpu(u32 cpu)
{
	/* Boot SMP callback */
	if (smp_cpu_ops[cpu]->cpu_boot) {
		return smp_cpu_ops[cpu]->cpu_boot(cpu);
	}

	return VMM_ENOSYS;
}

void __cpuinit arch_smp_postboot(void)
{
	u32 cpu = vmm_smp_processor_id();

	/* Postboot SMP callback */
	if (smp_cpu_ops[cpu]->cpu_postboot)
		smp_cpu_ops[cpu]->cpu_postboot();
}

