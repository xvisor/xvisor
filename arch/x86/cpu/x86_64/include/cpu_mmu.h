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
 * @brief: MMU related definition and structures.
 */

#ifndef __CPU_MMU_H_
#define __CPU_MMU_H_

#define VMM_CODE_SEG_SEL	0x08
#define VMM_DATA_SEG_SEL	0x10
#define VMM_TSS_SEG_SEL		0x18

/*
 * Bit width and mask for 4 tree levels used in
 * virtual address mapping. 4 tree levels, 9 bit
 * each cover 36-bit of virtual address and reset
 * of the lower 12-bits out of total 48 bits are
 * used as is for page offset.
 *
 *   63-48   47-39  38-30  29-21  20-12   11-0
 * +---------------------------------------------+
 * | UNSED | PML4 | PGDP | PGDI | PGTI | PG OFSET|
 * +---------------------------------------------+
 */
#define PGTREE_BIT_WIDTH	9
#if !defined(__ASSEMBLY__)
#define PGTREE_MASK		~((0x01UL << PGTREE_BIT_WIDTH) - 1)
#else
#define PGTREE_MASK		~((0x01 << PGTREE_BIT_WIDTH) - 1)
#endif

#define PML4_SHIFT		39
#define PML4_MASK		(PGTREE_MASK << PML4_SHIFT)
#define PML4_MAP_MASK		(~((1ULL << PML4_SHIFT) - 1))

#define PGDP_SHIFT		30
#define PGDP_MASK		(PGTREE_MASK << PGDP_SHIFT)
#define PGDP_MAP_MASK		(~((1ULL << PGDP_SHIFT) - 1))

#define PGDI_SHIFT		21
#define PGDI_MASK		(PGTREE_MASK << PGDI_SHIFT)
#define PGDI_MAP_MASK		(~((1ULL << PGDI_SHIFT) - 1))

#define PGTI_SHIFT		12
#define PGTI_MASK		(PGTREE_MASK << PGTI_SHIFT)
#define PGTI_MAP_MASK		(~((1ULL << PGTI_SHIFT) - 1))

#define PAGE_SHIFT		12
#define PAGE_SIZE		(0x01 << PAGE_SHIFT)
#define PAGE_MASK		~(PAGE_SIZE - 1)

#if !defined (__ASSEMBLY__)

#include <vmm_types.h>
#include <vmm_spinlocks.h>
#include <libs/list.h>

extern struct pgtbl_ctrl host_pgtbl_ctl;

static inline void invalidate_vaddr_tlb(virtual_addr_t vaddr)
{
	__asm__ __volatile__("invlpg (%0)\n\t"
			     ::"r"(vaddr):"memory");
}

union page {
	u64 _val;
	struct {
		u64 present:1;
		u64 rw:1;
		u64 priviledge:1;
		u64 write_through:1;
		u64 cache_disable:1;
		u64 accessed:1;
		u64 dirty:1;
		u64 pat:1;
		u64 global:1;
		u64 ignored:3;
		u64 paddr:40;
		u64 reserved:11;
		u64 execution_disable:1;
	} bits;
};

struct page_table {
	struct dlist head;
	struct page_table *parent;
	int level;
	int stage;
	physical_addr_t map_ia;
	physical_addr_t tbl_pa;
	vmm_spinlock_t tbl_lock; /*< Lock to protect table contents, 
				      entry_cnt, child_cnt, and child_list 
				  */
	virtual_addr_t tbl_va;
	u32 pte_cnt;
	u32 child_cnt;
	struct dlist child_list;
};

union seg_attrs {
	u16 bytes;
	struct {
		u16 type:4;
		u16 s:1;
		u16 dpl:2;
		u16 p:1;
		u16 avl:1;
		u16 l:1;
		u16 db:1;
		u16 g:1;
	} fields;
} __packed;

struct seg_selector {
	u16		sel;
	union seg_attrs	attrs;
	u32		limit;
	u64		base;
} __packed;

#endif

#endif /* __CPU_MMU_H_ */
