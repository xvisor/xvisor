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
 * @file mmu_lpae.h
 * @author Anup Patel (anup@brainfault.org)
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief MMU interface of LPAE enabled ARM processor
 */

#ifndef __MMU_LPAE_H__
#define __MMU_LPAE_H__

#include <vmm_const.h>

#define ARCH_MMU_STAGE1_ROOT_SIZE_ORDER		12
#define ARCH_MMU_STAGE1_ROOT_ALIGN_ORDER	12

#define ARCH_MMU_STAGE1_NONROOT_INITIAL_COUNT	8

#define ARCH_MMU_STAGE1_NONROOT_SIZE_ORDER	12
#define ARCH_MMU_STAGE1_NONROOT_ALIGN_ORDER	12

/* L0 index Bit[47:39] */
#define TTBL_L0_INDEX_MASK			0x0000FF8000000000ULL
#define TTBL_L0_INDEX_SHIFT			39
#define TTBL_L0_BLOCK_SHIFT			TTBL_L0_INDEX_SHIFT
#define TTBL_L0_BLOCK_SIZE			0x0001000000000000ULL
#define TTBL_L0_MAP_MASK			(~(TTBL_L0_BLOCK_SIZE - 1))
/* L1 index Bit[38:30] */
#define TTBL_L1_INDEX_MASK			0x0000007FC0000000ULL
#define TTBL_L1_INDEX_SHIFT			30
#define TTBL_L1_BLOCK_SHIFT			TTBL_L1_INDEX_SHIFT
#define TTBL_L1_BLOCK_SIZE			0x0000000040000000ULL
#define TTBL_L1_MAP_MASK			(~(TTBL_L1_BLOCK_SIZE - 1))
/* L2 index Bit[29:21] */
#define TTBL_L2_INDEX_MASK			0x000000003FE00000ULL
#define TTBL_L2_INDEX_SHIFT			21
#define TTBL_L2_BLOCK_SHIFT			TTBL_L2_INDEX_SHIFT
#define TTBL_L2_BLOCK_SIZE			0x0000000000200000ULL
#define TTBL_L2_MAP_MASK			(~(TTBL_L2_BLOCK_SIZE - 1))
/* L3 index Bit[20:12] */
#define TTBL_L3_INDEX_MASK			0x00000000001FF000ULL
#define TTBL_L3_INDEX_SHIFT			12
#define TTBL_L3_BLOCK_SHIFT			TTBL_L3_INDEX_SHIFT
#define TTBL_L3_BLOCK_SIZE			0x0000000000001000ULL
#define TTBL_L3_MAP_MASK			(~(TTBL_L3_BLOCK_SIZE - 1))

/* TTBL Common Attributes */
#define TTBL_UPPER_MASK				0xFFF0000000000000ULL
#define TTBL_UPPER_SHIFT			52
#define TTBL_OUTADDR_MASK			0x000000FFFFFFF000ULL
#define TTBL_OUTADDR_SHIFT			12
#define TTBL_AP_SRW_U				0x0
#define TTBL_AP_S_URW				0x1
#define TTBL_AP_SR_U				0x2
#define TTBL_AP_S_UR				0x3
#define TTBL_HAP_NOACCESS			0x0
#define TTBL_HAP_READONLY			0x1
#define TTBL_HAP_WRITEONLY			0x2
#define TTBL_HAP_READWRITE			0x3
#define TTBL_SH_NON_SHAREABLE			0x0
#define TTBL_SH_OUTER_SHAREABLE			0x2
#define TTBL_SH_INNER_SHAREABLE			0x3
#define TTBL_LOWER_MASK				0x0000000000000FFCULL
#define TTBL_LOWER_SHIFT			2
#define TTBL_TABLE_MASK				0x0000000000000002ULL
#define TTBL_TABLE_SHIFT			1
#define TTBL_VALID_MASK				0x0000000000000001ULL
#define TTBL_VALID_SHIFT			0

/* TTBL Stage1 Table Attributes */
#define TTBL_STAGE1_TABLE_NS_MASK		0x8000000000000000ULL
#define TTBL_STAGE1_TABLE_NS_SHIFT		63
#define TTBL_STAGE1_TABLE_AP_MASK		0x6000000000000000ULL
#define TTBL_STAGE1_TABLE_AP_SHIFT		61
#define TTBL_STAGE1_TABLE_XN_MASK		0x1000000000000000ULL
#define TTBL_STAGE1_TABLE_XN_SHIFT		60
#define TTBL_STAGE1_TABLE_PXN_MASK		0x0800000000000000ULL
#define TTBL_STAGE1_TABLE_PXN_SHIFT		59

/* TTBL Stage1 Block Upper Attributes */
#define TTBL_STAGE1_UPPER_XN_MASK		0x0040000000000000ULL
#define TTBL_STAGE1_UPPER_XN_SHIFT		54
#define TTBL_STAGE1_UPPER_PXN_MASK		0x0020000000000000ULL
#define TTBL_STAGE1_UPPER_PXN_SHIFT		53
#define TTBL_STAGE1_UPPER_CONT_MASK		0x0010000000000000ULL
#define TTBL_STAGE1_UPPER_CONT_SHIFT		52

