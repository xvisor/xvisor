/**
 * Copyright (c) 2012 Anup Patel.
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
 * @file vmm_percpu.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of per-cpu areas 
 */

#include <vmm_error.h>
#include <vmm_cpumask.h>
#include <vmm_host_aspace.h>
#include <vmm_percpu.h>
#include <arch_sections.h>
#include <libs/stringlib.h>

#ifdef CONFIG_SMP

virtual_addr_t __percpu_vaddr[CONFIG_CPU_COUNT] = { 0 };
virtual_addr_t __percpu_offset[CONFIG_CPU_COUNT] = { 0 };

int __init vmm_percpu_init(void)
{
	u32 cpu, pgcount;
	virtual_addr_t base = arch_percpu_vaddr();
	virtual_size_t size = arch_percpu_size();

	size = VMM_ROUNDUP2_PAGE_SIZE(size);
	pgcount = size / VMM_PAGE_SIZE;

	__percpu_vaddr[0] = base;
	__percpu_offset[0] = 0;
	for(cpu = 0; cpu < CONFIG_CPU_COUNT; cpu++) {
		if (cpu == 0) {
			continue;
		}
		__percpu_vaddr[cpu] = vmm_host_alloc_pages(pgcount, 
						VMM_MEMORY_FLAGS_NORMAL);
		if (!__percpu_vaddr[cpu]) {
			return VMM_ENOMEM;
		}
		__percpu_offset[cpu] = __percpu_vaddr[cpu] - base;
		memset((void *)__percpu_vaddr[cpu], 0, VMM_PAGE_SIZE * pgcount);
	}

	return VMM_OK;
}

#else

int __init vmm_percpu_init(void)
{
	/* Don't require to do anything for UP */
	return VMM_OK;
}

#endif
