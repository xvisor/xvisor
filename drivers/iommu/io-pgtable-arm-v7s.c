/**
 * Copyright (c) 2016 Anup Patel.
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
 * @file io-pgtable-arm-v7s.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief CPU-agnostic ARM page table allocator.
 *
 * The source has been largely adapted from Linux
 * drivers/iommu/io-pgtable-arm-v7s.c
 *
 * CPU-agnostic ARM page table allocator.
 *
 * ARMv7 Short-descriptor format, supporting
 * - Basic memory attributes
 * - Simplified access permissions (AP[2:1] model)
 * - Backwards-compatible TEX remap
 * - Large pages/supersections (if indicated by the caller)
 *
 * Not supporting:
 * - Legacy access permissions (AP[2:0] model)
 *
 * Almost certainly never supporting:
 * - PXN
 * - Domains
 *
 * Copyright (C) 2014-2015 ARM Limited
 * Copyright (c) 2014-2015 MediaTek Inc.
 *
 * The original code is licensed under the GPL.
 */

#include <vmm_error.h>
#include <vmm_macros.h>
#include <vmm_stdio.h>
#include <vmm_heap.h>
#include <vmm_iommu.h>
#include <vmm_limits.h>
#include <vmm_host_aspace.h>
#include <arch_barrier.h>
#include <libs/bitops.h>
#include <libs/mathlib.h>
#include <libs/stringlib.h>
#include <libs/log2.h>

#include "io-pgtable.h"

typedef u32 arm_v7s_iopte;

static bool selftest_running = FALSE;

struct arm_v7s_io_pgtable {
	struct io_pgtable	iop;

	arm_v7s_iopte		*pgd;
};

/* Struct accessors */
#define io_pgtable_to_data(x)						\
	container_of((x), struct arm_v7s_io_pgtable, iop)

#define io_pgtable_ops_to_data(x)					\
	io_pgtable_to_data(io_pgtable_ops_to_pgtable(x))

/*
 * We have 32 bits total; 12 bits resolved at level 1, 8 bits at level 2,
 * and 12 bits in a page. With some carefully-chosen coefficients we can
 * hide the ugly inconsistencies behind these macros and at least let the
 * rest of the code pretend to be somewhat sane.
 */
#define ARM_V7S_ADDR_BITS		32
#define _ARM_V7S_LVL_BITS(lvl)		(16 - (lvl) * 4)
#define ARM_V7S_LVL_SHIFT(lvl)		(ARM_V7S_ADDR_BITS - (4 + 8 * (lvl)))
#define ARM_V7S_TABLE_SHIFT		10

#define ARM_V7S_PTES_PER_LVL(lvl)	(1 << _ARM_V7S_LVL_BITS(lvl))
#define ARM_V7S_TABLE_SIZE(lvl)						\
	(ARM_V7S_PTES_PER_LVL(lvl) * sizeof(arm_v7s_iopte))

#define ARM_V7S_BLOCK_SIZE(lvl)		(1UL << ARM_V7S_LVL_SHIFT(lvl))
#define ARM_V7S_LVL_MASK(lvl)		((u32)(~0U << ARM_V7S_LVL_SHIFT(lvl)))
#define ARM_V7S_TABLE_MASK		((u32)(~0U << ARM_V7S_TABLE_SHIFT))
#define _ARM_V7S_IDX_MASK(lvl)		(ARM_V7S_PTES_PER_LVL(lvl) - 1)
#define ARM_V7S_LVL_IDX(addr, lvl)	({				\
	int _l = lvl;							\
	((u32)(addr) >> ARM_V7S_LVL_SHIFT(_l)) & _ARM_V7S_IDX_MASK(_l); \
})

/*
 * Large page/supersection entries are effectively a block of 16 page/section
 * entries, along the lines of the LPAE contiguous hint, but all with the
 * same output address. For want of a better common name we'll call them
 * "contiguous" versions of their respective page/section entries here, but
 * noting the distinction (WRT to TLB maintenance) that they represent *one*
 * entry repeated 16 times, not 16 separate entries (as in the LPAE case).
 */
#define ARM_V7S_CONT_PAGES		16

/* PTE type bits: these are all mixed up with XN/PXN bits in most cases */
#define ARM_V7S_PTE_TYPE_TABLE		0x1
#define ARM_V7S_PTE_TYPE_PAGE		0x2
#define ARM_V7S_PTE_TYPE_CONT_PAGE	0x1