/* TTBL Stage1 Block Lower Attributes */
#define TTBL_STAGE1_LOWER_NG_MASK		0x0000000000000800ULL
#define TTBL_STAGE1_LOWER_NG_SHIFT		11
#define TTBL_STAGE1_LOWER_AF_MASK		0x0000000000000400ULL
#define TTBL_STAGE1_LOWER_AF_SHIFT		10
#define TTBL_STAGE1_LOWER_SH_MASK		0x0000000000000300ULL
#define TTBL_STAGE1_LOWER_SH_SHIFT		8
#define TTBL_STAGE1_LOWER_AP_MASK		0x00000000000000C0ULL
#define TTBL_STAGE1_LOWER_AP_SHIFT		6
#define TTBL_STAGE1_LOWER_NS_MASK		0x0000000000000020ULL
#define TTBL_STAGE1_LOWER_NS_SHIFT		5
#define TTBL_STAGE1_LOWER_AINDEX_MASK		0x000000000000001CULL
#define TTBL_STAGE1_LOWER_AINDEX_SHIFT		2

/* TTBL Stage2 Block Upper Attributes */
#define TTBL_STAGE2_UPPER_MASK			0xFFF0000000000000ULL
#define TTBL_STAGE2_UPPER_SHIFT			52
#define TTBL_STAGE2_UPPER_XN_MASK		0x0040000000000000ULL
#define TTBL_STAGE2_UPPER_XN_SHIFT		54
#define TTBL_STAGE2_UPPER_CONT_MASK		0x0010000000000000ULL
#define TTBL_STAGE2_UPPER_CONT_SHIFT		52

/* TTBL Stage2 Block Lower Attributes */
#define TTBL_STAGE2_LOWER_MASK			0x0000000000000FFCULL
#define TTBL_STAGE2_LOWER_SHIFT			2
#define TTBL_STAGE2_LOWER_AF_MASK		0x0000000000000400ULL
#define TTBL_STAGE2_LOWER_AF_SHIFT		10
#define TTBL_STAGE2_LOWER_SH_MASK		0x0000000000000300ULL
#define TTBL_STAGE2_LOWER_SH_SHIFT		8
#define TTBL_STAGE2_LOWER_HAP_MASK		0x00000000000000C0ULL
#define TTBL_STAGE2_LOWER_HAP_SHIFT		6
#define TTBL_STAGE2_LOWER_MEMATTR_MASK		0x000000000000003CULL
#define TTBL_STAGE2_LOWER_MEMATTR_SHIFT		2

/* Attribute Indices */
#define AINDEX_DEVICE_nGnRnE			0
#define AINDEX_DEVICE_nGnRE			1
#define AINDEX_DEVICE_nGRE			2
#define AINDEX_DEVICE_GRE			3
#define AINDEX_NORMAL_WT			4
#define AINDEX_NORMAL_WB			5
#define AINDEX_NORMAL_NC			6

#ifndef __ASSEMBLY__

#include <vmm_types.h>

typedef u64 arch_pte_t;

struct arch_pgflags {
	/* upper */
	u8 xn;
	u8 pxn;
	u8 cont;
	/* lower */
	u8 ng;
	u8 af;
	u8 sh;
	u8 ap;
	u8 ns;
	u8 aindex;
	u8 memattr;
};
typedef struct arch_pgflags arch_pgflags_t;

int arch_mmu_pgtbl_min_align_order(int stage);

int arch_mmu_pgtbl_align_order(int stage, int level);

int arch_mmu_pgtbl_size_order(int stage, int level);

void arch_mmu_stage2_tlbflush(bool remote, bool use_vmid, u32 vmid,
			      physical_addr_t gpa, physical_size_t gsz);

void arch_mmu_stage1_tlbflush(bool remote, bool use_asid, u32 asid,
			      virtual_addr_t va, virtual_size_t sz);

bool arch_mmu_valid_block_size(physical_size_t sz);

int arch_mmu_start_level(int stage);

physical_size_t arch_mmu_level_block_size(int stage, int level);

int arch_mmu_level_block_shift(int stage, int level);

physical_addr_t arch_mmu_level_map_mask(int stage, int level);

int arch_mmu_level_index(physical_addr_t ia, int stage, int level);

int arch_mmu_level_index_shift(int stage, int level);

void arch_mmu_pgflags_set(arch_pgflags_t *flags, int stage, u32 mflags);

void arch_mmu_pte_sync(arch_pte_t *pte, int stage, int level);

void arch_mmu_pte_clear(arch_pte_t *pte, int stage, int level);

bool arch_mmu_pte_is_valid(arch_pte_t *pte, int stage, int level);

physical_addr_t arch_mmu_pte_addr(arch_pte_t *pte, int stage, int level);

void arch_mmu_pte_flags(arch_pte_t *pte, int stage, int level,
			arch_pgflags_t *out_flags);

void arch_mmu_pte_set(arch_pte_t *pte, int stage, int level,
		      physical_addr_t pa, arch_pgflags_t *flags);

bool arch_mmu_pte_is_table(arch_pte_t *pte, int stage, int level);

physical_addr_t arch_mmu_pte_table_addr(arch_pte_t *pte, int stage, int level);

void arch_mmu_pte_set_table(arch_pte_t *pte, int stage, int level,
			    physical_addr_t tbl_pa);

int arch_mmu_test_nested_pgtbl(physical_addr_t s2_tbl_pa,
				bool s1_avail, physical_addr_t s1_tbl_pa,
				u32 flags, virtual_addr_t addr,
				physical_addr_t *out_addr,
				u32 *out_fault_flags);

physical_addr_t arch_mmu_stage2_current_pgtbl_addr(void);

u32 arch_mmu_stage2_current_vmid(void);

int arch_mmu_stage2_change_pgtbl(bool have_vmid, u32 vmid,
				 physical_addr_t tbl_phys);

#endif /* !__ASSEMBLY__ */

#endif
