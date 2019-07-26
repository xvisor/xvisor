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
 * @file smp_ops.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Common SMP operations
 */

#include <vmm_error.h>
#include <vmm_types.h>
#include <vmm_limits.h>
#include <vmm_smp.h>
#include <vmm_compiler.h>
#include <vmm_cache.h>
#include <vmm_delay.h>
#include <vmm_stdio.h>
#include <libs/stringlib.h>
#include <arch_barrier.h>

#include <riscv_csr.h>
#include <smp_ops.h>

#undef DEBUG

#ifdef DEBUG
#define DPRINTF(msg...)		vmm_printf(msg)
#else
#define DPRINTF(msg...)
#endif

extern unsigned long _bootcpu_reg0;

volatile unsigned long start_secondary_pen_release = HARTID_INVALID;
volatile unsigned long start_secondary_smp_id = 0x0;
physical_addr_t __smp_logical_map[CONFIG_CPU_COUNT] =
				{ [0 ... CONFIG_CPU_COUNT-1] = HARTID_INVALID };

static LIST_HEAD(smp_ops_list);
static const struct smp_operations *smp_cpu_ops[CONFIG_CPU_COUNT];

/*
 * Write start_secondary_pen_release and start_secondary_smp_id in a way
 * that is guaranteed to be visible to all observers, irrespective of
 * whether they're taking part in coherency or not.  This is necessary
 * for the hotplug code to work reliably.
 */

void smp_write_pen_release(unsigned long val)
{
	arch_smp_mb();
	start_secondary_pen_release = val;
	vmm_flush_dcache_range((virtual_addr_t)&start_secondary_pen_release,
			(virtual_addr_t)&start_secondary_pen_release +
			sizeof(start_secondary_pen_release));
}

unsigned long smp_read_pen_release(void)
{
	return start_secondary_pen_release;
}

void smp_write_logical_id(unsigned long val)
{
	arch_smp_mb();
	start_secondary_smp_id = val;
	vmm_flush_dcache_range((virtual_addr_t)&start_secondary_smp_id,
			(virtual_addr_t)&start_secondary_smp_id +
			sizeof(start_secondary_smp_id));
}

unsigned long smp_read_logical_id(void)
{
	return start_secondary_smp_id;
}

static int __init smp_read_ops(struct vmm_devtree_node *dn, int cpu)
{
	/* Unlike ARM, we don't have a enable-method property in RISC-V */
	smp_cpu_ops[cpu] = &smp_default_ops;

	return 0;
}

/* Initialize all available SMP OPS */
static void __init smp_init_ops(void)
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
		ops->ops_init();
	}
}

int arch_smp_map_hwid(u32 cpu, unsigned long *hwid)
{
	*hwid = smp_logical_map(cpu);
	return VMM_OK;
}

int __init arch_smp_init_cpus(void)
{
	int rc;
	const char *str;
	unsigned int i, cpu = 1;
	bool bootcpu_valid = false;
	physical_addr_t hwid;
	struct vmm_devtree_node *dn, *cpus;

	smp_init_ops();

	cpus = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPARATOR_STRING "cpus");
	if (!cpus) {
		vmm_printf("%s: Failed to find cpus node\n",
			   __func__);
		return VMM_ENOTAVAIL;
	}

	dn = NULL;
	vmm_devtree_for_each_child(dn, cpus) {
		str = NULL;
		rc = vmm_devtree_read_string(dn,
				VMM_DEVTREE_DEVICE_TYPE_ATTR_NAME, &str);
		if (rc || !str) {
			continue;
		}
		if (strcmp(str, VMM_DEVTREE_DEVICE_TYPE_VAL_CPU)) {
			continue;
		}
		rc = vmm_devtree_read_physaddr(dn,
			VMM_DEVTREE_REG_ATTR_NAME, &hwid);
		if ((rc == VMM_OK) &&
		     (hwid == _bootcpu_reg0)) {
			smp_logical_map(0) = hwid;
			break;
		}
	}
	if (!dn) {
		vmm_printf("%s: Failed to find node for boot cpu\n",
			   __func__);
		vmm_devtree_dref_node(cpus);
		return VMM_ENODEV;
	}

	smp_read_ops(dn, 0);
	vmm_devtree_dref_node(dn);

	dn = NULL;
	vmm_devtree_for_each_child(dn, cpus) {
		str = NULL;
		rc = vmm_devtree_read_string(dn,
				VMM_DEVTREE_DEVICE_TYPE_ATTR_NAME, &str);
		if (rc || !str) {
			continue;
		}
		if (strcmp(str, VMM_DEVTREE_DEVICE_TYPE_VAL_CPU)) {
			continue;
		}

		/*
		 * A cpu node with missing "reg" property is
		 * considered invalid to build a smp_logical_map
		 * entry.
		 */
		rc = vmm_devtree_read_physaddr(dn,
					VMM_DEVTREE_REG_ATTR_NAME, &hwid);
		if (rc) {
			vmm_printf("%s: missing reg property\n", dn->name);
			goto next;
		}

		/*
		 * Non affinity bits must be set to 0 in the DT
		 */
		if (hwid & ~HARTID_HWID_BITMASK) {
			vmm_printf("%s: invalid reg property\n", dn->name);
			goto next;
		}

		/*
		 * Duplicate HARTIDs are a recipe for disaster. Scan
		 * all initialized entries and check for
		 * duplicates. If any is found just ignore the cpu.
		 * smp_logical_map was initialized to HARTID_INVALID to
		 * avoid matching valid HARTID values.
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

	/* De-reference cpus node */
	vmm_devtree_dref_node(cpus);

	/* sanity check */
	if (cpu > CONFIG_CPU_COUNT) {
		vmm_printf("%s: no. of cores (%d) greater than configured"
			   " maximum of %d - clipping\n",
			   __func__, cpu, CONFIG_CPU_COUNT);
	}

	if (!bootcpu_valid) {
		vmm_printf("%s: DT missing boot CPU HARTID, not enabling"
			   " secondaries\n", __func__);
		return VMM_ENODEV;
	}

	/*
	 * All the cpus that made it to the smp_logical_map have been
	 * validated so set them as possible cpus.
	 */
	for (i = 0; i < CONFIG_CPU_COUNT; i++) {
		if (smp_logical_map(i) != HARTID_INVALID)
			vmm_set_cpu_possible(i, TRUE);
	}

	return VMM_OK;
}

int __init arch_smp_prepare_cpus(unsigned int max_cpus)
{
	int err;
	unsigned int cpu;

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
	int rc;

	/* Update logical ID */
	smp_write_logical_id(cpu);

	/* Boot SMP callback */
	if (smp_cpu_ops[cpu]->cpu_boot) {
		rc = smp_cpu_ops[cpu]->cpu_boot(cpu);
	} else {
		rc = VMM_ENOSYS;
	}

	/* Wait 10ms before setting logical ID invalid */
	vmm_udelay(10000);

	/* Set logical ID to invalid value */
	smp_write_logical_id(CONFIG_CPU_COUNT);

	return rc;
}

void __cpuinit arch_smp_postboot(void)
{
	u32 cpu = vmm_smp_processor_id();

	/* Postboot SMP callback */
	if (smp_cpu_ops[cpu]->cpu_postboot)
		smp_cpu_ops[cpu]->cpu_postboot();
}