#define ARM_V7S_PTE_IS_VALID(pte)	(((pte) & 0x3) != 0)
#define ARM_V7S_PTE_IS_TABLE(pte, lvl)	(lvl == 1 && ((pte) & ARM_V7S_PTE_TYPE_TABLE))

/* Page table bits */
#define ARM_V7S_ATTR_XN(lvl)		BIT(4 * (2 - (lvl)))
#define ARM_V7S_ATTR_B			BIT(2)
#define ARM_V7S_ATTR_C			BIT(3)
#define ARM_V7S_ATTR_NS_TABLE		BIT(3)
#define ARM_V7S_ATTR_NS_SECTION		BIT(19)

#define ARM_V7S_CONT_SECTION		BIT(18)
#define ARM_V7S_CONT_PAGE_XN_SHIFT	15

/*
 * The attribute bits are consistently ordered*, but occupy bits [17:10] of
 * a level 1 PTE vs. bits [11:4] at level 2. Thus we define the individual
 * fields relative to that 8-bit block, plus a total shift relative to the PTE.
 */
#define ARM_V7S_ATTR_SHIFT(lvl)		(16 - (lvl) * 6)

#define ARM_V7S_ATTR_MASK		0xff
#define ARM_V7S_ATTR_AP0		BIT(0)
#define ARM_V7S_ATTR_AP1		BIT(1)
#define ARM_V7S_ATTR_AP2		BIT(5)
#define ARM_V7S_ATTR_S			BIT(6)
#define ARM_V7S_ATTR_NG			BIT(7)
#define ARM_V7S_TEX_SHIFT		2
#define ARM_V7S_TEX_MASK		0x7
#define ARM_V7S_ATTR_TEX(val)		(((val) & ARM_V7S_TEX_MASK) << ARM_V7S_TEX_SHIFT)

#define ARM_V7S_ATTR_MTK_4GB		BIT(9) /* MTK extend it for 4GB mode */

/* well, except for TEX on level 2 large pages, of course :( */
#define ARM_V7S_CONT_PAGE_TEX_SHIFT	6
#define ARM_V7S_CONT_PAGE_TEX_MASK	(ARM_V7S_TEX_MASK << ARM_V7S_CONT_PAGE_TEX_SHIFT)

/* Simplified access permissions */
#define ARM_V7S_PTE_AF			ARM_V7S_ATTR_AP0
#define ARM_V7S_PTE_AP_UNPRIV		ARM_V7S_ATTR_AP1
#define ARM_V7S_PTE_AP_RDONLY		ARM_V7S_ATTR_AP2

/* Register bits */
#define ARM_V7S_RGN_NC			0
#define ARM_V7S_RGN_WBWA		1
#define ARM_V7S_RGN_WT			2
#define ARM_V7S_RGN_WB			3

#define ARM_V7S_PRRR_TYPE_DEVICE	1
#define ARM_V7S_PRRR_TYPE_NORMAL	2
#define ARM_V7S_PRRR_TR(n, type)	(((type) & 0x3) << ((n) * 2))
#define ARM_V7S_PRRR_DS0		BIT(16)
#define ARM_V7S_PRRR_DS1		BIT(17)
#define ARM_V7S_PRRR_NS0		BIT(18)
#define ARM_V7S_PRRR_NS1		BIT(19)
#define ARM_V7S_PRRR_NOS(n)		BIT((n) + 24)

#define ARM_V7S_NMRR_IR(n, attr)	(((attr) & 0x3) << ((n) * 2))
#define ARM_V7S_NMRR_OR(n, attr)	(((attr) & 0x3) << ((n) * 2 + 16))

#define ARM_V7S_TTBR_S			BIT(1)
#define ARM_V7S_TTBR_NOS		BIT(5)
#define ARM_V7S_TTBR_ORGN_ATTR(attr)	(((attr) & 0x3) << 3)
#define ARM_V7S_TTBR_IRGN_ATTR(attr)					\
	((((attr) & 0x1) << 6) | (((attr) & 0x2) >> 1))

#define ARM_V7S_TCR_PD1			BIT(5)

static arm_v7s_iopte *iopte_deref(arm_v7s_iopte pte, int lvl)
{
	int rc;
	virtual_addr_t va = 0x0;

	if (ARM_V7S_PTE_IS_TABLE(pte, lvl))
		pte &= ARM_V7S_TABLE_MASK;
	else
		pte &= ARM_V7S_LVL_MASK(lvl);

	rc = vmm_host_pa2va((physical_addr_t)pte, &va);
	if (rc)
		return NULL;

	return (arm_v7s_iopte *)va;
}

