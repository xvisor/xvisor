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
 * @file cpu_mmu_initial_pgtbl.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Initial page table setup at boot-time
 */

#include <vmm_types.h>
#include <cpu_mmu.h>

struct cpu_mmu_entry_ctrl {
	int num_levels;
	u32 pgtbl_count;
	int *pgtbl_tree;
	cpu_pte_t *next_pgtbl;
	virtual_addr_t pgtbl_base;
};

extern u8 def_pgtbl[];
extern int def_pgtbl_tree[];
#ifdef CONFIG_DEFTERM_EARLY_PRINT
extern u8 defterm_early_base[];
#endif

void __attribute__ ((section(".entry")))
    __setup_initial_pgtbl(struct cpu_mmu_entry_ctrl *entry,
			  virtual_addr_t map_start,
			  virtual_addr_t map_end,
			  virtual_addr_t pa_start,
			  bool writeable)
{
	u32 i, index;
	cpu_pte_t *pgtbl;
	virtual_addr_t page_addr;

	/* align start addresses */
	map_start &= PGTBL_L0_MAP_MASK;
	pa_start &= PGTBL_L0_MAP_MASK;

	page_addr = map_start;
	while (page_addr < map_end) {
		pgtbl = (cpu_pte_t *)entry->pgtbl_base;

		/* Setup level3 table */
		if (entry->num_levels < 4) {
			goto skip_level3;
		}
#if CONFIG_64BIT
		index = (page_addr & PGTBL_L3_INDEX_MASK) >> PGTBL_L3_INDEX_SHIFT;
		if (pgtbl[index] & PGTBL_PTE_VALID_MASK) {
			/* Find level2 table */
			pgtbl = (cpu_pte_t *)(unsigned long)
				(((pgtbl[index] & PGTBL_PTE_ADDR_MASK)
				  >> PGTBL_PTE_ADDR_SHIFT)
				 << PGTBL_PAGE_SIZE_SHIFT);
		} else {
			/* Allocate new level2 table */
			if (entry->pgtbl_count == PGTBL_INITIAL_TABLE_COUNT) {
				while (1) ;	/* No initial table available */
			}
			for (i = 0; i < PGTBL_TABLE_ENTCNT; i++) {
				entry->next_pgtbl[i] = 0x0ULL;
			}
			entry->pgtbl_tree[entry->pgtbl_count] =
			    ((virtual_addr_t)pgtbl - entry->pgtbl_base) >>
			    PGTBL_TABLE_SIZE_SHIFT;
			entry->pgtbl_count++;
			pgtbl[index] = (virtual_addr_t)entry->next_pgtbl;
			pgtbl[index] = pgtbl[index] >> PGTBL_PAGE_SIZE_SHIFT;
			pgtbl[index] = pgtbl[index] << PGTBL_PTE_ADDR_SHIFT;
			pgtbl[index] |= PGTBL_PTE_VALID_MASK;
			pgtbl = entry->next_pgtbl;
			entry->next_pgtbl += PGTBL_TABLE_ENTCNT;
		}
#endif
skip_level3:

		/* Setup level2 table */
		if (entry->num_levels < 3) {
			goto skip_level2;
		}
#if CONFIG_64BIT
		index = (page_addr & PGTBL_L2_INDEX_MASK) >> PGTBL_L2_INDEX_SHIFT;
		if (pgtbl[index] & PGTBL_PTE_VALID_MASK) {
			/* Find level1 table */
			pgtbl = (cpu_pte_t *)(unsigned long)
				(((pgtbl[index] & PGTBL_PTE_ADDR_MASK)
				  >> PGTBL_PTE_ADDR_SHIFT)
				 << PGTBL_PAGE_SIZE_SHIFT);
		} else {
			/* Allocate new level1 table */
			if (entry->pgtbl_count == PGTBL_INITIAL_TABLE_COUNT) {
				while (1) ;	/* No initial table available */
			}
			for (i = 0; i < PGTBL_TABLE_ENTCNT; i++) {
				entry->next_pgtbl[i] = 0x0ULL;
			}
			entry->pgtbl_tree[entry->pgtbl_count] =
			    ((virtual_addr_t)pgtbl - entry->pgtbl_base) >>
			    PGTBL_TABLE_SIZE_SHIFT;
			entry->pgtbl_count++;
			pgtbl[index] = (virtual_addr_t)entry->next_pgtbl;
			pgtbl[index] = pgtbl[index] >> PGTBL_PAGE_SIZE_SHIFT;
			pgtbl[index] = pgtbl[index] << PGTBL_PTE_ADDR_SHIFT;
			pgtbl[index] |= PGTBL_PTE_VALID_MASK;
			pgtbl = entry->next_pgtbl;
			entry->next_pgtbl += PGTBL_TABLE_ENTCNT;
		}
#endif
skip_level2:

		/* Setup level1 table */
		if (entry->num_levels < 2) {
			goto skip_level1;
		}
		index = (page_addr & PGTBL_L1_INDEX_MASK) >> PGTBL_L1_INDEX_SHIFT;
		if (pgtbl[index] & PGTBL_PTE_VALID_MASK) {
			/* Find level0 table */
			pgtbl = (cpu_pte_t *)(unsigned long)
				(((pgtbl[index] & PGTBL_PTE_ADDR_MASK)
				  >> PGTBL_PTE_ADDR_SHIFT)
				 << PGTBL_PAGE_SIZE_SHIFT);
		} else {
			/* Allocate new level0 table */
			if (entry->pgtbl_count == PGTBL_INITIAL_TABLE_COUNT) {
				while (1) ;	/* No initial table available */
			}
			for (i = 0; i < PGTBL_TABLE_ENTCNT; i++) {
				entry->next_pgtbl[i] = 0x0ULL;
			}
			entry->pgtbl_tree[entry->pgtbl_count] =
			    ((virtual_addr_t)pgtbl - entry->pgtbl_base) >>
			    PGTBL_TABLE_SIZE_SHIFT;
			entry->pgtbl_count++;
			pgtbl[index] = (virtual_addr_t)entry->next_pgtbl;
			pgtbl[index] = pgtbl[index] >> PGTBL_PAGE_SIZE_SHIFT;
			pgtbl[index] = pgtbl[index] << PGTBL_PTE_ADDR_SHIFT;
			pgtbl[index] |= PGTBL_PTE_VALID_MASK;
			pgtbl = entry->next_pgtbl;
			entry->next_pgtbl += PGTBL_TABLE_ENTCNT;
		}
skip_level1:

		/* Setup level0 table */
		index = (page_addr & PGTBL_L0_INDEX_MASK) >> PGTBL_L0_INDEX_SHIFT;
		if (!(pgtbl[index] & PGTBL_PTE_VALID_MASK)) {
			/* Update level0 table */
			pgtbl[index] = (page_addr - map_start) + pa_start;
			pgtbl[index] = pgtbl[index] >> PGTBL_PAGE_SIZE_SHIFT;
			pgtbl[index] = pgtbl[index] << PGTBL_PTE_ADDR_SHIFT;
			pgtbl[index] |= PGTBL_PTE_EXECUTE_MASK;
			pgtbl[index] |= (writeable) ? PGTBL_PTE_WRITE_MASK : 0;
			pgtbl[index] |= PGTBL_PTE_READ_MASK;
			pgtbl[index] |= PGTBL_PTE_VALID_MASK;
		}

		/* Point to next page */
		page_addr += PGTBL_L0_BLOCK_SIZE;
	}
}

