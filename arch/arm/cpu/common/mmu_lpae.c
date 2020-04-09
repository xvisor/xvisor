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
 * @file mmu_lpae.c
 * @author Anup Patel (anup@brainfault.org)
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief Implementation of MMU for LPAE enabled ARM processor
 */

#include <vmm_error.h>
#include <vmm_types.h>
#include <vmm_smp.h>
#include <vmm_host_aspace.h>
#include <libs/stringlib.h>
#include <generic_mmu.h>

void arch_mmu_pgtbl_clear(virtual_addr_t tbl_va)
{
	memset((void *)tbl_va, 0, ARCH_MMU_PGTBL_SIZE);
}

void arch_mmu_stage2_tlbflush(physical_addr_t gpa, physical_size_t gsz)
{
	cpu_invalid_ipa_guest_tlb(gpa);
}

void arch_mmu_stage1_tlbflush(virtual_addr_t va, virtual_size_t sz)
{
	cpu_invalid_va_hypervisor_tlb(va);
}

bool arch_mmu_valid_block_size(physical_size_t sz)
{
	if ((sz == TTBL_L0_BLOCK_SIZE) ||
	    (sz == TTBL_L1_BLOCK_SIZE) ||
	    (sz == TTBL_L2_BLOCK_SIZE) ||
	    (sz == TTBL_L3_BLOCK_SIZE)) {
		return TRUE;
	}
	return FALSE;
}

int arch_mmu_start_level(int stage)
{
	return 2;
}

physical_size_t arch_mmu_level_block_size(int stage, int level)
{
	switch (level) {
	case 0:
		return TTBL_L3_BLOCK_SIZE;
	case 1:
		return TTBL_L2_BLOCK_SIZE;
	case 2:
		return TTBL_L1_BLOCK_SIZE;
	case 3:
		return TTBL_L0_BLOCK_SIZE;
	default:
		break;
	};
	return TTBL_L3_BLOCK_SIZE;
}

int arch_mmu_level_block_shift(int stage, int level)
{
	switch (level) {
	case 0:
		return TTBL_L3_BLOCK_SHIFT;
	case 1:
		return TTBL_L2_BLOCK_SHIFT;
	case 2:
		return TTBL_L1_BLOCK_SHIFT;
	case 3:
		return TTBL_L0_BLOCK_SHIFT;
	default:
		break;
	};
	return TTBL_L3_BLOCK_SHIFT;
}

physical_addr_t arch_mmu_level_map_mask(int stage, int level)
{
	switch (level) {
	case 0:
		return TTBL_L3_MAP_MASK;
	case 1:
		return TTBL_L2_MAP_MASK;
	case 2:
		return TTBL_L1_MAP_MASK;
	case 3:
		return TTBL_L0_MAP_MASK;
	default:
		break;
	};
	return TTBL_L3_MAP_MASK;
}

int arch_mmu_level_index(physical_addr_t ia, int stage, int level)
{
	switch (level) {
	case 0:
		return (ia & TTBL_L3_INDEX_MASK) >> TTBL_L3_INDEX_SHIFT;
	case 1:
		return (ia & TTBL_L2_INDEX_MASK) >> TTBL_L2_INDEX_SHIFT;
	case 2:
		return (ia & TTBL_L1_INDEX_MASK) >> TTBL_L1_INDEX_SHIFT;
	case 3:
		return (ia & TTBL_L0_INDEX_MASK) >> TTBL_L0_INDEX_SHIFT;
	default:
		break;
	};
	return (ia & TTBL_L3_INDEX_MASK) >> TTBL_L3_INDEX_SHIFT;
}

int arch_mmu_level_index_shift(int stage, int level)
{
	switch (level) {
	case 0:
		return TTBL_L3_INDEX_SHIFT;
	case 1:
		return TTBL_L2_INDEX_SHIFT;
	case 2:
		return TTBL_L1_INDEX_SHIFT;
	case 3:
		return TTBL_L0_INDEX_SHIFT;
	default:
		break;
	};
	return TTBL_L3_INDEX_SHIFT;
}