static void *__arm_v7s_alloc_table(int lvl, struct arm_v7s_io_pgtable *data)
{
	size_t p, size = ARM_V7S_TABLE_SIZE(lvl);
	virtual_addr_t table = 0x0;

	if (lvl == 1)
		table = vmm_host_alloc_aligned_pages(VMM_SIZE_TO_PAGE(size),
						ilog2(size),
						VMM_MEMORY_FLAGS_NORMAL_NOCACHE);
	else if (lvl == 2)
		table = vmm_host_alloc_pages(VMM_SIZE_TO_PAGE(size),
					VMM_MEMORY_FLAGS_NORMAL_NOCACHE);
	else
		return NULL;

	for (p = 0; p < (VMM_SIZE_TO_PAGE(size) * VMM_PAGE_SIZE); p += 4) {
		*(u32 *)((void *)table + p) = 0x0;
	}

	return (void *)table;
}

static void __arm_v7s_free_table(void *table, int lvl,
				 struct arm_v7s_io_pgtable *data)
{
	size_t size = ARM_V7S_TABLE_SIZE(lvl);

	vmm_host_free_pages((virtual_addr_t)table, VMM_SIZE_TO_PAGE(size));
}

static void __arm_v7s_pte_sync(arm_v7s_iopte *ptep, int num_entries,
			       struct io_pgtable_cfg *cfg)
{
	arch_smp_wmb();
}

static void __arm_v7s_set_pte(arm_v7s_iopte *ptep, arm_v7s_iopte pte,
			      int num_entries, struct io_pgtable_cfg *cfg)
{
	int i;

	for (i = 0; i < num_entries; i++)
		ptep[i] = pte;

	__arm_v7s_pte_sync(ptep, num_entries, cfg);
}

static arm_v7s_iopte arm_v7s_prot_to_pte(int prot, int lvl,
					 struct io_pgtable_cfg *cfg)
{
	bool ap = !(cfg->quirks & IO_PGTABLE_QUIRK_NO_PERMS);
	arm_v7s_iopte pte = ARM_V7S_ATTR_NG | ARM_V7S_ATTR_S;

	if (!(prot & VMM_IOMMU_MMIO))
		pte |= ARM_V7S_ATTR_TEX(1);
	if (ap) {
		pte |= ARM_V7S_PTE_AF | ARM_V7S_PTE_AP_UNPRIV;
		if (!(prot & VMM_IOMMU_WRITE))
			pte |= ARM_V7S_PTE_AP_RDONLY;
	}
	pte <<= ARM_V7S_ATTR_SHIFT(lvl);

	if ((prot & VMM_IOMMU_NOEXEC) && ap)
		pte |= ARM_V7S_ATTR_XN(lvl);
	if (prot & VMM_IOMMU_MMIO)
		pte |= ARM_V7S_ATTR_B;
	else if (prot & VMM_IOMMU_CACHE)
		pte |= ARM_V7S_ATTR_B | ARM_V7S_ATTR_C;

	return pte;
}

static int arm_v7s_pte_to_prot(arm_v7s_iopte pte, int lvl)
{
	int prot = VMM_IOMMU_READ;
	arm_v7s_iopte attr = pte >> ARM_V7S_ATTR_SHIFT(lvl);

	if (!(attr & ARM_V7S_PTE_AP_RDONLY))
		prot |= VMM_IOMMU_WRITE;
	if ((attr & (ARM_V7S_TEX_MASK << ARM_V7S_TEX_SHIFT)) == 0)
		prot |= VMM_IOMMU_MMIO;
	else if (pte & ARM_V7S_ATTR_C)
		prot |= VMM_IOMMU_CACHE;
	if (pte & ARM_V7S_ATTR_XN(lvl))
		prot |= VMM_IOMMU_NOEXEC;

	return prot;
}

static arm_v7s_iopte arm_v7s_pte_to_cont(arm_v7s_iopte pte, int lvl)
{
	if (lvl == 1) {
		pte |= ARM_V7S_CONT_SECTION;
	} else if (lvl == 2) {
		arm_v7s_iopte xn = pte & ARM_V7S_ATTR_XN(lvl);
		arm_v7s_iopte tex = pte & ARM_V7S_CONT_PAGE_TEX_MASK;

		pte ^= xn | tex | ARM_V7S_PTE_TYPE_PAGE;
		pte |= (xn << ARM_V7S_CONT_PAGE_XN_SHIFT) |
		       (tex << ARM_V7S_CONT_PAGE_TEX_SHIFT) |
		       ARM_V7S_PTE_TYPE_CONT_PAGE;
	}
	return pte;
}

