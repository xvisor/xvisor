/**
 * Copyright (c) 2020 Anup Patel.
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
 * @file generic_mmu.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Generic MMU interface header
 */

#ifndef __GENERIC_MMU_H__
#define __GENERIC_MMU_H__

#include <vmm_spinlocks.h>
#include <libs/list.h>
#include <arch_mmu.h>

/** MMU page/block */
struct mmu_page {
	physical_addr_t ia;
	physical_addr_t oa;
	physical_size_t sz;
	arch_pgflags_t flags;
};

/** MMU stages */
enum mmu_stage {
	MMU_STAGE_UNKNOWN=0,
	MMU_STAGE1,
	MMU_STAGE2,
	MMU_STAGE_MAX
};

/** MMU page table attributes */
#define MMU_ATTR_REMOTE_TLB_FLUSH	(1 << 0)
#define MMU_ATTR_HW_TAG_VALID		(1 << 1)

/** MMU page table */
struct mmu_pgtbl {
	struct dlist head;
	struct mmu_pgtbl *parent;
	enum mmu_stage stage;
	int level;
	u32 attr;
	u32 hw_tag;
	physical_addr_t map_ia;
	physical_addr_t tbl_pa;
	vmm_spinlock_t tbl_lock; /*< Lock to protect table contents, 
				      pte_cnt, child_cnt, and child_list
				  */
	virtual_addr_t tbl_va;
	virtual_size_t tbl_sz;
	u32 pte_cnt;
	u32 child_cnt;
	struct dlist child_list;
};

/* MMU stage1 page table symbols
 *
 * Note: Architecture specific initial page table setup can use this symbols
 * as hypervisor (i.e. stage1) page table.
 */
extern u8 stage1_pgtbl_root[];
extern u8 stage1_pgtbl_nonroot[];

u64 mmu_pgtbl_count(int stage, int level);

struct mmu_pgtbl *mmu_pgtbl_find(int stage, physical_addr_t tbl_pa);

struct mmu_pgtbl *mmu_pgtbl_alloc(int stage, int level, u32 attr, u32 hw_tag);

int mmu_pgtbl_free(struct mmu_pgtbl *pgtbl);

static inline enum mmu_stage mmu_pgtbl_stage(struct mmu_pgtbl *pgtbl)
{
	return (pgtbl) ? pgtbl->stage : MMU_STAGE_UNKNOWN;
}

static inline int mmu_pgtbl_level(struct mmu_pgtbl *pgtbl)
{
	return (pgtbl) ? pgtbl->level : -1;
}

static inline bool mmu_pgtbl_need_remote_tlbflush(struct mmu_pgtbl *pgtbl)
{
	return (pgtbl && (pgtbl->attr & MMU_ATTR_REMOTE_TLB_FLUSH)) ?
		TRUE : FALSE;
}

static inline bool mmu_pgtbl_has_hw_tag(struct mmu_pgtbl *pgtbl)
{
	return (pgtbl && (pgtbl->attr & MMU_ATTR_HW_TAG_VALID)) ?
		TRUE : FALSE;
}

static inline u32 mmu_pgtbl_hw_tag(struct mmu_pgtbl *pgtbl)
{
	return (pgtbl) ? pgtbl->hw_tag : 0;
}

static inline physical_addr_t mmu_pgtbl_map_addr(struct mmu_pgtbl *pgtbl)
{
	return (pgtbl) ? pgtbl->map_ia : 0;
}

static inline physical_addr_t mmu_pgtbl_map_addr_end(struct mmu_pgtbl *pgtbl)
{
	if (!pgtbl) {
		return 0;
	}

	return (pgtbl->map_ia +
		((pgtbl->tbl_sz / sizeof(arch_pte_t)) *
		 arch_mmu_level_block_size(pgtbl->stage, pgtbl->level))) - 1;
}

static inline physical_addr_t mmu_pgtbl_physical_addr(struct mmu_pgtbl *pgtbl)
{
	return (pgtbl) ? pgtbl->tbl_pa : 0;
}

static inline virtual_size_t mmu_pgtbl_size(struct mmu_pgtbl *pgtbl)
{
	return (pgtbl) ? pgtbl->tbl_sz : 0;
}

