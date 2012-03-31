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
 * @file cpu_mmu.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of memory managment unit for ARMv7a family
 */

#include <vmm_error.h>
#include <vmm_string.h>
#include <vmm_host_aspace.h>
#include <vmm_main.h>
#include <vmm_stdio.h>
#include <vmm_types.h>
#include <arch_cpu.h>
#include <cpu_defines.h>
#include <cpu_cache.h>
#include <cpu_barrier.h>
#include <cpu_inline_asm.h>
#include <cpu_mmu.h>

u8 __attribute__ ((aligned(TTBL_TABLE_SIZE))) defl1_ttbl[TTBL_TABLE_SIZE];
u8 __attribute__ ((aligned(TTBL_TABLE_SIZE))) defl2_ttbl[TTBL_L3_BLOCK_SIZE*2];
u8 __attribute__ ((aligned(TTBL_TABLE_SIZE))) defl3_ttbl[TTBL_L3_BLOCK_SIZE*5];

/* FIXME: */
u32 cpu_mmu_best_page_size(virtual_addr_t va, physical_addr_t pa, u32 availsz)
{
	return TTBL_L3_BLOCK_SIZE;
}

/* FIXME: */
int cpu_mmu_get_page(struct cpu_l1tbl * l1, virtual_addr_t va, struct cpu_page * pg)
{
	return VMM_OK;
}

/* FIXME: */
int cpu_mmu_unmap_page(struct cpu_l1tbl * l1, struct cpu_page * pg)
{
	return VMM_OK;
}

/* FIXME: */
int cpu_mmu_map_page(struct cpu_l1tbl * l1, struct cpu_page * pg)
{
	return VMM_OK;
}

/* FIXME: */
int cpu_mmu_get_reserved_page(virtual_addr_t va, struct cpu_page * pg)
{
	return VMM_OK;
}

/* FIXME: */
int cpu_mmu_unmap_reserved_page(struct cpu_page * pg)
{
	return VMM_OK;
}

/* FIXME: */
int cpu_mmu_map_reserved_page(struct cpu_page * pg)
{
	return VMM_OK;
}

/* FIXME: */
struct cpu_l1tbl *cpu_mmu_l1tbl_alloc(void)
{
	return NULL;
}

/* FIXME: */
int cpu_mmu_l1tbl_free(struct cpu_l1tbl * l1)
{
	return VMM_OK;
}

/* FIXME: */
struct cpu_l1tbl *cpu_mmu_l1tbl_default(void)
{
	return NULL;
}

/* FIXME: */
struct cpu_l1tbl *cpu_mmu_l1tbl_current(void)
{
	return NULL;
}

/* FIXME: */
u32 cpu_mmu_physical_read32(physical_addr_t pa)
{
	return 0;
}

/* FIXME: */
void cpu_mmu_physical_write32(physical_addr_t pa, u32 val)
{
	return;
}

/* FIXME: */
int cpu_mmu_chttbr(struct cpu_l1tbl * l1)
{
	return VMM_OK;
}

/* FIXME: */
int arch_cpu_aspace_map(virtual_addr_t va, 
			virtual_size_t sz, 
			physical_addr_t pa,
			u32 mem_flags)
{
	return VMM_OK;
}

/* FIXME: */
int arch_cpu_aspace_unmap(virtual_addr_t va, 
			 virtual_size_t sz)
{
	return VMM_OK;
}

/* FIXME: */
int arch_cpu_aspace_va2pa(virtual_addr_t va, physical_addr_t * pa)
{
	return VMM_OK;
}

/* FIXME: */
int __init arch_cpu_aspace_init(physical_addr_t * core_resv_pa, 
				virtual_addr_t * core_resv_va,
				virtual_size_t * core_resv_sz,
				physical_addr_t * arch_resv_pa,
				virtual_addr_t * arch_resv_va,
				virtual_size_t * arch_resv_sz)
{
	return VMM_OK;
}