static arm_v7s_iopte arm_v7s_cont_to_pte(arm_v7s_iopte pte, int lvl)
{
	if (lvl == 1) {
		pte &= ~ARM_V7S_CONT_SECTION;
	} else if (lvl == 2) {
		arm_v7s_iopte xn = pte & BIT(ARM_V7S_CONT_PAGE_XN_SHIFT);
		arm_v7s_iopte tex = pte & (ARM_V7S_CONT_PAGE_TEX_MASK <<
					   ARM_V7S_CONT_PAGE_TEX_SHIFT);

		pte ^= xn | tex | ARM_V7S_PTE_TYPE_CONT_PAGE;
		pte |= (xn >> ARM_V7S_CONT_PAGE_XN_SHIFT) |
		       (tex >> ARM_V7S_CONT_PAGE_TEX_SHIFT) |
		       ARM_V7S_PTE_TYPE_PAGE;
	}
	return pte;
}

static bool arm_v7s_pte_is_cont(arm_v7s_iopte pte, int lvl)
{
	if (lvl == 1 && !ARM_V7S_PTE_IS_TABLE(pte, lvl))
		return pte & ARM_V7S_CONT_SECTION;
	else if (lvl == 2)
		return !(pte & ARM_V7S_PTE_TYPE_PAGE);
	return FALSE;
}

static int __arm_v7s_unmap(struct arm_v7s_io_pgtable *, physical_addr_t,
			   size_t, int, arm_v7s_iopte *);

static int arm_v7s_init_pte(struct arm_v7s_io_pgtable *data,
			    physical_addr_t iova, physical_addr_t paddr,
			    int prot, int lvl, int num_entries,
			    arm_v7s_iopte *ptep)
{
	struct io_pgtable_cfg *cfg = &data->iop.cfg;
	arm_v7s_iopte pte = arm_v7s_prot_to_pte(prot, lvl, cfg);
	int i;

	for (i = 0; i < num_entries; i++)
		if (ARM_V7S_PTE_IS_TABLE(ptep[i], lvl)) {
			/*
			 * We need to unmap and free the old table before
			 * overwriting it with a block entry.
			 */
			arm_v7s_iopte *tblp;
			size_t sz = ARM_V7S_BLOCK_SIZE(lvl);

			tblp = ptep - ARM_V7S_LVL_IDX(iova, lvl);
			if (WARN_ON(__arm_v7s_unmap(data, iova + i * sz,
						    sz, lvl, tblp) != sz))
				return VMM_EINVALID;
		} else if (ptep[i]) {
			/* We require an unmap first */
			WARN_ON(!selftest_running);
			return VMM_EEXIST;
		}

	pte |= ARM_V7S_PTE_TYPE_PAGE;
	if (lvl == 1 && (cfg->quirks & IO_PGTABLE_QUIRK_ARM_NS))
		pte |= ARM_V7S_ATTR_NS_SECTION;

	if (cfg->quirks & IO_PGTABLE_QUIRK_ARM_MTK_4GB)
		pte |= ARM_V7S_ATTR_MTK_4GB;

	if (num_entries > 1)
		pte = arm_v7s_pte_to_cont(pte, lvl);

	pte |= paddr & ARM_V7S_LVL_MASK(lvl);

	__arm_v7s_set_pte(ptep, pte, num_entries, cfg);
	return 0;
}

static int __arm_v7s_map(struct arm_v7s_io_pgtable *data,
			 physical_addr_t iova, physical_addr_t paddr,
			 size_t size, int prot, int lvl, arm_v7s_iopte *ptep)
{
	int rc;
	physical_addr_t pa;
	struct io_pgtable_cfg *cfg = &data->iop.cfg;
	arm_v7s_iopte pte, *cptep;
	int num_entries = size >> ARM_V7S_LVL_SHIFT(lvl);

	/* Find our entry at the current level */
	ptep += ARM_V7S_LVL_IDX(iova, lvl);

	/* If we can install a leaf entry at this level, then do so */
	if (num_entries)
		return arm_v7s_init_pte(data, iova, paddr, prot,
					lvl, num_entries, ptep);

	/* We can't allocate tables at the final level */
	if (WARN_ON(lvl == 2))
		return VMM_EINVALID;

	/* Grab a pointer to the next level */
	pte = *ptep;
	if (!pte) {
		cptep = __arm_v7s_alloc_table(lvl + 1, data);
		if (!cptep)
			return VMM_ENOMEM;

		rc = vmm_host_va2pa((virtual_addr_t)cptep, &pa);
		BUG_ON(rc);

		pte = ((arm_v7s_iopte)pa) | ARM_V7S_PTE_TYPE_TABLE;
		if (cfg->quirks & IO_PGTABLE_QUIRK_ARM_NS)
			pte |= ARM_V7S_ATTR_NS_TABLE;

		__arm_v7s_set_pte(ptep, pte, 1, cfg);
	} else {
		cptep = iopte_deref(pte, lvl);
	}

	/* Rinse, repeat */
	return __arm_v7s_map(data, iova, paddr, size, prot, lvl + 1, cptep);
}

