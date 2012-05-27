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
 * @file cpu_mmu.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief MMU interface of LPAE enabled ARM processor
 */
#ifndef _CPU_MMU_H__
#define _CPU_MMU_H__

#include <vmm_types.h>
#include <list.h>

/** LPAE page/block */
struct cpu_page {
	physical_addr_t ia;
	physical_addr_t oa;
	physical_size_t sz;
	/* upper */
	u32 xn:1;
	u32 pxn:1;
	u32 cont:1;
	/* lower */
	u32 ng:1;
	u32 af:1;
	u32 sh:1;
	u32 ap:2;
	u32 ns:1;
	u32 aindex:3;
	u32 memattr:4;
	/* padding */
	u32 pad:16;
};

/** LPAE translation table */
struct cpu_ttbl {
	struct dlist head;
	struct cpu_ttbl * parent;
	int stage;
	int level;
	physical_addr_t map_ia;
	physical_addr_t tbl_pa;
	virtual_addr_t tbl_va;
	u32 tte_cnt;
	u32 child_cnt;
	struct dlist child_list;
};

/** Estimate good page size */
u32 cpu_mmu_best_page_size(physical_addr_t ia, 
			   physical_addr_t oa, 
			   u32 availsz);

/** Get page from a given virtual address */
int cpu_mmu_get_page(struct cpu_ttbl * ttbl, 
		     physical_addr_t ia, 
		     struct cpu_page * pg);

/** Unmap a page from given translation table */
int cpu_mmu_unmap_page(struct cpu_ttbl * ttbl, struct cpu_page * pg);

/** Map a page under a given translation table */
int cpu_mmu_map_page(struct cpu_ttbl * ttbl, struct cpu_page * pg);

/** Get page from a given virtual address */
int cpu_mmu_get_hypervisor_page(virtual_addr_t va, struct cpu_page * pg);

/** Unmap a page from hypervisor translation table */
int cpu_mmu_unmap_hypervisor_page(struct cpu_page * pg);

/** Map a page in hypervisor translation table */
int cpu_mmu_map_hypervisor_page(struct cpu_page * pg);

/** Allocate a new translation table */
struct cpu_ttbl *cpu_mmu_ttbl_alloc(int stage);

/** Free existing translation table */
int cpu_mmu_ttbl_free(struct cpu_ttbl * ttbl);

/** Get child under a parent translation table */
struct cpu_ttbl *cpu_mmu_ttbl_get_child(struct cpu_ttbl *parent,
					physical_addr_t map_ia,
					bool create);

/** Get hypervisor translation table */
struct cpu_ttbl * cpu_mmu_hypervisor_ttbl(void);

/** Get current stage2 translation table */
struct cpu_ttbl *cpu_mmu_stage2_curttbl(void);

/** Get current stage2 VMID */
u8 cpu_mmu_stage2_curvmid(void);

/** Change translation table for stage2 */
int cpu_mmu_stage2_chttbl(u8 vmid, struct cpu_ttbl * ttbl);

#endif /* _CPU_MMU_H */
