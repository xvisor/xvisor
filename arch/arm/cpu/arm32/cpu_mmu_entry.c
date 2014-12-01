/**
 * Copyright (c) 2014 Anup Patel.
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
 * @file cpu_mmu_entry.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Initial translation table setup at boot time
 */

#include <vmm_types.h>
#include <cpu_defines.h>
#include <cpu_mmu.h>

struct mmu_entry_ctrl {
	virtual_addr_t l1_base;
	virtual_addr_t l2_base;
	u32 *next_l2;
	u32 l2_count;
	int *l2_used;
	virtual_addr_t *l2_mapva;
};

extern u8 defl1_ttbl[];
extern u8 defl2_ttbl[];
extern int defl2_ttbl_used[];
extern virtual_addr_t defl2_ttbl_mapva[];
#ifdef CONFIG_DEFTERM_EARLY_PRINT
extern u8 defterm_early_base[];
#endif

void __attribute__ ((section(".entry")))
    __setup_initial_ttbl(struct mmu_entry_ctrl *entry,
			 virtual_addr_t map_start, virtual_addr_t map_end,
			 virtual_addr_t pa_start, int cacheable, int writable)
{
	u32 i;
	u32 *l1_tte, *l2_tte;
	u32 l1_tte_type, l2_tte_type;
	virtual_addr_t l2base, page_addr;

	/* align start addresses */
	map_start &= ~(TTBL_L2TBL_SMALL_PAGE_SIZE - 1);
	pa_start &= ~(TTBL_L2TBL_SMALL_PAGE_SIZE - 1);

	page_addr = map_start;

	while (page_addr < map_end) {

		/* Setup level1 table */
		l1_tte = (u32 *) (entry->l1_base +
			  ((page_addr >> TTBL_L1TBL_TTE_OFFSET_SHIFT) << 2));
		l1_tte_type = *l1_tte & TTBL_L1TBL_TTE_TYPE_MASK;
		if (l1_tte_type == TTBL_L1TBL_TTE_TYPE_COARSE_L2TBL) {
			/* Find level2 table */
			l2base = *l1_tte & TTBL_L1TBL_TTE_BASE10_MASK;
		} else {
			/* Allocate new level2 table */
			if (entry->l2_count == TTBL_INITIAL_L2TBL_COUNT) {
				while (1) ; /* No initial table available */
			}
			for (i = 0; i < (TTBL_L2TBL_SIZE/4); i++) {
				entry->next_l2[i] = 0x0;
			}
			entry->l2_used[entry->l2_count] = 1;
			entry->l2_mapva[entry->l2_count] =
					page_addr & TTBL_L1TBL_TTE_OFFSET_MASK;
			entry->l2_count++;

#if defined(CONFIG_ARMV5)
			*l1_tte = TTBL_L1TBL_TTE_REQ_MASK;
			*l1_tte |= TTBL_L1TBL_TTE_DOM_RESERVED <<
				    TTBL_L1TBL_TTE_DOM_SHIFT;
			*l1_tte |= ((virtual_addr_t)entry->next_l2) &
				    TTBL_L1TBL_TTE_BASE10_MASK;
			*l1_tte |= TTBL_L1TBL_TTE_TYPE_COARSE_L2TBL;
#else
			*l1_tte = 0x0;
			*l1_tte |= TTBL_L1TBL_TTE_DOM_RESERVED <<
				   TTBL_L1TBL_TTE_DOM_SHIFT;
			*l1_tte |= ((virtual_addr_t)entry->next_l2) &
				   TTBL_L1TBL_TTE_BASE10_MASK;
			*l1_tte |= TTBL_L1TBL_TTE_TYPE_L2TBL;
#endif

			l2base = (virtual_addr_t)entry->next_l2;
			entry->next_l2 += (TTBL_L2TBL_SIZE/4);
		}

		/* Setup level2 table */
		l2_tte = (u32 *)((page_addr & ~TTBL_L1TBL_TTE_OFFSET_MASK) >>
				  TTBL_L2TBL_TTE_OFFSET_SHIFT);
		l2_tte = (u32 *)(l2base + ((u32)l2_tte << 2));
		l2_tte_type = *l2_tte & TTBL_L2TBL_TTE_TYPE_MASK;
		if (l2_tte_type != TTBL_L2TBL_TTE_TYPE_SMALL) {
#if defined(CONFIG_ARMV5)
			*l2_tte = (((page_addr - map_start) + pa_start) &
				   TTBL_L2TBL_TTE_BASE12_MASK);
			*l2_tte |= TTBL_L2TBL_TTE_TYPE_SMALL;
			*l2_tte |=
			       (TTBL_AP_SRW_U << TTBL_L2TBL_TTE_V5_AP0_SHIFT) &
			    	TTBL_L2TBL_TTE_V5_AP0_MASK;
			*l2_tte |=
			       (TTBL_AP_SRW_U << TTBL_L2TBL_TTE_V5_AP1_SHIFT) &
				TTBL_L2TBL_TTE_V5_AP1_MASK;
			*l2_tte |=
			       (TTBL_AP_SRW_U << TTBL_L2TBL_TTE_V5_AP2_SHIFT) &
				TTBL_L2TBL_TTE_V5_AP2_MASK;
			*l2_tte |=
			       (TTBL_AP_SRW_U << TTBL_L2TBL_TTE_V5_AP3_SHIFT) &
				TTBL_L2TBL_TTE_V5_AP3_MASK;
			*l2_tte |=
			       (cacheable << TTBL_L2TBL_TTE_C_SHIFT) &
				TTBL_L2TBL_TTE_C_MASK;
			*l2_tte |=
			       (cacheable << TTBL_L2TBL_TTE_B_SHIFT) &
				TTBL_L2TBL_TTE_B_MASK;
#else
			*l2_tte = 0x0;
			*l2_tte |= (((page_addr - map_start) + pa_start) &
				    TTBL_L2TBL_TTE_BASE12_MASK);
			/*
			 * When JTAG debugging is disable, set writable page to
			 * TTBL_L2TBL_TTE_TYPE_SMALL_XN
			 */
			*l2_tte |= TTBL_L2TBL_TTE_TYPE_SMALL_X;
			*l2_tte |= (0x0 << TTBL_L2TBL_TTE_STEX_SHIFT) &
				    TTBL_L2TBL_TTE_STEX_MASK;
			*l2_tte |= (0x0 << TTBL_L2TBL_TTE_NG_SHIFT) &
				    TTBL_L2TBL_TTE_NG_MASK;
			*l2_tte |= (0x0 << TTBL_L2TBL_TTE_S_SHIFT) &
				    TTBL_L2TBL_TTE_S_MASK;
			if (writable) {
				*l2_tte |=
					(TTBL_AP_SRW_U << (TTBL_L2TBL_TTE_AP2_SHIFT - 2)) &
					TTBL_L2TBL_TTE_AP2_MASK;
				*l2_tte |= (TTBL_AP_SRW_U << TTBL_L2TBL_TTE_AP_SHIFT) &
					TTBL_L2TBL_TTE_AP_MASK;
			} else {
				*l2_tte |= (TTBL_AP_SR_U << (TTBL_L2TBL_TTE_AP2_SHIFT - 2)) &
					TTBL_L2TBL_TTE_AP2_MASK;
				*l2_tte |= (TTBL_AP_SR_U << TTBL_L2TBL_TTE_AP_SHIFT) &
					TTBL_L2TBL_TTE_AP_MASK;
			}
			*l2_tte |= (cacheable << TTBL_L2TBL_TTE_C_SHIFT) &
				    TTBL_L2TBL_TTE_C_MASK;
			*l2_tte |= (cacheable << TTBL_L2TBL_TTE_B_SHIFT) &
				    TTBL_L2TBL_TTE_B_MASK;
#endif
		}

		/* Point to next page */
		page_addr += TTBL_L2TBL_SMALL_PAGE_SIZE;
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

#define DECLARE_EXTERN_SECTION(SECTION)					\
	extern virtual_addr_t SECTION_START(SECTION);			\
	extern virtual_addr_t SECTION_END(SECTION)

DECLARE_EXTERN_SECTION(text);
DECLARE_EXTERN_SECTION(cpuinit);
DECLARE_EXTERN_SECTION(spinlock);
DECLARE_EXTERN_SECTION(init);
DECLARE_EXTERN_SECTION(initdata);
DECLARE_EXTERN_SECTION(rodata);
DECLARE_EXTERN_SECTION(data);
DECLARE_EXTERN_SECTION(percpu);
DECLARE_EXTERN_SECTION(bss);
DECLARE_EXTERN_SECTION(svc_stack);
DECLARE_EXTERN_SECTION(abt_stack);
DECLARE_EXTERN_SECTION(und_stack);
DECLARE_EXTERN_SECTION(irq_stack);
DECLARE_EXTERN_SECTION(fiq_stack);

#define SETUP_RO_SECTION(ENTRY, SECTION)				\
	__setup_initial_ttbl(&(ENTRY),					\
			     SECTION_ADDR_START(SECTION),		\
			     SECTION_ADDR_END(SECTION),			\
			     to_load_pa(SECTION_ADDR_START(SECTION)),	\
			     TRUE,					\
			     FALSE)

void __attribute__ ((section(".entry")))
    _setup_initial_ttbl(virtual_addr_t load_start, virtual_addr_t load_end,
		    virtual_addr_t exec_start, virtual_addr_t exec_end)
{
	u32 i;
#ifdef CONFIG_DEFTERM_EARLY_PRINT
	virtual_addr_t defterm_early_va;
#endif
	struct mmu_entry_ctrl entry = { 0, 0, NULL, 0, NULL };

	/* Init ttbl_used, ttbl_mapva, and related stuff */
	entry.l2_used =
		(int *)to_load_pa((virtual_addr_t)&defl2_ttbl_used);
	entry.l2_mapva = 
		(virtual_addr_t *)to_load_pa((virtual_addr_t)&defl2_ttbl_mapva);
	for (i = 0; i < TTBL_INITIAL_L2TBL_COUNT; i++) {
		entry.l2_used[i] = 0;
		entry.l2_mapva[i] = 0;
	}
	entry.l1_base = to_load_pa((virtual_addr_t)&defl1_ttbl);
	entry.l2_base = to_load_pa((virtual_addr_t)&defl2_ttbl);
	entry.next_l2 = (u32 *)entry.l2_base;

	/* Init l1 ttbl */
	for (i = 0; i < (TTBL_L1TBL_SIZE/4); i++) {
		((u32 *)entry.l1_base)[i] = 0x0;
	}

#ifdef CONFIG_DEFTERM_EARLY_PRINT
	/* Map UART for early defterm
	 * Note: This is for early debug purpose
	 */
	defterm_early_va = to_exec_va((virtual_addr_t)&defterm_early_base);
	__setup_initial_ttbl(&entry,
			     defterm_early_va,
			     defterm_early_va + TTBL_L2TBL_SMALL_PAGE_SIZE,
			     (virtual_addr_t)CONFIG_DEFTERM_EARLY_BASE_PA,
			     FALSE, TRUE);
#endif

	/* Map physical = logical
	 * Note: This mapping is using at boot time only
	 */
	__setup_initial_ttbl(&entry, load_start, load_end, load_start,
			     TRUE, TRUE);

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
	__setup_initial_ttbl(&entry, exec_start, exec_end, load_start,
			     TRUE, TRUE);
}
