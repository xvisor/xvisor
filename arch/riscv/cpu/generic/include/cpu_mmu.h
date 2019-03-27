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
 * @file cpu_mmu.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief RISC-V MMU interface header
 */

#ifndef __CPU_MMU_H__
#define __CPU_MMU_H__

#ifndef __ASSEMBLY__

#include <vmm_types.h>
#include <vmm_spinlocks.h>
#include <libs/list.h>

/** page/block */
struct cpu_page {
	physical_addr_t ia;
	physical_addr_t oa;
	physical_size_t sz;
	u8 rsw;
	u8 dirty;
	u8 accessed;
	u8 global;
	u8 user;
	u8 execute;
	u8 write;
	u8 read;
	u8 valid;
};

/** page table */
enum cpu_pgtbl_stages {
	PGTBL_STAGE1=1,
	PGTBL_STAGE2=2,
};

/** page table */
struct cpu_pgtbl {
	struct dlist head;
	struct cpu_pgtbl *parent;
	int stage;
	int level;
	physical_addr_t map_ia;
	physical_addr_t tbl_pa;
	vmm_spinlock_t tbl_lock; /*< Lock to protect table contents, 
				      tte_cnt, child_cnt, and child_list 
				  */
	virtual_addr_t tbl_va;
	u32 pte_cnt;
	u32 child_cnt;
	struct dlist child_list;
};

struct cpu_pgtbl *cpu_mmu_pgtbl_alloc(int stage);

int cpu_mmu_pgtbl_free(struct cpu_pgtbl *pgtbl);

struct cpu_pgtbl *cpu_mmu_pgtbl_get_child(struct cpu_pgtbl *parent,
					  physical_addr_t map_ia,
					  bool create);

u64 cpu_mmu_best_page_size(physical_addr_t ia,
			   physical_addr_t oa,
			   u32 availsz);

int cpu_mmu_get_page(struct cpu_pgtbl *pgtbl,
		     physical_addr_t ia, struct cpu_page *pg);

int cpu_mmu_unmap_page(struct cpu_pgtbl *pgtbl, struct cpu_page *pg);

int cpu_mmu_map_page(struct cpu_pgtbl *pgtbl, struct cpu_page *pg);

int cpu_mmu_get_hypervisor_page(virtual_addr_t va, struct cpu_page *pg);

int cpu_mmu_unmap_hypervisor_page(struct cpu_page *pg);

int cpu_mmu_map_hypervisor_page(struct cpu_page *pg);

struct cpu_pgtbl *cpu_mmu_stage2_current_pgtbl(void);

u32 cpu_mmu_stage2_current_vmid(void);

int cpu_mmu_stage2_change_pgtbl(u32 vmid, struct cpu_pgtbl *pgtbl);

#endif

#ifdef CONFIG_64BIT
typedef u64 cpu_pte_t;
#else
typedef u32 cpu_pte_t;
#endif

#define PGTBL_INITIAL_TABLE_COUNT			8
#define PGTBL_TABLE_SIZE				0x00001000
#define PGTBL_TABLE_SIZE_SHIFT				12
#ifdef CONFIG_64BIT
#define PGTBL_TABLE_ENTCNT				512
#define PGTBL_TABLE_ENTSZ				8
#else
#define PGTBL_TABLE_ENTCNT				1024
#define PGTBL_TABLE_ENTSZ				4
#endif
#define PGTBL_PAGE_SIZE					0x00001000
#define PGTBL_PAGE_SIZE_SHIFT				12