void arch_mmu_stage1_pgflags_set(arch_pgflags_t *flags, u32 mem_flags)
{
	flags->af = 1;
	if (mem_flags & VMM_MEMORY_WRITEABLE) {
		flags->ap = TTBL_AP_SRW_U;
	} else if (mem_flags & VMM_MEMORY_READABLE) {
		flags->ap = TTBL_AP_SR_U;
	} else {
		flags->ap = TTBL_AP_SR_U;
	}
	flags->xn = (mem_flags & VMM_MEMORY_EXECUTABLE) ? 0 : 1;
	flags->ns = 1;
	flags->sh = TTBL_SH_INNER_SHAREABLE;

	if ((mem_flags & VMM_MEMORY_CACHEABLE) &&
	    (mem_flags & VMM_MEMORY_BUFFERABLE)) {
		flags->aindex = AINDEX_NORMAL_WB;
	} else if (mem_flags & VMM_MEMORY_CACHEABLE) {
		flags->aindex = AINDEX_NORMAL_WT;
	} else if (mem_flags & VMM_MEMORY_BUFFERABLE) {
		flags->aindex = AINDEX_NORMAL_WB;
	} else if (mem_flags & VMM_MEMORY_IO_DEVICE) {
		flags->aindex = AINDEX_DEVICE_nGnRE;
	} else if (mem_flags & VMM_MEMORY_DMA_COHERENT) {
		flags->aindex = AINDEX_NORMAL_WB;
	} else if (mem_flags & VMM_MEMORY_DMA_NONCOHERENT) {
		flags->aindex = AINDEX_NORMAL_NC;
	} else {
		flags->aindex = AINDEX_DEVICE_nGnRnE;
	}
}

void arch_mmu_pte_sync(arch_pte_t *pte, int stage, int level)
{
	cpu_mmu_sync_tte(pte);
}

void arch_mmu_pte_clear(arch_pte_t *pte, int stage, int level)
{
	*pte = 0x0;
}

bool arch_mmu_pte_is_valid(arch_pte_t *pte, int stage, int level)
{
	if (!level) {
		/* For last level, check both valid bit and table bit */
		return ((*pte & TTBL_TABLE_MASK) && (*pte & TTBL_VALID_MASK)) ?
			TRUE : FALSE;
	}
	/* For non-last levels, check valid bit or table bit */
	return ((*pte & TTBL_TABLE_MASK) || (*pte & TTBL_VALID_MASK)) ?
		TRUE : FALSE;
}

physical_addr_t arch_mmu_pte_addr(arch_pte_t *pte, int stage, int level)
{
	return *pte & TTBL_OUTADDR_MASK;
}