static int arm_v7s_map(struct io_pgtable_ops *ops, physical_addr_t iova,
			physical_addr_t paddr, size_t size, int prot)
{
	struct arm_v7s_io_pgtable *data = io_pgtable_ops_to_data(ops);
	struct io_pgtable *iop = &data->iop;
	int ret;

	/* If no access, then nothing to do */
	if (!(prot & (VMM_IOMMU_READ | VMM_IOMMU_WRITE)))
		return 0;

	ret = __arm_v7s_map(data, iova, paddr, size, prot, 1, data->pgd);
	/*
	 * Synchronise all PTE updates for the new mapping before there's
	 * a chance for anything to kick off a table walk for the new iova.
	 */
	if (iop->cfg.quirks & IO_PGTABLE_QUIRK_TLBI_ON_MAP) {
		io_pgtable_tlb_add_flush(iop, iova, size,
					 ARM_V7S_BLOCK_SIZE(2), FALSE);
		io_pgtable_tlb_sync(iop);
	} else {
		arch_smp_wmb();
	}

	return ret;
}

static void arm_v7s_free_pgtable(struct io_pgtable *iop)
{
	struct arm_v7s_io_pgtable *data = io_pgtable_to_data(iop);
	int i;

	for (i = 0; i < ARM_V7S_PTES_PER_LVL(1); i++) {
		arm_v7s_iopte pte = data->pgd[i];

		if (ARM_V7S_PTE_IS_TABLE(pte, 1))
			__arm_v7s_free_table(iopte_deref(pte, 1), 2, data);
	}
	__arm_v7s_free_table(data->pgd, 1, data);
	vmm_free(data);
}

static void arm_v7s_split_cont(struct arm_v7s_io_pgtable *data,
			       physical_addr_t iova, int idx, int lvl,
			       arm_v7s_iopte *ptep)
{
	struct io_pgtable *iop = &data->iop;
	arm_v7s_iopte pte;
	size_t size = ARM_V7S_BLOCK_SIZE(lvl);
	int i;

	ptep -= idx & (ARM_V7S_CONT_PAGES - 1);
	pte = arm_v7s_cont_to_pte(*ptep, lvl);
	for (i = 0; i < ARM_V7S_CONT_PAGES; i++) {
		ptep[i] = pte;
		pte += size;
	}

	__arm_v7s_pte_sync(ptep, ARM_V7S_CONT_PAGES, &iop->cfg);

	size *= ARM_V7S_CONT_PAGES;
	io_pgtable_tlb_add_flush(iop, iova, size, size, TRUE);
	io_pgtable_tlb_sync(iop);
}

static int arm_v7s_split_blk_unmap(struct arm_v7s_io_pgtable *data,
				   physical_addr_t iova, size_t size,
				   arm_v7s_iopte *ptep)
{
	physical_addr_t blk_start, blk_end, blk_size;
	physical_addr_t blk_paddr;
	arm_v7s_iopte table = 0;
	int prot = arm_v7s_pte_to_prot(*ptep, 1);

	blk_size = ARM_V7S_BLOCK_SIZE(1);
	blk_start = iova & ARM_V7S_LVL_MASK(1);
	blk_end = blk_start + ARM_V7S_BLOCK_SIZE(1);
	blk_paddr = *ptep & ARM_V7S_LVL_MASK(1);

	for (; blk_start < blk_end; blk_start += size, blk_paddr += size) {
		arm_v7s_iopte *tablep;

		/* Unmap! */
		if (blk_start == iova)
			continue;

		/* __arm_v7s_map expects a pointer to the start of the table */
		tablep = &table - ARM_V7S_LVL_IDX(blk_start, 1);
		if (__arm_v7s_map(data, blk_start, blk_paddr, size, prot, 1,
				  tablep) < 0) {
			if (table) {
				/* Free the table we allocated */
				tablep = iopte_deref(table, 1);
				__arm_v7s_free_table(tablep, 2, data);
			}
			return 0; /* Bytes unmapped */
		}
	}

	__arm_v7s_set_pte(ptep, table, 1, &data->iop.cfg);
	iova &= ~(blk_size - 1);
	io_pgtable_tlb_add_flush(&data->iop, iova, blk_size, blk_size, TRUE);
	return size;
}

