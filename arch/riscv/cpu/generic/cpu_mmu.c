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
 * @file cpu_mmu.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of Arch MMU for RISC-V CPUs
 */

#include <vmm_error.h>
#include <vmm_types.h>
#include <vmm_smp.h>
#include <vmm_host_aspace.h>
#include <libs/stringlib.h>
#include <generic_mmu.h>
#include <arch_barrier.h>

#include <cpu_hwcap.h>
#include <cpu_tlb.h>
#include <cpu_sbi.h>

#ifdef CONFIG_64BIT
/* Assume Sv39 */
unsigned long riscv_stage1_mode = SATP_MODE_SV39;
#else
/* Assume Sv32 */
unsigned long riscv_stage1_mode = SATP_MODE_SV32;
#endif

void arch_mmu_pgtbl_clear(virtual_addr_t tbl_va)
{
	memset((void *)tbl_va, 0, ARCH_MMU_PGTBL_SIZE);
}

void arch_mmu_stage2_tlbflush(bool remote,
			      physical_addr_t gpa, physical_size_t gsz)
{
	physical_addr_t off;

	if (remote) {
		sbi_remote_hfence_gvma(NULL, gpa, gsz);
	} else {
		for (off = 0; off < gsz; off += VMM_PAGE_SIZE)
			__hfence_gvma_gpa(gpa + off);
	}
}

void arch_mmu_stage1_tlbflush(bool remote,
			      virtual_addr_t va, virtual_size_t sz)
{
	virtual_addr_t off;

	if (remote) {
		sbi_remote_sfence_vma(NULL, va, sz);
	} else {
		for (off = 0; off < sz; off += VMM_PAGE_SIZE)
			__sfence_vma_va(va + off);
	}
}

bool arch_mmu_valid_block_size(physical_size_t sz)
{
	if (
#ifdef CONFIG_64BIT
	    (sz == PGTBL_L3_BLOCK_SIZE) ||
	    (sz == PGTBL_L2_BLOCK_SIZE) ||
#endif
	    (sz == PGTBL_L1_BLOCK_SIZE) ||
	    (sz == PGTBL_L0_BLOCK_SIZE)) {
		return TRUE;
	}
	return FALSE;
}

int arch_mmu_start_level(int stage)
{
	if (stage == MMU_STAGE1) {
		switch (riscv_stage1_mode) {
		case SATP_MODE_SV32:
			return 1;
	#ifdef CONFIG_64BIT
		case SATP_MODE_SV39:
			return 2;
		case SATP_MODE_SV48:
			return 3;
	#endif
		default:
			return 0;
		};
	} else {
		switch (riscv_stage2_mode) {
		case HGATP_MODE_SV32X4:
			return 1;
	#ifdef CONFIG_64BIT
		case HGATP_MODE_SV39X4:
			return 2;
		case HGATP_MODE_SV48X4:
			return 3;
	#endif
		default:
			return 0;
		};
	}
}

physical_size_t arch_mmu_level_block_size(int stage, int level)
{
	switch (level) {
	case 0:
		return PGTBL_L0_BLOCK_SIZE;
	case 1:
		return PGTBL_L1_BLOCK_SIZE;
#ifdef CONFIG_64BIT
	case 2:
		return PGTBL_L2_BLOCK_SIZE;
	case 3:
		return PGTBL_L3_BLOCK_SIZE;
#endif
	default:
		break;
	};
	return PGTBL_L0_BLOCK_SIZE;
}

int arch_mmu_level_block_shift(int stage, int level)
{
	switch (level) {
	case 0:
		return PGTBL_L0_BLOCK_SHIFT;
	case 1:
		return PGTBL_L1_BLOCK_SHIFT;
#ifdef CONFIG_64BIT
	case 2:
		return PGTBL_L2_BLOCK_SHIFT;
	case 3:
		return PGTBL_L3_BLOCK_SHIFT;
#endif
	default:
		break;
	};
	return PGTBL_L0_BLOCK_SHIFT;
}