void arch_mmu_pte_flags(arch_pte_t *pte, int stage, int level,
			arch_pgflags_t *out_flags)
{
	if (stage == MMU_STAGE2) {
		out_flags->xn = (*pte & TTBL_STAGE2_UPPER_XN_MASK) >>
						TTBL_STAGE2_UPPER_XN_SHIFT;
		out_flags->cont = (*pte & TTBL_STAGE2_UPPER_CONT_MASK) >>
						TTBL_STAGE2_UPPER_CONT_SHIFT;
		out_flags->af = (*pte & TTBL_STAGE2_LOWER_AF_MASK) >>
						TTBL_STAGE2_LOWER_AF_SHIFT;
		out_flags->sh = (*pte & TTBL_STAGE2_LOWER_SH_MASK) >>
						TTBL_STAGE2_LOWER_SH_SHIFT;
		out_flags->ap = (*pte & TTBL_STAGE2_LOWER_HAP_MASK) >>
						TTBL_STAGE2_LOWER_HAP_SHIFT;
		out_flags->memattr = (*pte & TTBL_STAGE2_LOWER_MEMATTR_MASK) >>
					TTBL_STAGE2_LOWER_MEMATTR_SHIFT;
	} else {
		out_flags->xn = (*pte & TTBL_STAGE1_UPPER_XN_MASK) >>
						TTBL_STAGE1_UPPER_XN_SHIFT;
		out_flags->pxn = (*pte & TTBL_STAGE1_UPPER_PXN_MASK) >>
						TTBL_STAGE1_UPPER_PXN_SHIFT;
		out_flags->cont = (*pte & TTBL_STAGE1_UPPER_CONT_MASK) >>
						TTBL_STAGE1_UPPER_CONT_SHIFT;
		out_flags->ng = (*pte & TTBL_STAGE1_LOWER_NG_MASK) >>
						TTBL_STAGE1_LOWER_NG_SHIFT;
		out_flags->af = (*pte & TTBL_STAGE1_LOWER_AF_MASK) >>
						TTBL_STAGE1_LOWER_AF_SHIFT;
		out_flags->sh = (*pte & TTBL_STAGE1_LOWER_SH_MASK) >>
						TTBL_STAGE1_LOWER_SH_SHIFT;
		out_flags->ap = (*pte & TTBL_STAGE1_LOWER_AP_MASK) >>
						TTBL_STAGE1_LOWER_AP_SHIFT;
		out_flags->ns = (*pte & TTBL_STAGE1_LOWER_NS_MASK) >>
						TTBL_STAGE1_LOWER_NS_SHIFT;
		out_flags->aindex = (*pte & TTBL_STAGE1_LOWER_AINDEX_MASK) >>
						TTBL_STAGE1_LOWER_AINDEX_SHIFT;
	}
}

void arch_mmu_pte_set(arch_pte_t *pte, int stage, int level,
		      physical_addr_t pa, arch_pgflags_t *flags)
{
	*pte = pa & arch_mmu_level_map_mask(stage, level);
 	*pte = *pte & TTBL_OUTADDR_MASK;

	if (stage == MMU_STAGE2) {
		*pte |= ((u64)flags->xn << TTBL_STAGE2_UPPER_XN_SHIFT) &
						TTBL_STAGE2_UPPER_XN_MASK;
		*pte |= ((u64)flags->cont << TTBL_STAGE2_UPPER_CONT_SHIFT) &
						TTBL_STAGE2_UPPER_CONT_MASK;
		*pte |= ((u64)flags->af << TTBL_STAGE2_LOWER_AF_SHIFT) &
						TTBL_STAGE1_LOWER_AF_MASK;
		*pte |= ((u64)flags->sh << TTBL_STAGE2_LOWER_SH_SHIFT) &
						TTBL_STAGE2_LOWER_SH_MASK;
		*pte |= ((u64)flags->ap << TTBL_STAGE2_LOWER_HAP_SHIFT) &
						TTBL_STAGE2_LOWER_HAP_MASK;
		*pte |= ((u64)flags->memattr <<
				TTBL_STAGE2_LOWER_MEMATTR_SHIFT) &
				TTBL_STAGE2_LOWER_MEMATTR_MASK;
	} else {
		*pte |= ((u64)flags->xn << TTBL_STAGE1_UPPER_XN_SHIFT) &
						TTBL_STAGE1_UPPER_XN_MASK;
		*pte |= ((u64)flags->pxn << TTBL_STAGE1_UPPER_PXN_SHIFT) &
						TTBL_STAGE1_UPPER_PXN_MASK;
		*pte |= ((u64)flags->cont << TTBL_STAGE1_UPPER_CONT_SHIFT) &
						TTBL_STAGE1_UPPER_CONT_MASK;
		*pte |= ((u64)flags->ng << TTBL_STAGE1_LOWER_NG_SHIFT) &
						TTBL_STAGE1_LOWER_NG_MASK;
		*pte |= ((u64)flags->af << TTBL_STAGE1_LOWER_AF_SHIFT) &
						TTBL_STAGE1_LOWER_AF_MASK;
		*pte |= ((u64)flags->sh << TTBL_STAGE1_LOWER_SH_SHIFT) &
						TTBL_STAGE1_LOWER_SH_MASK;
		*pte |= ((u64)flags->ap << TTBL_STAGE1_LOWER_AP_SHIFT) &
						TTBL_STAGE1_LOWER_AP_MASK;
		*pte |= ((u64)flags->ns << TTBL_STAGE1_LOWER_NS_SHIFT) &
						TTBL_STAGE1_LOWER_NS_MASK;
		*pte |= ((u64)flags->aindex <<
				TTBL_STAGE1_LOWER_AINDEX_SHIFT) &
				TTBL_STAGE1_LOWER_AINDEX_MASK;
	}

	if (!level) {
		*pte |= TTBL_TABLE_MASK;
	}
	*pte |= TTBL_VALID_MASK;
}

