/**
 * Copyright (c) 2012 Sukanto Ghosh.
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
 * @file brd_smp.c
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief board specific smp functions
 */

#include <vmm_error.h>
#include <vmm_types.h>
#include <vmm_smp.h>
#include <vmm_stdio.h>
#include <vmm_delay.h>
#include <vmm_cache.h>
#include <vmm_host_io.h>
#include <vmm_host_irq.h>
#include <vmm_host_aspace.h>
#include <vmm_compiler.h>
#include <libs/libfdt.h>
#include <libs/stringlib.h>
#include <smp_scu.h>
#include <gic.h>

#define MPIDR_HWID_BITMASK	0xff00ffffffLU

int __init arch_smp_init_cpus(void)
{
	int i = 0;
	const char *aval;
	struct dlist *l;
	struct vmm_devtree_node *node, *cnode;

	node = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPARATOR_STRING
				   "cpus");
	if (!node) {
		return VMM_EFAIL;
	}

	list_for_each(l, &node->child_list) {
		cnode = list_entry(l, struct vmm_devtree_node, head);
		aval = vmm_devtree_attrval(cnode, 
				VMM_DEVTREE_DEVICE_TYPE_ATTR_NAME);
		if (aval && 
		    !strcmp(aval, VMM_DEVTREE_DEVICE_TYPE_VAL_CPU)) {
			vmm_set_cpu_possible(i, TRUE);
			i++;
		}
	}

	return VMM_OK;
}

extern u8 _start_secondary;
volatile unsigned long secondary_holding_pen_release;

/*
 * Write secondary_holding_pen_release in a way that is guaranteed to be
 * visible to all observers, irrespective of whether they're taking part
 * in coherency or not.  This is necessary for the hotplug code to work
 * reliably.
 */
static void write_pen_release(u64 val)
{
	virtual_addr_t start = (virtual_addr_t)&secondary_holding_pen_release;
	unsigned long size = sizeof(secondary_holding_pen_release);

	secondary_holding_pen_release = val;
	vmm_flush_dcache_range(start, start + size);
}

int __init arch_smp_prepare_cpus(unsigned int max_cpus)
{
	int cpu = 0, rc = VMM_OK;
	struct dlist *l;
	struct vmm_devtree_node *node, *cnode;

	node = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPARATOR_STRING
				   "cpus");
	if (!node) {
		return VMM_EFAIL;
	}

	list_for_each(l, &node->child_list) {
		physical_addr_t *release_addr_pa;
		virtual_addr_t release_addr_va;
		physical_addr_t secondary_jump_addr;
		const char *enable_method;

		cnode = list_entry(l, struct vmm_devtree_node, head);

		if (cpu >= max_cpus) {
			break;
		}

		enable_method = vmm_devtree_attrval(cnode, 
					VMM_DEVTREE_ENABLE_METHOD_ATTR_NAME);
		if (!enable_method) {
			vmm_printf("%s: enable-method not set\n", cnode->name);
			continue;
		}

		/* We currently support only the "spin-table" enable-method. */
		if (strcmp(enable_method, "spin-table")) {
			vmm_printf("%s: enable-method \'%s\' not supported\n",
				   cnode->name, enable_method);
			continue;
		}

		release_addr_pa = vmm_devtree_attrval(cnode, 
					VMM_DEVTREE_CPU_RELEASE_ADDR_ATTR_NAME);
		if (!release_addr_pa) {
			vmm_printf("%s: cpu-release-addr not set\n", cnode->name);
			continue;
		}

		release_addr_va = vmm_host_iomap(*release_addr_pa, 
							sizeof(physical_addr_t));
		rc = vmm_host_va2pa((virtual_addr_t)&_start_secondary, 
							&secondary_jump_addr);
		if (rc) {
			vmm_printf("%s: failed to iomap cpu-release-addr\n", 
				   cnode->name);
			continue;
		}
		vmm_writeq(secondary_jump_addr, (void *)release_addr_va);
		vmm_host_iounmap((virtual_addr_t)release_addr_va, 
						sizeof(physical_addr_t));

		/* Send an event to wake up the secondary CPU. */
		asm volatile ("sev");

		vmm_set_cpu_present(cpu, TRUE);

		cpu++;
	}

	return VMM_OK;
}

int __init arch_smp_start_cpu(u32 cpu)
{
	int i = 0;
	struct dlist *l;
	physical_addr_t hwid;
	struct vmm_devtree_node *node, *cnode;

	node = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPARATOR_STRING
				   "cpus");
	if (!node) {
		return VMM_EFAIL;
	}

	list_for_each(l, &node->child_list) {
		if (i++ == cpu) {
			cnode = list_entry(l, struct vmm_devtree_node, head);
			break;
		}
	}

	if (vmm_devtree_regaddr(cnode, &hwid, 0)) {
		return VMM_EFAIL;
	}

	/*
	 * Non affinity bits must be set to 0 in the DT
	 */
	if (hwid & ~MPIDR_HWID_BITMASK) {
		vmm_printf("%s: invalid mpidr value in reg property\n", 
			   cnode->name);
		return VMM_EFAIL;
	}

	write_pen_release(hwid);
	asm volatile ("sev");
	vmm_udelay(1000);

	return VMM_OK;
}

static vmm_irq_return_t smp_ipi_handler(int irq_no, void *dev)
{
	/* Call core code to handle IPI1 */
	vmm_smp_ipi_exec();	

	return VMM_IRQ_HANDLED;
}

void arch_smp_ipi_trigger(const struct vmm_cpumask *dest)
{
	/* Send IPI1 to other cores */
	gic_raise_softirq(dest, 1);
}

int __cpuinit arch_smp_ipi_init(void)
{
	int rc;
	u32 cpu = vmm_smp_processor_id();

	if (!cpu) {
		/* Register IPI1 interrupt handler */
		rc = vmm_host_irq_register(1, "IPI1", &smp_ipi_handler, NULL);
		if (rc) {
			return rc;
		}

		/* Mark IPI1 interrupt as per-cpu */
		rc = vmm_host_irq_mark_per_cpu(1);
		if (rc) {
			return rc;
		}
	}

	/* Explicitly enable IPI1 interrupt */
	gic_enable_ppi(1);

	return VMM_OK;
}