physical_addr_t arch_mmu_level_map_mask(int stage, int level)
{
	switch (level) {
	case 0:
		return PGTBL_L0_MAP_MASK;
	case 1:
		return PGTBL_L1_MAP_MASK;
#ifdef CONFIG_64BIT
	case 2:
		return PGTBL_L2_MAP_MASK;
	case 3:
		return PGTBL_L3_MAP_MASK;
#endif
	default:
		break;
	};
	return PGTBL_L0_MAP_MASK;
}

int arch_mmu_level_index(physical_addr_t ia, int stage, int level)
{
	switch (level) {
	case 0:
		return (ia & PGTBL_L0_INDEX_MASK) >> PGTBL_L0_INDEX_SHIFT;
	case 1:
		return (ia & PGTBL_L1_INDEX_MASK) >> PGTBL_L1_INDEX_SHIFT;
#ifdef CONFIG_64BIT
	case 2:
		return (ia & PGTBL_L2_INDEX_MASK) >> PGTBL_L2_INDEX_SHIFT;
	case 3:
		return (ia & PGTBL_L3_INDEX_MASK) >> PGTBL_L3_INDEX_SHIFT;
#endif
	default:
		break;
	};
	return (ia & PGTBL_L0_INDEX_MASK) >> PGTBL_L0_INDEX_SHIFT;
}

int arch_mmu_level_index_shift(int stage, int level)
{
	switch (level) {
	case 0:
		return PGTBL_L0_INDEX_SHIFT;
	case 1:
		return PGTBL_L1_INDEX_SHIFT;
#ifdef CONFIG_64BIT
	case 2:
		return PGTBL_L2_INDEX_SHIFT;
	case 3:
		return PGTBL_L3_INDEX_SHIFT;
#endif
	default:
		break;
	};
	return PGTBL_L0_INDEX_SHIFT;
}

void arch_mmu_stage1_pgflags_set(arch_pgflags_t *flags, u32 mem_flags)
{
	flags->rsw = 0;
	flags->accessed = 0;
	flags->dirty = 0;
	flags->global = 1;
	flags->user = 0;
	flags->execute = (mem_flags & VMM_MEMORY_EXECUTABLE) ? 0 : 1;
	flags->write = (mem_flags & VMM_MEMORY_WRITEABLE) ? 1 : 0;
	flags->read = (mem_flags & VMM_MEMORY_READABLE) ? 1 : 0;
	flags->valid = 1;

	/*
	 * We ignore following flags:
	 * VMM_MEMORY_CACHEABLE
	 * VMM_MEMORY_BUFFERABLE
	 * VMM_MEMORY_IO_DEVICE
	 * VMM_MEMORY_DMA_COHERENT
	 * VMM_MEMORY_DMA_NONCOHERENT
	 */
}

void arch_mmu_pte_sync(arch_pte_t *pte, int stage, int level)
{
	arch_smp_mb();
}

void arch_mmu_pte_clear(arch_pte_t *pte, int stage, int level)
{
	*pte = 0x0;
}

bool arch_mmu_pte_is_valid(arch_pte_t *pte, int stage, int level)
{
	return (*pte & PGTBL_PTE_VALID_MASK) ? TRUE : FALSE;
}

physical_addr_t arch_mmu_pte_addr(arch_pte_t *pte,
				  int stage, int level)
{
	return ((*pte & PGTBL_PTE_ADDR_MASK) >> PGTBL_PTE_ADDR_SHIFT) <<
					PGTBL_PAGE_SIZE_SHIFT;
}

void arch_mmu_pte_flags(arch_pte_t *pte, int stage, int level,
			arch_pgflags_t *out_flags)
{
	out_flags->rsw = (*pte & PGTBL_PTE_RSW_MASK) >>
					PGTBL_PTE_RSW_SHIFT;
	out_flags->dirty = (*pte & PGTBL_PTE_DIRTY_MASK) >>
					PGTBL_PTE_DIRTY_SHIFT;
	out_flags->accessed = (*pte & PGTBL_PTE_ACCESSED_MASK) >>
					PGTBL_PTE_ACCESSED_SHIFT;
	out_flags->global = (*pte & PGTBL_PTE_GLOBAL_MASK) >>
					PGTBL_PTE_GLOBAL_SHIFT;
	out_flags->user = (*pte & PGTBL_PTE_USER_MASK) >>
					PGTBL_PTE_USER_SHIFT;
	out_flags->execute = (*pte & PGTBL_PTE_EXECUTE_MASK) >>
					PGTBL_PTE_EXECUTE_SHIFT;
	out_flags->write = (*pte & PGTBL_PTE_WRITE_MASK) >>
					PGTBL_PTE_WRITE_SHIFT;
	out_flags->read = (*pte & PGTBL_PTE_READ_MASK) >>
					PGTBL_PTE_READ_SHIFT;
	out_flags->valid = (*pte & PGTBL_PTE_VALID_MASK) >>
					PGTBL_PTE_VALID_SHIFT;
}