static int __arm_v7s_unmap(struct arm_v7s_io_pgtable *data,
			    physical_addr_t iova, size_t size, int lvl,
			    arm_v7s_iopte *ptep)
{
	arm_v7s_iopte pte[ARM_V7S_CONT_PAGES];
	struct io_pgtable *iop = &data->iop;
	int idx, i = 0, num_entries = size >> ARM_V7S_LVL_SHIFT(lvl);

	/* Something went horribly wrong and we ran out of page table */
	if (WARN_ON(lvl > 2))
		return 0;

	idx = ARM_V7S_LVL_IDX(iova, lvl);
	ptep += idx;
	do {
		if (WARN_ON(!ARM_V7S_PTE_IS_VALID(ptep[i])))
			return 0;
		pte[i] = ptep[i];
	} while (++i < num_entries);

	/*
	 * If we've hit a contiguous 'large page' entry at this level, it
	 * needs splitting first, unless we're unmapping the whole lot.
	 */
	if (num_entries <= 1 && arm_v7s_pte_is_cont(pte[0], lvl))
		arm_v7s_split_cont(data, iova, idx, lvl, ptep);

	/* If the size matches this level, we're in the right place */
	if (num_entries) {
		size_t blk_size = ARM_V7S_BLOCK_SIZE(lvl);

		__arm_v7s_set_pte(ptep, 0, num_entries, &iop->cfg);

		for (i = 0; i < num_entries; i++) {
			if (ARM_V7S_PTE_IS_TABLE(pte[i], lvl)) {
				/* Also flush any partial walks */
				io_pgtable_tlb_add_flush(iop, iova, blk_size,
					ARM_V7S_BLOCK_SIZE(lvl + 1), FALSE);
				io_pgtable_tlb_sync(iop);
				ptep = iopte_deref(pte[i], lvl);
				__arm_v7s_free_table(ptep, lvl + 1, data);
			} else {
				io_pgtable_tlb_add_flush(iop, iova, blk_size,
							 blk_size, TRUE);
			}
			iova += blk_size;
		}
		return size;
	} else if (lvl == 1 && !ARM_V7S_PTE_IS_TABLE(pte[0], lvl)) {
		/*
		 * Insert a table at the next level to map the old region,
		 * minus the part we want to unmap
		 */
		return arm_v7s_split_blk_unmap(data, iova, size, ptep);
	}

	/* Keep on walkin' */
	ptep = iopte_deref(pte[0], lvl);
	return __arm_v7s_unmap(data, iova, size, lvl + 1, ptep);
}

static int arm_v7s_unmap(struct io_pgtable_ops *ops, physical_addr_t iova,
			 size_t size)
{
	struct arm_v7s_io_pgtable *data = io_pgtable_ops_to_data(ops);
	size_t unmapped;

	unmapped = __arm_v7s_unmap(data, iova, size, 1, data->pgd);
	if (unmapped)
		io_pgtable_tlb_sync(&data->iop);

	return unmapped;
}

static physical_addr_t arm_v7s_iova_to_phys(struct io_pgtable_ops *ops,
					    physical_addr_t iova)
{
	struct arm_v7s_io_pgtable *data = io_pgtable_ops_to_data(ops);
	arm_v7s_iopte *ptep = data->pgd, pte;
	int lvl = 0;
	u32 mask;

	do {
		pte = ptep[ARM_V7S_LVL_IDX(iova, ++lvl)];
		ptep = iopte_deref(pte, lvl);
	} while (ARM_V7S_PTE_IS_TABLE(pte, lvl));

	if (!ARM_V7S_PTE_IS_VALID(pte))
		return 0;

	mask = ARM_V7S_LVL_MASK(lvl);
	if (arm_v7s_pte_is_cont(pte, lvl))
		mask *= ARM_V7S_CONT_PAGES;
	return (pte & mask) | (iova & ~mask);
}

