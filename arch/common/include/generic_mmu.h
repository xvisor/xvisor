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
	MMU_STAGE1=1,
	MMU_STAGE2=2,
};

/** MMU page table */
struct mmu_pgtbl {
	struct dlist head;
	struct mmu_pgtbl *parent;
	enum mmu_stage stage;
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

struct mmu_pgtbl *mmu_pgtbl_find(physical_addr_t tbl_pa);

struct mmu_pgtbl *mmu_pgtbl_alloc(int stage);

int mmu_pgtbl_free(struct mmu_pgtbl *pgtbl);

struct mmu_pgtbl *mmu_pgtbl_get_child(struct mmu_pgtbl *parent,
					  physical_addr_t map_ia,
					  bool create);

int mmu_get_page(struct mmu_pgtbl *pgtbl,
		     physical_addr_t ia, struct mmu_page *pg);

int mmu_unmap_page(struct mmu_pgtbl *pgtbl, struct mmu_page *pg);

int mmu_map_page(struct mmu_pgtbl *pgtbl, struct mmu_page *pg);

int mmu_find_pte(struct mmu_pgtbl *pgtbl, physical_addr_t ia,
		     arch_pte_t **ptep, struct mmu_pgtbl **pgtblp);

int mmu_get_hypervisor_page(virtual_addr_t va, struct mmu_page *pg);

int mmu_unmap_hypervisor_page(struct mmu_page *pg);

int mmu_map_hypervisor_page(struct mmu_page *pg);

struct mmu_pgtbl *mmu_hypervisor_pgtbl(void);

static inline struct mmu_pgtbl *mmu_stage2_current_pgtbl(void)
{
	physical_addr_t tbl_pa = arch_mmu_stage2_current_pgtbl_addr();
	return mmu_pgtbl_find(tbl_pa);
}

static inline u32 mmu_stage2_current_vmid(void)
{
	return arch_mmu_stage2_current_vmid();
}

static inline int mmu_stage2_change_pgtbl(u32 vmid, struct mmu_pgtbl *pgtbl)
{
	return arch_mmu_stage2_change_pgtbl(vmid, pgtbl->tbl_pa);
}

#endif
