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
 * @file cpu_defines.h
 * @author Anup Patel (anup@brainfault.org)
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief MMU interface of LPAE enabled ARM processor
 */

#ifndef _MMU_LPAE_H__
#define _MMU_LPAE_H__

#ifndef __ASSEMBLY__

#include <vmm_types.h>
#include <vmm_spinlocks.h>
#include <libs/list.h>

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
	u32 sh:2;
	u32 ap:2;
	u32 ns:1;
	u32 aindex:3;
	u32 memattr:4;
	/* padding */
	u32 pad:15;
};

/** LPAE translation table */
struct cpu_ttbl {
	struct dlist head;
	struct cpu_ttbl *parent;
	int stage;
	int level;
	physical_addr_t map_ia;
	physical_addr_t tbl_pa;
	vmm_spinlock_t tbl_lock; /*< Lock to protect table contents, 
				      tte_cnt, child_cnt, and child_list 
				  */
	virtual_addr_t tbl_va;
	u32 tte_cnt;
	u32 child_cnt;
	struct dlist child_list;
};

/** Estimate good page size */
u32 mmu_lpae_best_page_size(physical_addr_t ia, 
			   physical_addr_t oa, 
			   u32 availsz);

/** Get page from a given virtual address */
int mmu_lpae_get_page(struct cpu_ttbl *ttbl, 
		     physical_addr_t ia, 
		     struct cpu_page *pg);

/** Unmap a page from given translation table */
int mmu_lpae_unmap_page(struct cpu_ttbl *ttbl, struct cpu_page *pg);

/** Map a page under a given translation table */
int mmu_lpae_map_page(struct cpu_ttbl *ttbl, struct cpu_page *pg);

/** Get page from a given virtual address */
int mmu_lpae_get_hypervisor_page(virtual_addr_t va, struct cpu_page *pg);

/** Unmap a page from hypervisor translation table */
int mmu_lpae_unmap_hypervisor_page(struct cpu_page *pg);

/** Map a page in hypervisor translation table */
int mmu_lpae_map_hypervisor_page(struct cpu_page *pg);

/** Allocate a new translation table */
struct cpu_ttbl *mmu_lpae_ttbl_alloc(int stage);

/** Free existing translation table */
int mmu_lpae_ttbl_free(struct cpu_ttbl *ttbl);

/** Get child under a parent translation table */
struct cpu_ttbl *mmu_lpae_ttbl_get_child(struct cpu_ttbl *parent,
					physical_addr_t map_ia,
					bool create);

/** Get current stage2 translation table */
struct cpu_ttbl *mmu_lpae_stage2_curttbl(void);

/** Get current stage2 VMID */
u8 mmu_lpae_stage2_curvmid(void);

/** Change translation table for stage2 */
int mmu_lpae_stage2_chttbl(u8 vmid, struct cpu_ttbl *ttbl);

#endif /* !__ASSEMBLY__ */

/* TTBL Generic */
#define TTBL_INITIAL_TABLE_COUNT			8
#define TTBL_TABLE_SIZE					0x00001000
#define TTBL_TABLE_SIZE_SHIFT				12
#define TTBL_TABLE_ENTCNT				512
#define TTBL_TABLE_ENTSZ				8
#define TTBL_STAGE1					1
#define TTBL_STAGE2					2
#define TTBL_LEVEL1					1
#define TTBL_LEVEL2					2
#define TTBL_LEVEL3					3
/* L1 index Bit[38:30] */
#define TTBL_L1_INDEX_MASK				0x0000007FC0000000ULL
#define TTBL_L1_INDEX_SHIFT				30
#define TTBL_L1_BLOCK_SIZE				0x0000000040000000ULL
#define TTBL_L1_MAP_MASK				(~(TTBL_L1_BLOCK_SIZE - 1))
/* L2 index Bit[29:21] */
#define TTBL_L2_INDEX_MASK				0x000000003FE00000ULL
#define TTBL_L2_INDEX_SHIFT				21
#define TTBL_L2_BLOCK_SIZE				0x0000000000200000ULL
#define TTBL_L2_MAP_MASK				(~(TTBL_L2_BLOCK_SIZE - 1))
/* L3 index Bit[20:12] */
#define TTBL_L3_INDEX_MASK				0x00000000001FF000ULL
#define TTBL_L3_INDEX_SHIFT				12
#define TTBL_L3_BLOCK_SIZE				0x0000000000001000ULL
#define TTBL_L3_MAP_MASK				(~(TTBL_L3_BLOCK_SIZE - 1))
#define TTBL_UPPER_MASK					0xFFF0000000000000ULL
#define TTBL_UPPER_SHIFT				52
#define TTBL_OUTADDR_MASK				0x000000FFFFFFF000ULL
#define TTBL_OUTADDR_SHIFT				12
#define TTBL_AP_SRW_U					0x0
#define TTBL_AP_S_URW					0x1
#define TTBL_AP_SR_U					0x2
#define TTBL_AP_S_UR					0x3
#define TTBL_HAP_NOACCESS				0x0
#define TTBL_HAP_READONLY				0x1
#define TTBL_HAP_WRITEONLY				0x2
#define TTBL_HAP_READWRITE				0x3
#define TTBL_SH_NON_SHAREABLE				0x0
#define TTBL_SH_OUTER_SHAREABLE				0x2
#define TTBL_SH_INNER_SHAREABLE				0x3
#define TTBL_LOWER_MASK					0x0000000000000FFCULL
#define TTBL_LOWER_SHIFT				2
#define TTBL_TABLE_MASK					0x0000000000000002ULL
#define TTBL_TABLE_SHIFT				1
#define TTBL_VALID_MASK					0x0000000000000001ULL
#define TTBL_VALID_SHIFT				0