static struct io_pgtable *arm_v7s_alloc_pgtable(struct io_pgtable_cfg *cfg,
						void *cookie)
{
	int rc;
	physical_addr_t pa;
	struct arm_v7s_io_pgtable *data;

	if (cfg->ias > ARM_V7S_ADDR_BITS || cfg->oas > ARM_V7S_ADDR_BITS)
		return NULL;

	if (cfg->quirks & ~(IO_PGTABLE_QUIRK_ARM_NS |
			    IO_PGTABLE_QUIRK_NO_PERMS |
			    IO_PGTABLE_QUIRK_TLBI_ON_MAP |
			    IO_PGTABLE_QUIRK_ARM_MTK_4GB))
		return NULL;

	/* If ARM_MTK_4GB is enabled, the NO_PERMS is also expected. */
	if (cfg->quirks & IO_PGTABLE_QUIRK_ARM_MTK_4GB &&
	    !(cfg->quirks & IO_PGTABLE_QUIRK_NO_PERMS))
			return NULL;

	data = vmm_zalloc(sizeof(*data));
	if (!data)
		return NULL;

	data->iop.ops = (struct io_pgtable_ops) {
		.map		= arm_v7s_map,
		.unmap		= arm_v7s_unmap,
		.iova_to_phys	= arm_v7s_iova_to_phys,
	};

	/* We have to do this early for __arm_v7s_alloc_table to work... */
	data->iop.cfg = *cfg;

	/*
	 * Unless the IOMMU driver indicates supersection support by
	 * having SZ_16M set in the initial bitmap, they won't be used.
	 */
	cfg->pgsize_bitmap &= SZ_4K | SZ_64K | SZ_1M | SZ_16M;

	/* TCR: T0SZ=0, disable TTBR1 */
	cfg->arm_v7s_cfg.tcr = ARM_V7S_TCR_PD1;

	/*
	 * TEX remap: the indices used map to the closest equivalent types
	 * under the non-TEX-remap interpretation of those attribute bits,
	 * excepting various implementation-defined aspects of shareability.
	 */
	cfg->arm_v7s_cfg.prrr = ARM_V7S_PRRR_TR(1, ARM_V7S_PRRR_TYPE_DEVICE) |
				ARM_V7S_PRRR_TR(4, ARM_V7S_PRRR_TYPE_NORMAL) |
				ARM_V7S_PRRR_TR(7, ARM_V7S_PRRR_TYPE_NORMAL) |
				ARM_V7S_PRRR_DS0 | ARM_V7S_PRRR_DS1 |
				ARM_V7S_PRRR_NS1 | ARM_V7S_PRRR_NOS(7);
	cfg->arm_v7s_cfg.nmrr = ARM_V7S_NMRR_IR(7, ARM_V7S_RGN_WBWA) |
				ARM_V7S_NMRR_OR(7, ARM_V7S_RGN_WBWA);

	/* Looking good; allocate a pgd */
	data->pgd = __arm_v7s_alloc_table(1, data);
	if (!data->pgd)
		goto out_free_data;

	/* Ensure the empty pgd is visible before any actual TTBR write */
	arch_smp_wmb();

	rc = vmm_host_va2pa((virtual_addr_t)data->pgd, &pa);
	BUG_ON(rc);

	/* TTBRs */
	cfg->arm_v7s_cfg.ttbr[0] = ((arm_v7s_iopte)pa) |
				   ARM_V7S_TTBR_S | ARM_V7S_TTBR_NOS |
				   ARM_V7S_TTBR_IRGN_ATTR(ARM_V7S_RGN_NC) |
				   ARM_V7S_TTBR_ORGN_ATTR(ARM_V7S_RGN_NC);
	cfg->arm_v7s_cfg.ttbr[1] = 0;
	return &data->iop;

out_free_data:
	vmm_free(data);
	return NULL;
}

struct io_pgtable_init_fns io_pgtable_arm_v7s_init_fns = {
	.alloc	= arm_v7s_alloc_pgtable,
	.free	= arm_v7s_free_pgtable,
};

#ifdef CONFIG_IOMMU_IO_PGTABLE_ARMV7S_SELFTEST

#include <vmm_modules.h>
#include <libs/bitmap.h>

#define MODULE_DESC			"ARMv7s IOPGTABLE Selftest"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(1)
#define	MODULE_INIT			arm_v7s_selftest_init
#define	MODULE_EXIT			arm_v7s_selftest_exit

static struct io_pgtable_cfg *cfg_cookie;

static void dummy_tlb_flush_all(void *cookie)
{
	WARN_ON(cookie != cfg_cookie);
}

static void dummy_tlb_add_flush(physical_addr_t iova, size_t size,
				size_t granule, bool leaf, void *cookie)
{
	WARN_ON(cookie != cfg_cookie);
	WARN_ON(!(size & cfg_cookie->pgsize_bitmap));
}