struct mmu_pgtbl *mmu_pgtbl_get_child(struct mmu_pgtbl *parent,
					  physical_addr_t map_ia,
					  bool create);

int mmu_get_page(struct mmu_pgtbl *pgtbl,
		     physical_addr_t ia, struct mmu_page *pg);

int mmu_unmap_page(struct mmu_pgtbl *pgtbl, struct mmu_page *pg);

int mmu_map_page(struct mmu_pgtbl *pgtbl, struct mmu_page *pg);

int mmu_find_pte(struct mmu_pgtbl *pgtbl, physical_addr_t ia,
		     arch_pte_t **ptep, struct mmu_pgtbl **pgtblp);

struct mmu_get_guest_page_ops {
	void (*setfault)(void *opaque, int stage, int level,
			 physical_addr_t guest_ia);
	int (*gpa2hpa)(void *opaque, int stage, int level,
		       physical_addr_t guest_pa,
		       physical_addr_t *out_host_pa);
};

/**
 * Get guest page table entry
 *
 * Returns VMM_OK on success, VMM_EFAULT on trap and VMM_Exxx on failure.
 */
int mmu_get_guest_page(physical_addr_t pgtbl_guest_ia, int stage, int level,
		       const struct mmu_get_guest_page_ops *ops,
		       void *opaque, physical_addr_t guest_ia,
		       struct mmu_page *pg);

void mmu_walk_address(struct mmu_pgtbl *pgtbl, physical_addr_t ia,
		      void (*fn)(struct mmu_pgtbl *, arch_pte_t *, void *),
		      void *opaque);

void mmu_walk_tables(struct mmu_pgtbl *pgtbl,
		     void (*fn)(struct mmu_pgtbl *pgtbl, void *),
		     void *opaque);

int mmu_find_free_address(struct mmu_pgtbl *pgtbl, physical_addr_t min_addr,
			   int page_order, physical_addr_t *addr);

int mmu_idmap_nested_pgtbl(struct mmu_pgtbl *s2_pgtbl,
			   struct mmu_pgtbl *s1_pgtbl,
			   physical_size_t map_size, u32 reg_flags);

#define MMU_TEST_WIDTH_8BIT		(1UL << 0)
#define MMU_TEST_WIDTH_16BIT		(1UL << 1)
#define MMU_TEST_WIDTH_32BIT		(1UL << 2)
#define MMU_TEST_WRITE			(1UL << 3)
#define MMU_TEST_VALID_MASK		0xfUL

#define MMU_TEST_FAULT_S1		(1UL << 0)
#define MMU_TEST_FAULT_NOMAP		(1UL << 1)
#define MMU_TEST_FAULT_READ		(1UL << 2)
#define MMU_TEST_FAULT_WRITE		(1UL << 3)
#define MMU_TEST_FAULT_UNKNOWN	(1UL << 4)

int mmu_test_nested_pgtbl(struct mmu_pgtbl *s2_pgtbl,
			  struct mmu_pgtbl *s1_pgtbl,
			  u32 flags, virtual_addr_t addr,
			  physical_addr_t expected_output_addr,
			  u32 expected_fault_flags);

int mmu_get_hypervisor_page(virtual_addr_t va, struct mmu_page *pg);

int mmu_unmap_hypervisor_page(struct mmu_page *pg);

int mmu_map_hypervisor_page(struct mmu_page *pg);

struct mmu_pgtbl *mmu_hypervisor_pgtbl(void);

static inline struct mmu_pgtbl *mmu_stage2_current_pgtbl(void)
{
	physical_addr_t tbl_pa = arch_mmu_stage2_current_pgtbl_addr();
	return mmu_pgtbl_find(MMU_STAGE2, tbl_pa);
}

static inline u32 mmu_stage2_current_vmid(void)
{
	return arch_mmu_stage2_current_vmid();
}

static inline int mmu_stage2_change_pgtbl(struct mmu_pgtbl *pgtbl)
{
	return arch_mmu_stage2_change_pgtbl(mmu_pgtbl_has_hw_tag(pgtbl),
					    mmu_pgtbl_hw_tag(pgtbl),
					    pgtbl->tbl_pa);
}

#endif