/* TTBL Stage1 Table Attributes */
#define TTBL_STAGE1_TABLE_NS_MASK			0x8000000000000000ULL
#define TTBL_STAGE1_TABLE_NS_SHIFT			63
#define TTBL_STAGE1_TABLE_AP_MASK			0x6000000000000000ULL
#define TTBL_STAGE1_TABLE_AP_SHIFT			61
#define TTBL_STAGE1_TABLE_XN_MASK			0x1000000000000000ULL
#define TTBL_STAGE1_TABLE_XN_SHIFT			60
#define TTBL_STAGE1_TABLE_PXN_MASK			0x0800000000000000ULL
#define TTBL_STAGE1_TABLE_PXN_SHIFT			59

/* TTBL Stage1 Block Upper Attributes */
#define TTBL_STAGE1_UPPER_XN_MASK			0x0040000000000000ULL
#define TTBL_STAGE1_UPPER_XN_SHIFT			54
#define TTBL_STAGE1_UPPER_PXN_MASK			0x0020000000000000ULL
#define TTBL_STAGE1_UPPER_PXN_SHIFT			53
#define TTBL_STAGE1_UPPER_CONT_MASK			0x0010000000000000ULL
#define TTBL_STAGE1_UPPER_CONT_SHIFT			52

/* TTBL Stage1 Block Lower Attributes */
#define TTBL_STAGE1_LOWER_NG_MASK			0x0000000000000800ULL
#define TTBL_STAGE1_LOWER_NG_SHIFT			11
#define TTBL_STAGE1_LOWER_AF_MASK			0x0000000000000400ULL
#define TTBL_STAGE1_LOWER_AF_SHIFT			10
#define TTBL_STAGE1_LOWER_SH_MASK			0x0000000000000300ULL
#define TTBL_STAGE1_LOWER_SH_SHIFT			8
#define TTBL_STAGE1_LOWER_AP_MASK			0x00000000000000C0ULL
#define TTBL_STAGE1_LOWER_AP_SHIFT			6
#define TTBL_STAGE1_LOWER_NS_MASK			0x0000000000000020ULL
#define TTBL_STAGE1_LOWER_NS_SHIFT			5
#define TTBL_STAGE1_LOWER_AINDEX_MASK			0x000000000000001CULL
#define TTBL_STAGE1_LOWER_AINDEX_SHIFT			2

/* TTBL Stage2 Block Upper Attributes */
#define TTBL_STAGE2_UPPER_MASK				0xFFF0000000000000ULL
#define TTBL_STAGE2_UPPER_SHIFT				52
#define TTBL_STAGE2_UPPER_XN_MASK			0x0040000000000000ULL
#define TTBL_STAGE2_UPPER_XN_SHIFT			54
#define TTBL_STAGE2_UPPER_CONT_MASK			0x0010000000000000ULL
#define TTBL_STAGE2_UPPER_CONT_SHIFT			52

/* TTBL Stage2 Block Lower Attributes */
#define TTBL_STAGE2_LOWER_MASK				0x0000000000000FFCULL
#define TTBL_STAGE2_LOWER_SHIFT				2
#define TTBL_STAGE2_LOWER_AF_MASK			0x0000000000000400ULL
#define TTBL_STAGE2_LOWER_AF_SHIFT			10
#define TTBL_STAGE2_LOWER_SH_MASK			0x0000000000000300ULL
#define TTBL_STAGE2_LOWER_SH_SHIFT			8
#define TTBL_STAGE2_LOWER_HAP_MASK			0x00000000000000C0ULL
#define TTBL_STAGE2_LOWER_HAP_SHIFT			6
#define TTBL_STAGE2_LOWER_MEMATTR_MASK			0x000000000000003CULL
#define TTBL_STAGE2_LOWER_MEMATTR_SHIFT			2

/* Attribute Indices */
#define AINDEX_DEVICE_nGnRnE				0
#define AINDEX_NORMAL_WT				1
#define AINDEX_NORMAL_WB				2
#define AINDEX_NORMAL_NC				3

#endif