void arch_mmu_pte_set(arch_pte_t *pte, int stage, int level,
		      physical_addr_t pa, arch_pgflags_t *flags)
{
	*pte = pa & arch_mmu_level_map_mask(stage, level);
	*pte = *pte >> PGTBL_PAGE_SIZE_SHIFT;
	*pte = *pte << PGTBL_PTE_ADDR_SHIFT;
	*pte |= ((arch_pte_t)flags->rsw << PGTBL_PTE_RSW_SHIFT) &
					PGTBL_PTE_RSW_MASK;
	*pte |= ((arch_pte_t)flags->dirty << PGTBL_PTE_DIRTY_SHIFT) &
					PGTBL_PTE_DIRTY_MASK;
	*pte |= ((arch_pte_t)flags->accessed << PGTBL_PTE_ACCESSED_SHIFT) &
					PGTBL_PTE_ACCESSED_MASK;
	*pte |= ((arch_pte_t)flags->global << PGTBL_PTE_GLOBAL_SHIFT) &
					PGTBL_PTE_GLOBAL_MASK;
	*pte |= ((arch_pte_t)flags->user << PGTBL_PTE_USER_SHIFT) &
					PGTBL_PTE_USER_MASK;
	*pte |= ((arch_pte_t)flags->execute << PGTBL_PTE_EXECUTE_SHIFT) &
					PGTBL_PTE_EXECUTE_MASK;
	*pte |= ((arch_pte_t)flags->write << PGTBL_PTE_WRITE_SHIFT) &
					PGTBL_PTE_WRITE_MASK;
	*pte |= ((arch_pte_t)flags->read << PGTBL_PTE_READ_SHIFT) &
					PGTBL_PTE_READ_MASK;
	*pte |= PGTBL_PTE_VALID_MASK;
}

bool arch_mmu_pte_is_table(arch_pte_t *pte, int stage, int level)
{
	return (*pte & PGTBL_PTE_PERM_MASK) ? FALSE : TRUE;
}

physical_addr_t arch_mmu_pte_table_addr(arch_pte_t *pte, int stage, int level)
{
	return ((*pte & PGTBL_PTE_ADDR_MASK) >> PGTBL_PTE_ADDR_SHIFT) <<
					PGTBL_PAGE_SIZE_SHIFT;
}

void arch_mmu_pte_set_table(arch_pte_t *pte, int stage, int level,
			    physical_addr_t tbl_pa)
{
	*pte = tbl_pa >> PGTBL_PAGE_SIZE_SHIFT;
	*pte = *pte << PGTBL_PTE_ADDR_SHIFT;
	*pte |= PGTBL_PTE_VALID_MASK;
}

physical_addr_t arch_mmu_stage2_current_pgtbl_addr(void)
{
	unsigned long pgtbl_ppn = csr_read(CSR_HGATP) & HGATP_PPN;
	return pgtbl_ppn << PGTBL_PAGE_SIZE_SHIFT;
}

u32 arch_mmu_stage2_current_vmid(void)
{
	return (csr_read(CSR_HGATP) & HGATP_VMID_MASK) >> HGATP_VMID_SHIFT;
}

int arch_mmu_stage2_change_pgtbl(u32 vmid, physical_addr_t tbl_phys)
{
	unsigned long hgatp;

	hgatp = riscv_stage2_mode << HGATP_MODE_SHIFT;
	hgatp |= ((unsigned long)vmid << HGATP_VMID_SHIFT) & HGATP_VMID_MASK;
	hgatp |= (tbl_phys >> PGTBL_PAGE_SIZE_SHIFT) & HGATP_PPN;

	csr_write(CSR_HGATP, hgatp);

	return VMM_OK;
}
