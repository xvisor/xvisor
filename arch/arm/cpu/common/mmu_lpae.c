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
#include <vmm_guest_aspace.h>
#include <vmm_host_aspace.h>
#include <generic_mmu.h>

int arch_mmu_pgtbl_min_align_order(int stage)
{
	return TTBL_L3_BLOCK_SHIFT;
}

int arch_mmu_pgtbl_align_order(int stage, int level)
{
	return TTBL_L3_BLOCK_SHIFT;
}

int arch_mmu_pgtbl_size_order(int stage, int level)
{
	return TTBL_L3_BLOCK_SHIFT;
}

void arch_mmu_stage2_tlbflush(bool remote, bool use_vmid, u32 vmid,
			      physical_addr_t gpa, physical_size_t gsz)
{
	cpu_invalid_ipa_guest_tlb(gpa);
}

void arch_mmu_stage1_tlbflush(bool remote, bool use_asid, u32 asid,
			      virtual_addr_t va, virtual_size_t sz)
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

void arch_mmu_pgflags_set(arch_pgflags_t *flags, int stage, u32 mflags)
{
	if (stage == MMU_STAGE2) {
		flags->sh = 3U;
		if (mflags & VMM_REGION_VIRTUAL) {
			flags->af = 0;
			flags->ap = TTBL_HAP_NOACCESS;
		} else if (mflags & VMM_REGION_READONLY) {
			flags->af = 1;
			flags->ap = TTBL_HAP_READONLY;
		} else {
			flags->af = 1;
			flags->ap = TTBL_HAP_READWRITE;
		}

		/* memattr in stage 2
		 * ------------------
		 *  0x0 - strongly ordered
		 *  0x5 - normal-memory NC
		 *  0xA - normal-memory WT
		 *  0xF - normal-memory WB
		 */
		if (mflags & VMM_REGION_CACHEABLE) {
			if (mflags & VMM_REGION_BUFFERABLE) {
				flags->memattr = 0xF;
			} else {
				flags->memattr = 0xA;
			}
		} else {
			flags->memattr = 0x0;
		}
	} else {
		flags->af = 1;
		if (mflags & VMM_MEMORY_WRITEABLE) {
			flags->ap = TTBL_AP_SRW_U;
		} else if (mflags & VMM_MEMORY_READABLE) {
			flags->ap = TTBL_AP_SR_U;
		} else {
			flags->af = 0;
			flags->ap = TTBL_AP_SR_U;
		}
		flags->xn = (mflags & VMM_MEMORY_EXECUTABLE) ? 0 : 1;
		flags->ns = 1;
		flags->sh = TTBL_SH_INNER_SHAREABLE;

		if ((mflags & VMM_MEMORY_CACHEABLE) &&
		    (mflags & VMM_MEMORY_BUFFERABLE)) {
			flags->aindex = AINDEX_NORMAL_WB;
		} else if (mflags & VMM_MEMORY_CACHEABLE) {
			flags->aindex = AINDEX_NORMAL_WT;
		} else if (mflags & VMM_MEMORY_BUFFERABLE) {
			flags->aindex = AINDEX_NORMAL_WB;
		} else if (mflags & VMM_MEMORY_IO_DEVICE) {
			flags->aindex = AINDEX_DEVICE_nGnRE;
		} else if (mflags & VMM_MEMORY_DMA_COHERENT) {
			flags->aindex = AINDEX_NORMAL_WB;
		} else if (mflags & VMM_MEMORY_DMA_NONCOHERENT) {
			flags->aindex = AINDEX_NORMAL_NC;
		} else {
			flags->aindex = AINDEX_NORMAL_NC;
		}
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

int arch_mmu_test_nested_pgtbl(physical_addr_t s2_tbl_pa,
				bool s1_avail, physical_addr_t s1_tbl_pa,
				u32 flags, virtual_addr_t addr,
				physical_addr_t *out_addr,
				u32 *out_fault_flags)
{
	irq_flags_t f;
	struct mmu_page pg;
	struct mmu_pgtbl *s1_pgtbl, *s2_pgtbl;
	struct cpu_mmu_at_test_result t = { 0 };

	arch_cpu_irq_save(f);
	cpu_mmu_at_test_exec(s2_tbl_pa, s1_avail, s1_tbl_pa, addr,
			     (flags & MMU_TEST_WRITE) ? TRUE : FALSE, &t);
	arch_cpu_irq_restore(f);

	*out_addr = 0;
	*out_fault_flags = 0;
	if (t.fault) {
		*out_fault_flags |= (t.fault_s2) ? 0 : MMU_TEST_FAULT_S1;

		if (t.fault_translation) {
			*out_fault_flags |= MMU_TEST_FAULT_NOMAP;
		} else if (t.fault_unknown) {
			*out_fault_flags |= MMU_TEST_FAULT_UNKNOWN;
		}

		*out_fault_flags |= (flags & MMU_TEST_WRITE) ?
				MMU_TEST_FAULT_WRITE : MMU_TEST_FAULT_READ;

		*out_addr = addr;
		if (*out_fault_flags & MMU_TEST_FAULT_NOMAP) {
			if (s1_avail) {
				s1_pgtbl = mmu_pgtbl_find(MMU_STAGE1, s1_tbl_pa);
				if (!s1_pgtbl) {
					return VMM_EFAIL;
				}

				if (!mmu_get_page(s1_pgtbl, *out_addr, &pg)) {
					*out_addr = pg.oa | (addr & (pg.sz - 1));
				}
			}

			s2_pgtbl = mmu_pgtbl_find(MMU_STAGE2, s2_tbl_pa);
			if (!s2_pgtbl) {
				return VMM_EFAIL;
			}

			if (!mmu_get_page(s2_pgtbl, *out_addr, &pg)) {
				*out_addr = pg.oa | (*out_addr & (pg.sz - 1));
			}
		} else if (*out_fault_flags & (MMU_TEST_FAULT_READ |
						MMU_TEST_FAULT_WRITE)) {
			if (*out_fault_flags & MMU_TEST_FAULT_S1) {
				*out_addr = addr;
			} else if (s1_avail) {
				s1_pgtbl = mmu_pgtbl_find(MMU_STAGE1, s1_tbl_pa);
				if (!s1_pgtbl) {
					return VMM_EFAIL;
				}

				if (!mmu_get_page(s1_pgtbl, *out_addr, &pg)) {
					*out_addr = pg.oa | (addr & (pg.sz - 1));
				}
			}
		}
	} else {
		*out_addr = t.addr;
	}

	return 0;
}

physical_addr_t arch_mmu_stage2_current_pgtbl_addr(void)
{
	return cpu_stage2_ttbl_pa();
}

u32 arch_mmu_stage2_current_vmid(void)
{
	return cpu_stage2_vmid();
}

int arch_mmu_stage2_change_pgtbl(bool have_vmid, u32 vmid,
				 physical_addr_t tbl_phys)
{
	cpu_stage2_update(tbl_phys, vmid);

	return VMM_OK;
}
