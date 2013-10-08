/*
 * Copyright (c) 2010-2020 Himanshu Chauhan.
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
 * @author: Himanshu Chauhan <hschauhan@nulltrace.org>
 * @brief: Generic pagetable handling defines and externs.
 */

#ifndef __CPU_PGTBL_HELPER_H_
#define __CPU_PGTBL_HELPER_H_

struct page_table;

/* Note: we use 1/8th or 12.5% of VAPOOL memory as page table pool. 
 * For example if VAPOOL is 8 MB then page table pool will be 1 MB
 * or 1 MB / 4 KB = 256 page tables
 */
#define PGTBL_FIRST_LEVEL			0
#define PGTBL_LAST_LEVEL			3
#define PGTBL_TABLE_SIZE_SHIFT			12
#define PGTBL_TABLE_SIZE			4096
#define PGTBL_TABLE_ENTCNT			512

struct pgtbl_ctrl {
	struct page_table *base_pgtbl;
	virtual_addr_t pgtbl_base_va;
	physical_addr_t pgtbl_base_pa;
	struct page_table *pgtbl_array;
	struct page_table pgtbl_pml4;
	struct page_table pgtbl_pgdp;
	struct page_table pgtbl_pgdi;
	struct page_table pgtbl_pgti;
	vmm_spinlock_t alloc_lock;
	u32 pgtbl_alloc_count;
	u64 pgtbl_max_size;
	u32 pgtbl_size_shift;
	u32 pgtbl_max_count;
	struct dlist free_pgtbl_list;
};

static inline physical_addr_t mmu_level_map_mask(int level)
{
	switch (level) {
	case 0:
		return PML4_MAP_MASK;
	case 1:
		return PGDP_MAP_MASK;
	case 2:
		return PGDI_MAP_MASK;
	default:
		break;
	};
	return PGTI_MAP_MASK;
}

static inline int mmu_level_index(physical_addr_t ia, int level)
{
	switch (level) {
	case 0:
		return (ia >> PML4_SHIFT) & ~PGTREE_MASK;
	case 1:
		return (ia >> PGDP_SHIFT) & ~PGTREE_MASK;
	case 2:
		return (ia >> PGDI_SHIFT) & ~PGTREE_MASK;
	default:
		break;
	};
	return (ia >> PGTI_SHIFT) & ~PGTREE_MASK;
}

extern int mmu_get_page(struct pgtbl_ctrl *ctrl, struct page_table *pgtbl,
				physical_addr_t ia, union page *pg);
extern int mmu_unmap_page(struct pgtbl_ctrl *ctrl, struct page_table *pgtbl, physical_addr_t ia);
extern int mmu_map_page(struct pgtbl_ctrl *ctrl, struct page_table *pgtbl, physical_addr_t ia, union page *pg);

#endif /* __CPU_PGTBL_HELPER_H_ */