bool arch_mmu_pte_is_table(arch_pte_t *pte, int stage, int level)
{
	if (!level) {
		/* For last level, always return false */
		return FALSE;
	}
	/* For non-last levels, check both valid bit and table bit */
	return ((*pte & TTBL_TABLE_MASK) && (*pte & TTBL_VALID_MASK)) ?
		TRUE : FALSE;
}

physical_addr_t arch_mmu_pte_table_addr(arch_pte_t *pte, int stage, int level)
{
	return *pte & TTBL_OUTADDR_MASK;
}

void arch_mmu_pte_set_table(arch_pte_t *pte, int stage, int level,
			    physical_addr_t tbl_pa)
{
	*pte = tbl_pa & TTBL_OUTADDR_MASK;
	*pte |= (TTBL_TABLE_MASK | TTBL_VALID_MASK);
}

physical_addr_t arch_mmu_stage2_current_pgtbl_addr(void)
{
	return cpu_stage2_ttbl_pa();
}

u32 arch_mmu_stage2_current_vmid(void)
{
	return cpu_stage2_vmid();
}

int arch_mmu_stage2_change_pgtbl(u32 vmid, physical_addr_t tbl_phys)
{
	cpu_stage2_update(tbl_phys, vmid);

	return VMM_OK;
}

/* Initialized by memory read/write init */
static struct mmu_pgtbl *mem_rw_ttbl[CONFIG_CPU_COUNT];
static arch_pte_t *mem_rw_tte[CONFIG_CPU_COUNT];
static physical_addr_t mem_rw_outaddr_mask[CONFIG_CPU_COUNT];

#define PHYS_RW_TTE							\
	((TTBL_TABLE_MASK|TTBL_VALID_MASK)	|			\
	 ((0x1ULL << TTBL_STAGE1_UPPER_XN_SHIFT) &			\
	    TTBL_STAGE1_UPPER_XN_MASK)		|			\
	 ((0x1 << TTBL_STAGE1_LOWER_AF_SHIFT) &				\
	    TTBL_STAGE1_LOWER_AF_MASK)		|			\
	 ((TTBL_SH_INNER_SHAREABLE << TTBL_STAGE1_LOWER_SH_SHIFT) &	\
	    TTBL_STAGE1_LOWER_SH_MASK)		|			\
	 ((TTBL_AP_SRW_U << TTBL_STAGE1_LOWER_AP_SHIFT) &		\
	    TTBL_STAGE1_LOWER_AP_MASK)		|			\
	 ((0x1 << TTBL_STAGE1_LOWER_NS_SHIFT) &				\
	    TTBL_STAGE1_LOWER_NS_MASK))

#define PHYS_RW_TTE_NOCACHE						\
	(PHYS_RW_TTE |							\
	 ((AINDEX_NORMAL_NC << TTBL_STAGE1_LOWER_AINDEX_SHIFT) &	\
	    TTBL_STAGE1_LOWER_AINDEX_MASK))

#define PHYS_RW_TTE_CACHE						\
	(PHYS_RW_TTE |							\
	 ((AINDEX_NORMAL_WB << TTBL_STAGE1_LOWER_AINDEX_SHIFT) &	\
	    TTBL_STAGE1_LOWER_AINDEX_MASK))

