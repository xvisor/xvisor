/**
 * Copyright (c) 2010-20 Himanshu Chauhan.
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
 * @file cpu_host_aspace.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief CPU specific source file for host virtual address space management.
 */

#include <arch_cpu.h>
#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_string.h>
#include <vmm_host_aspace.h>
#include <cpu_mmu.h>


extern u8 _code_end;
extern u8 _code_start;

int arch_cpu_aspace_map(virtual_addr_t va,
		       virtual_size_t sz,
		       physical_addr_t pa,
		       u32 mem_flags);

int arch_cpu_aspace_init(physical_addr_t * resv_pa,
			virtual_addr_t * resv_va,
			virtual_size_t * resv_sz)
{
	return VMM_OK;
}

int arch_cpu_aspace_map(virtual_addr_t va,
			virtual_size_t sz,
			physical_addr_t pa,
			u32 mem_flags)
{
	return VMM_OK;
}

int arch_cpu_aspace_unmap(virtual_addr_t va,
			 virtual_size_t sz)
{
	return VMM_OK;
}

int arch_cpu_aspace_va2pa(virtual_addr_t va, physical_addr_t * pa)
{
	return VMM_OK;
}

virtual_addr_t arch_code_vaddr_start(void)
{
	return ((virtual_addr_t) 0xC0000000);
}

physical_addr_t arch_code_paddr_start(void)
{
	return ((physical_addr_t) 0);
}

virtual_size_t cpu_code_base_size(void)
{
	return (virtual_size_t)(&_code_end - &_code_start);
}

virtual_size_t arch_code_size(void)
{
	return VMM_ROUNDUP2_PAGE_SIZE(cpu_code_base_size());
}