#ifdef CONFIG_64BIT
/* L3 index Bit[47:39] */
#define PGTBL_L3_INDEX_MASK				0x0000FF8000000000ULL
#define PGTBL_L3_INDEX_SHIFT				39
#define PGTBL_L3_BLOCK_SIZE				0x0000008000000000ULL
#define PGTBL_L3_MAP_MASK				(~(PGTBL_L3_BLOCK_SIZE - 1))
/* L2 index Bit[38:30] */
#define PGTBL_L2_INDEX_MASK				0x0000007FC0000000ULL
#define PGTBL_L2_INDEX_SHIFT				30
#define PGTBL_L2_BLOCK_SIZE				0x0000000040000000ULL
#define PGTBL_L2_MAP_MASK				(~(PGTBL_L2_BLOCK_SIZE - 1))
/* L1 index Bit[29:21] */
#define PGTBL_L1_INDEX_MASK				0x000000003FE00000ULL
#define PGTBL_L1_INDEX_SHIFT				21
#define PGTBL_L1_BLOCK_SHIFT				21
#define PGTBL_L1_BLOCK_SIZE				0x0000000000200000ULL
#define PGTBL_L1_MAP_MASK				(~(PGTBL_L1_BLOCK_SIZE - 1))
/* L0 index Bit[20:12] */
#define PGTBL_L0_INDEX_MASK				0x00000000001FF000ULL
#define PGTBL_L0_INDEX_SHIFT				12
#define PGTBL_L0_BLOCK_SHIFT				12
#define PGTBL_L0_BLOCK_SIZE				0x0000000000001000ULL
#define PGTBL_L0_MAP_MASK				(~(PGTBL_L0_BLOCK_SIZE - 1))
#else
/* L1 index Bit[31:22] */
#define PGTBL_L1_INDEX_MASK				0xFFC00000UL
#define PGTBL_L1_INDEX_SHIFT				22
#define PGTBL_L1_BLOCK_SHIFT				22
#define PGTBL_L1_BLOCK_SIZE				0x00400000UL
#define PGTBL_L1_MAP_MASK				(~(PGTBL_L1_BLOCK_SIZE - 1))
/* L0 index Bit[21:12] */
#define PGTBL_L0_INDEX_MASK				0x003FF000UL
#define PGTBL_L0_INDEX_SHIFT				12
#define PGTBL_L0_BLOCK_SHIFT				12
#define PGTBL_L0_BLOCK_SIZE				0x00001000UL
#define PGTBL_L0_MAP_MASK				(~(PGTBL_L0_BLOCK_SIZE - 1))
#endif

#define PGTBL_PTE_ADDR_MASK				0x003FFFFFFFFFFC00ULL
#define PGTBL_PTE_ADDR_SHIFT				10
#define PGTBL_PTE_RSW_MASK				0x0000000000000300ULL
#define PGTBL_PTE_RSW_SHIFT				8
#define PGTBL_PTE_DIRTY_MASK				0x0000000000000080ULL
#define PGTBL_PTE_DIRTY_SHIFT				7
#define PGTBL_PTE_ACCESSED_MASK				0x0000000000000040ULL
#define PGTBL_PTE_ACCESSED_SHIFT			6
#define PGTBL_PTE_GLOBAL_MASK				0x0000000000000020ULL
#define PGTBL_PTE_GLOBAL_SHIFT				5
#define PGTBL_PTE_USER_MASK				0x0000000000000010ULL
#define PGTBL_PTE_USER_SHIFT				4
#define PGTBL_PTE_EXECUTE_MASK				0x0000000000000008ULL
#define PGTBL_PTE_EXECUTE_SHIFT				3
#define PGTBL_PTE_WRITE_MASK				0x0000000000000004ULL
#define PGTBL_PTE_WRITE_SHIFT				2
#define PGTBL_PTE_READ_MASK				0x0000000000000002ULL
#define PGTBL_PTE_READ_SHIFT				1
#define PGTBL_PTE_PERM_MASK				(PGTBL_PTE_EXECUTE_MASK | \
							 PGTBL_PTE_WRITE_MASK | \
							 PGTBL_PTE_READ_MASK)
#define PGTBL_PTE_VALID_MASK				0x0000000000000001ULL
#define PGTBL_PTE_VALID_SHIFT				0

#endif