int arch_cpu_aspace_memory_read(virtual_addr_t tmp_va,
				physical_addr_t src,
				void *dst, u32 len, bool cacheable)
{
	u64 old_tte_val;
	u32 cpu = vmm_smp_processor_id();
	arch_pte_t *tte = mem_rw_tte[cpu];
	physical_addr_t outaddr_mask = mem_rw_outaddr_mask[cpu];
	virtual_addr_t offset = (src & VMM_PAGE_MASK);

	old_tte_val = *tte;

	if (cacheable) {
		*tte = PHYS_RW_TTE_CACHE;
	} else {
		*tte = PHYS_RW_TTE_NOCACHE;
	}
	*tte |= src & outaddr_mask;

	cpu_mmu_sync_tte(tte);
	cpu_invalid_va_hypervisor_tlb(tmp_va);

	switch (len) {
	case 1:
		*((u8 *)dst) = *(u8 *)(tmp_va + offset);
		break;
	case 2:
		*((u16 *)dst) = *(u16 *)(tmp_va + offset);
		break;
	case 4:
		*((u32 *)dst) = *(u32 *)(tmp_va + offset);
		break;
	case 8:
		*((u64 *)dst) = *(u64 *)(tmp_va + offset);
		break;
	default:
		memcpy(dst, (void *)(tmp_va + offset), len);
		break;
	};

	*tte = old_tte_val;
	cpu_mmu_sync_tte(tte);

	return VMM_OK;
}

int arch_cpu_aspace_memory_write(virtual_addr_t tmp_va,
				 physical_addr_t dst,
				 void *src, u32 len, bool cacheable)
{
	u64 old_tte_val;
	u32 cpu = vmm_smp_processor_id();
	arch_pte_t *tte = mem_rw_tte[cpu];
	physical_addr_t outaddr_mask = mem_rw_outaddr_mask[cpu];
	virtual_addr_t offset = (dst & VMM_PAGE_MASK);

	old_tte_val = *tte;

	if (cacheable) {
		*tte = PHYS_RW_TTE_CACHE;
	} else {
		*tte = PHYS_RW_TTE_NOCACHE;
	}
	*tte |= dst & outaddr_mask;

	cpu_mmu_sync_tte(tte);
	cpu_invalid_va_hypervisor_tlb(tmp_va);

	switch (len) {
	case 1:
		*(u8 *)(tmp_va + offset) = *((u8 *)src);
		break;
	case 2:
		*(u16 *)(tmp_va + offset) = *((u16 *)src);
		break;
	case 4:
		*(u32 *)(tmp_va + offset) = *((u32 *)src);
		break;
	case 8:
		*(u64 *)(tmp_va + offset) = *((u64 *)src);
		break;
	default:
		memcpy((void *)(tmp_va + offset), src, len);
		break;
	};

	*tte = old_tte_val;
	cpu_mmu_sync_tte(tte);

	return VMM_OK;
}

int __cpuinit arch_cpu_aspace_memory_rwinit(virtual_addr_t tmp_va)
{
	int rc;
	u32 cpu = vmm_smp_processor_id();
	struct mmu_page p;

	memset(&p, 0, sizeof(p));
	p.ia = tmp_va;
	p.oa = 0x0;
	p.sz = VMM_PAGE_SIZE;
	p.flags.af = 1;
	p.flags.ap = TTBL_AP_SR_U;
	p.flags.xn = 1;
	p.flags.ns = 1;
	p.flags.sh = TTBL_SH_INNER_SHAREABLE;
	p.flags.aindex = AINDEX_NORMAL_NC;

	rc = mmu_map_hypervisor_page(&p);
	if (rc) {
		return rc;
	}

	mem_rw_tte[cpu] = NULL;
	mem_rw_ttbl[cpu] = NULL;

	rc = mmu_find_pte(mmu_hypervisor_pgtbl(), tmp_va,
			  &mem_rw_tte[cpu], &mem_rw_ttbl[cpu]);
	if (rc) {
		return rc;
	}

	mem_rw_outaddr_mask[cpu] =
		arch_mmu_level_map_mask(MMU_STAGE1, mem_rw_ttbl[cpu]->level);

	return VMM_OK;
}