/* Note: This functions must be called with MMU disabled from
 * primary CPU only.
 * Note: This functions cannot refer to any global variable &
 * functions to ensure that it can execute from anywhere.
 */
#define to_load_pa(va)	({ \
			virtual_addr_t _tva = (va); \
			if (exec_start <= _tva && _tva < exec_end) { \
				_tva = _tva - exec_start + load_start; \
			} \
			_tva; \
			})
#define to_exec_va(va)	({ \
			virtual_addr_t _tva = (va); \
			if (load_start <= _tva && _tva < load_end) { \
				_tva = _tva - load_start + exec_start; \
			} \
			_tva; \
			})

#define SECTION_START(SECTION)	_ ## SECTION ## _start
#define SECTION_END(SECTION)	_ ## SECTION ## _end

#define SECTION_ADDR_START(SECTION)	(virtual_addr_t)&SECTION_START(SECTION)
#define SECTION_ADDR_END(SECTION)	(virtual_addr_t)&SECTION_END(SECTION)

#define DECLARE_SECTION(SECTION)					\
	extern virtual_addr_t SECTION_START(SECTION);			\
	extern virtual_addr_t SECTION_END(SECTION)

DECLARE_SECTION(text);
DECLARE_SECTION(cpuinit);
DECLARE_SECTION(spinlock);
DECLARE_SECTION(init);
DECLARE_SECTION(rodata);

