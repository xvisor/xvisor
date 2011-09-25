/**
 * Copyright (c) 2011 Anup Patel.
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
 * @file cpu_mmu.h
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief Header File for memory management unit
 */
#ifndef _CPU_MMU_H__
#define _CPU_MMU_H__

#include <vmm_types.h>
#include <vmm_list.h>

typedef struct cpu_page cpu_page_t;
typedef struct cpu_l1tbl cpu_l1tbl_t;
typedef struct cpu_l2tbl cpu_l2tbl_t;

struct cpu_page {
	virtual_addr_t va;
	physical_addr_t pa;
	virtual_size_t sz;
	u32 ns:1;
	u32 ng:1;
	u32 s:1;
	u32 tex:3;
	u32 ap:3;
	u32 imp:1;
	u32 dom:4;
	u32 xn:1;
	u32 c:1;
	u32 b:1;
	u32 pad:15;
} __attribute__((packed));

struct cpu_l2tbl {
	struct dlist head;
	int l2_num;
	cpu_l1tbl_t *l1;
	u32 imp;
	u32 domain;
	physical_addr_t tbl_pa;
	virtual_addr_t tbl_va;
	virtual_addr_t map_va;
	u32 tte_cnt;
};

struct cpu_l1tbl {
	struct dlist head;
	int l1_num;
	physical_addr_t tbl_pa;
	virtual_addr_t tbl_va;
	u32 tte_cnt;
	u32 l2tbl_cnt;
	struct dlist l2tbl_list;
};

/** Estimate good page size */
u32 cpu_mmu_best_page_size(virtual_addr_t va, physical_addr_t pa, u32 availsz);

/** Get page from a given virtual address */
int cpu_mmu_get_page(cpu_l1tbl_t * l1, virtual_addr_t va, cpu_page_t * pg);

/** Unmap a page from given L1 table */
int cpu_mmu_unmap_page(cpu_l1tbl_t * l1, cpu_page_t * pg);

/** Map a page under a given L1 table */
int cpu_mmu_map_page(cpu_l1tbl_t * l1, cpu_page_t * pg);

/** Get reserved page from a given virtual address */
int cpu_mmu_get_reserved_page(virtual_addr_t va, cpu_page_t * pg);

/** Unmap a reserved page */
int cpu_mmu_unmap_reserved_page(cpu_page_t * pg);

/** Map a reserved page */
int cpu_mmu_map_reserved_page(cpu_page_t * pg);

/** Allocate a L1 table */
cpu_l1tbl_t *cpu_mmu_l1tbl_alloc(void);

/** Free a L1 table */
int cpu_mmu_l1tbl_free(cpu_l1tbl_t * l1);

/** Current L1 table */
cpu_l1tbl_t *cpu_mmu_l1tbl_default(void);

/** Current L1 table */
cpu_l1tbl_t *cpu_mmu_l1tbl_current(void);

/** Optimized read word from a physical address */
u32 cpu_mmu_physical_read32(physical_addr_t pa);

/** Change domain access control register */
int cpu_mmu_chdacr(u32 new_dacr);

/** Change translation table base register */
int cpu_mmu_chttbr(cpu_l1tbl_t * l1);

#endif /** _CPU_MMU_H */
