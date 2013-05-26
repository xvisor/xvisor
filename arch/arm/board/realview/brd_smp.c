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
 * @file brd_smp.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief board specific smp functions
 */

#include <vmm_error.h>
#include <vmm_types.h>
#include <vmm_stdio.h>
#include <vmm_host_io.h>
#include <vmm_host_aspace.h>
#include <vmm_compiler.h>
#include <libs/libfdt.h>
#include <libs/stringlib.h>
#include <realview_plat.h>
#include <smp_scu.h>
#include <gic.h>

static virtual_addr_t scu_base;

int __init arch_smp_init_cpus(void)
{
	u32 ncores;
	int i, rc = VMM_OK;
	const char *aval;
	struct dlist *l;
	struct vmm_devtree_node *node, *cnode;

	node = vmm_devtree_find_compatible(NULL, NULL, "arm,arm11mp-scu");
	
	if (node) {
		/* Map SCU registers */
		rc = vmm_devtree_regmap(node, &scu_base, 0);
		if (rc) {
			return rc;
		}
	} else {
		/* No SCU present */
		scu_base = 0x0;
	}

	if (scu_base) {
		/* Get core count from SCU */
		ncores = scu_get_core_count((void *)scu_base);

		/* Update the cpu_possible bitmap based on SCU */
		for (i = 0; i < CONFIG_CPU_COUNT; i++) {
			if ((i < ncores) &&
			    scu_cpu_core_is_smp((void *)scu_base, i)) {
				vmm_set_cpu_possible(i, TRUE);
			}
		}
	} else {
		/* Rely on "cpus" node when no SCU present */
		node = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPARATOR_STRING
					   "cpus");
		if (!node) {
			return VMM_EFAIL;
		}

		i = 0;
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
	}

	return VMM_OK;
}

extern u8 _start_secondary;

int __init arch_smp_prepare_cpus(unsigned int max_cpus)
{
	int i, rc;
	physical_addr_t _start_secondary_pa;

	/* Get physical address secondary startup code */
	rc = vmm_host_va2pa((virtual_addr_t)&_start_secondary, 
			    &_start_secondary_pa);
	if (rc) {
		return rc;
	}

	/* Update the cpu_present bitmap */
	for (i = 0; i < max_cpus; i++) {
		vmm_set_cpu_present(i, TRUE);
	}

	if (scu_base) {
		/* Enable snooping through SCU */
		scu_enable((void *)scu_base);
	}

	/* Write the entry address for the secondary cpus */
	realview_flags_set((u32)_start_secondary_pa);

	return VMM_OK;
}

int __init arch_smp_start_cpu(u32 cpu)
{
	const struct vmm_cpumask *mask = get_cpu_mask(cpu);

	/* Wakeup target cpu from wfe/wfi by sending an IPI */
	gic_raise_softirq(mask, 0);

	return VMM_OK;
}