static void dummy_tlb_sync(void *cookie)
{
	WARN_ON(cookie != cfg_cookie);
}

static struct iommu_gather_ops dummy_tlb_ops = {
	.tlb_flush_all	= dummy_tlb_flush_all,
	.tlb_add_flush	= dummy_tlb_add_flush,
	.tlb_sync	= dummy_tlb_sync,
};

#define __FAIL(ops)	({					\
	vmm_lwarning("selftest", "arm-v7s failed\n");	\
	selftest_running = FALSE;				\
	VMM_EFAIL;						\
})

static int __init arm_v7s_do_selftests(void)
{
	struct io_pgtable_ops *ops;
	struct io_pgtable_cfg cfg = {
		.tlb = &dummy_tlb_ops,
		.oas = 32,
		.ias = 32,
		.quirks = IO_PGTABLE_QUIRK_ARM_NS,
		.pgsize_bitmap = SZ_4K | SZ_64K | SZ_1M | SZ_16M,
	};
	unsigned int iova, size, iova_start;
	unsigned int i, loopnr = 0;

	selftest_running = TRUE;

	cfg_cookie = &cfg;

	ops = alloc_io_pgtable_ops(ARM_V7S, &cfg, &cfg);
	if (!ops) {
		vmm_lerror("selftest",
			   "arm-v7s failed to allocate io pgtable ops\n");
		return VMM_EINVALID;
	}

	/*
	 * Initial sanity checks.
	 * Empty page tables shouldn't provide any translations.
	 */
	if (ops->iova_to_phys(ops, 42))
		return __FAIL(ops);

	if (ops->iova_to_phys(ops, SZ_1G + 42))
		return __FAIL(ops);

	if (ops->iova_to_phys(ops, SZ_2G + 42))
		return __FAIL(ops);

	/*
	 * Distinct mappings of different granule sizes.
	 */
	iova = 0;
	i = find_first_bit(&cfg.pgsize_bitmap, BITS_PER_LONG);
	while (i != BITS_PER_LONG) {
		size = 1UL << i;
		if (ops->map(ops, iova, iova, size, VMM_IOMMU_READ |
						    VMM_IOMMU_WRITE |
						    VMM_IOMMU_NOEXEC |
						    VMM_IOMMU_CACHE))
			return __FAIL(ops);

		/* Overlapping mappings */
		if (!ops->map(ops, iova, iova + size, size,
			      VMM_IOMMU_READ | VMM_IOMMU_NOEXEC))
			return __FAIL(ops);

		if (ops->iova_to_phys(ops, iova + 42) != (iova + 42))
			return __FAIL(ops);

		iova += SZ_16M;
		i++;
		i = find_next_bit(&cfg.pgsize_bitmap, BITS_PER_LONG, i);
		loopnr++;
	}

	/* Partial unmap */
	i = 1;
	size = 1UL << __ffs(cfg.pgsize_bitmap);
	while (i < loopnr) {
		iova_start = i * SZ_16M;
		if (ops->unmap(ops, iova_start + size, size) != size)
			return __FAIL(ops);

		/* Remap of partial unmap */
		if (ops->map(ops, iova_start + size, size, size, VMM_IOMMU_READ))
			return __FAIL(ops);

		if (ops->iova_to_phys(ops, iova_start + size + 42)
		    != (size + 42))
			return __FAIL(ops);
		i++;
	}

	/* Full unmap */
	iova = 0;
	i = find_first_bit(&cfg.pgsize_bitmap, BITS_PER_LONG);
	while (i != BITS_PER_LONG) {
		size = 1UL << i;

		if (ops->unmap(ops, iova, size) != size)
			return __FAIL(ops);

		if (ops->iova_to_phys(ops, iova + 42))
			return __FAIL(ops);

		/* Remap full block */
		if (ops->map(ops, iova, iova, size, VMM_IOMMU_WRITE))
			return __FAIL(ops);

		if (ops->iova_to_phys(ops, iova + 42) != (iova + 42))
			return __FAIL(ops);

		iova += SZ_16M;
		i++;
		i = find_next_bit(&cfg.pgsize_bitmap, BITS_PER_LONG, i);
	}

	free_io_pgtable_ops(ops);

	selftest_running = FALSE;

	vmm_linfo("selftest", "arm-v7s ok\n");
	return 0;
}

static int __init arm_v7s_selftest_init(void)
{
	arm_v7s_do_selftests();
	return VMM_OK;
}

static void __exit arm_v7s_selftest_exit(void)
{
	/* Nothing to do here. */
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
#endif