#define SETUP_RO_SECTION(ENTRY, SECTION)				\
	__setup_initial_pgtbl(&(ENTRY),					\
			     to_exec_va(SECTION_ADDR_START(SECTION)),	\
			     to_exec_va(SECTION_ADDR_END(SECTION)),	\
			     to_load_pa(SECTION_ADDR_START(SECTION)),	\
			     FALSE)

void __attribute__ ((section(".entry")))
    _setup_initial_pgtbl(virtual_addr_t load_start, virtual_addr_t load_end,
			 virtual_addr_t exec_start, virtual_addr_t exec_end)
{
	u32 i;
#ifdef CONFIG_DEFTERM_EARLY_PRINT
	virtual_addr_t defterm_early_va;
#endif
	struct cpu_mmu_entry_ctrl entry = { 0, 0, NULL, NULL, 0 };

	/* For now assume 3-level page table */
#ifdef CONFIG_64BIT
	entry.num_levels = 3;
#else
	entry.num_levels = 2;
#endif

	/* Init pgtbl_base, pgtbl_tree, and next_pgtbl */
	entry.pgtbl_tree =
		(int *)to_load_pa((virtual_addr_t)&def_pgtbl_tree);
	for (i = 0; i < PGTBL_INITIAL_TABLE_COUNT; i++) {
		entry.pgtbl_tree[i] = -1;
	}
	entry.pgtbl_base = to_load_pa((virtual_addr_t)&def_pgtbl);
	entry.next_pgtbl = (cpu_pte_t *)entry.pgtbl_base;

	/* Init first pgtbl */
	for (i = 0; i < PGTBL_TABLE_ENTCNT; i++) {
		entry.next_pgtbl[i] = 0x0ULL;
	}

	entry.pgtbl_count++;
	entry.next_pgtbl += PGTBL_TABLE_ENTCNT;

#ifdef CONFIG_DEFTERM_EARLY_PRINT
	/* Map UART for early defterm
	 * Note: This is for early debug purpose
	 */
	defterm_early_va = to_exec_va((virtual_addr_t)&defterm_early_base);
	__setup_initial_pgtbl(&entry,
			      defterm_early_va,
			      defterm_early_va + PGTBL_L0_BLOCK_SIZE,
			      (virtual_addr_t)CONFIG_DEFTERM_EARLY_BASE_PA,
			      TRUE);
#endif

	/* Map physical = logical
	 * Note: This mapping is using at boot time only
	 */
	__setup_initial_pgtbl(&entry, load_start, load_end, load_start, TRUE);

	/* Map to logical addresses which are
	 * covered by read-only linker sections
	 * Note: This mapping is used at runtime
	 */
	SETUP_RO_SECTION(entry, text);
	SETUP_RO_SECTION(entry, init);
	SETUP_RO_SECTION(entry, cpuinit);
	SETUP_RO_SECTION(entry, spinlock);
	SETUP_RO_SECTION(entry, rodata);

	/* Map rest of logical addresses which are
	 * not covered by read-only linker sections
	 * Note: This mapping is used at runtime
	 */
	__setup_initial_pgtbl(&entry, exec_start, exec_end, load_start, TRUE);
}
