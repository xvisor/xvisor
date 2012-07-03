/**
 * Copyright (c) 2012 Jean-Christophe Dubois.
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
 * @file arm_mmu.c
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @brief source file for MMU functions
 */

#include <arm_plat.h>
#include <arm_inline_asm.h>
#include <arm_defines.h>
#include <arm_mmu.h>
#include <arm_stdio.h>
#include <arm_string.h>

static u32 __attribute__((aligned(TTBL_L1TBL_SIZE))) l1[TTBL_L1TBL_SIZE / 4];
static u32 __attribute__((aligned(TTBL_L2TBL_SIZE))) l2[TTBL_L2TBL_SIZE / 4];
static u32 l2_mapva;

static u32 test_area_pa;
static u32 test_area_size;

void arm_mmu_syscall(struct pt_regs *regs)
{
	u32 inst = 0x0;
	inst = *((u32 *)regs->pc);
	inst &= 0x00FFFFFF;
	if (inst == 0x1) {
		regs->cpsr &= ~CPSR_MODE_MASK;
		regs->cpsr |= CPSR_MODE_SUPERVISOR;
		regs->pc += 4;
	}
}

extern void _switch_to_user_mode (u32, u32, u32);

#define ARM_MMU_TEST_SWITCH_TO_USER()	_switch_to_user_mode(0x0, 0x0, 0)
#define ARM_MMU_TEST_SWITCH_TO_SUPER()	asm ("svc 0x1")

static u32 test_prefetch_abort_fs;
static u32 test_prefetch_abort_result;

void arm_mmu_prefetch_abort(struct pt_regs *regs)
{
	u32 ifsr, fs;

	ifsr = read_ifsr();
	fs = (ifsr & IFSR_FS_MASK);

	if (fs == test_prefetch_abort_fs) {
		test_prefetch_abort_result = 1;
	}

	regs->pc += 4;
}

static u32 test_data_abort_fs;
static u32 test_data_abort_far;
static u32 test_data_abort_dom;
static u32 test_data_abort_result;

void arm_mmu_data_abort(struct pt_regs *regs)
{
	u32 dfsr, dfar, fs, dom;

	dfsr = read_dfsr();
	dfar = read_dfar();
	fs = (dfsr & DFSR_FS_MASK);
#if 0
	dom = (dfsr & DFSR_DOM_MASK) >> DFSR_DOM_SHIFT;
#else
	dom = test_data_abort_dom;
#endif

	if ((fs == test_data_abort_fs) &&
	    (dfar == test_data_abort_far) &&
	    (dom == test_data_abort_dom)) {
		test_data_abort_result = 1;
	}

	regs->pc += 4;
}

static void arm_mmu_section_test_iter(u32 free_sec0, u32 free_sec1, 
				      u32 * total, u32 * pass, u32 * fail)
{
	volatile u32 * test_ptr = NULL;
	u32 tmp;

	/* 1. Unmapped Read/Write test */
	/* 1.1 */
	test_ptr = (u32 *)(free_sec0 << TTBL_L1TBL_TTE_BASE20_SHIFT);
	test_data_abort_fs = DFSR_FS_TRANS_FAULT_SECTION;
	test_data_abort_far = (u32)test_ptr;
	test_data_abort_dom = 0x0;
	test_data_abort_result = 0;
	*(test_ptr) = 0xC001BABE;
	(*total)++;
	if (test_data_abort_result) {
		(*pass)++;
	} else {
		(*fail)++;
	}
	test_data_abort_result = 0;
	tmp = *(test_ptr);
	(*total)++;
	if (test_data_abort_result) {
		(*pass)++;
	} else {
		(*fail)++;
	}

	/* 1.2 */
	test_ptr = (u32 *)((free_sec0 << 
			    TTBL_L1TBL_TTE_BASE20_SHIFT) + 
			   (TTBL_L1TBL_SECTION_PAGE_SIZE / 2) - 4);	
	test_data_abort_fs = DFSR_FS_TRANS_FAULT_SECTION;
	test_data_abort_far = (u32)test_ptr;
	test_data_abort_dom = 0x0;
	test_data_abort_result = 0;
	*(test_ptr) = 0xC001BABE;
	(*total)++;
	if (test_data_abort_result) {
		(*pass)++;
	} else {
		(*fail)++;
	}
	test_data_abort_result = 0;
	tmp = *(test_ptr);
	(*total)++;
	if (test_data_abort_result) {
		(*pass)++;
	} else {
		(*fail)++;
	}
	/* 1.3 */
	test_ptr = (u32 *)((free_sec0 << 
			    TTBL_L1TBL_TTE_BASE20_SHIFT) + 
			    TTBL_L1TBL_SECTION_PAGE_SIZE - 4);
	test_data_abort_fs = DFSR_FS_TRANS_FAULT_SECTION;
	test_data_abort_far = (u32)test_ptr;
	test_data_abort_dom = 0x0;
	test_data_abort_result = 0;
	*(test_ptr) = 0xC001BABE;
	(*total)++;
	if (test_data_abort_result) {
		(*pass)++;
	} else {
		(*fail)++;
	}
	test_data_abort_result = 0;
	tmp = *(test_ptr);
	(*total)++;
	if (test_data_abort_result) {
		(*pass)++;
	} else {
		(*fail)++;
	}

	/* 2. Mapped Read/Write test */
	tmp = 0x0;
	tmp |= (TTBL_L1TBL_TTE_DOM_CHECKAP << TTBL_L1TBL_TTE_DOM_SHIFT);
	tmp |= (TTBL_AP_SRW_URW << TTBL_L1TBL_TTE_AP_SHIFT);
	tmp |= TTBL_L1TBL_TTE_REQ_MASK;
	tmp |= TTBL_L1TBL_TTE_C_MASK;
	tmp |= TTBL_L1TBL_TTE_TYPE_SECTION;
	l1[free_sec0] = tmp | (test_area_pa);
	l1[free_sec1] = tmp | (test_area_pa);
	test_ptr = (u32 *)(free_sec0 << TTBL_L1TBL_TTE_BASE20_SHIFT);
	test_ptr[0] = 0xC001BABE;
	test_ptr = (u32 *)((free_sec0 << 
			    TTBL_L1TBL_TTE_BASE20_SHIFT) + 
			   (TTBL_L1TBL_SECTION_PAGE_SIZE / 2) - 8);
	test_ptr[0] = 0xC001BABE;
	test_ptr = (u32 *)((free_sec0 << 
			    TTBL_L1TBL_TTE_BASE20_SHIFT) + 
			    TTBL_L1TBL_SECTION_PAGE_SIZE - 8);
	test_ptr[0] = 0xC001BABE;
	test_ptr = (u32 *)(free_sec1 << TTBL_L1TBL_TTE_BASE20_SHIFT);
	test_ptr[1] = 0xD00DFEED;
	test_ptr = (u32 *)((free_sec1 << 
			    TTBL_L1TBL_TTE_BASE20_SHIFT) + 
			   (TTBL_L1TBL_SECTION_PAGE_SIZE / 2) - 8);
	test_ptr[1] = 0xD00DFEED;
	test_ptr = (u32 *)((free_sec1 << 
			    TTBL_L1TBL_TTE_BASE20_SHIFT) + 
			    TTBL_L1TBL_SECTION_PAGE_SIZE - 8);
	test_ptr[1] = 0xD00DFEED;
	test_ptr = (u32 *)(free_sec0 << TTBL_L1TBL_TTE_BASE20_SHIFT);
	(*total)++;
	if (test_ptr[0] == 0xC001BABE && test_ptr[1] == 0xD00DFEED) {
		(*pass)++;
	} else {
		(*fail)++;
	}
	test_ptr = (u32 *)((free_sec0 << 
			    TTBL_L1TBL_TTE_BASE20_SHIFT) + 
			   (TTBL_L1TBL_SECTION_PAGE_SIZE / 2) - 8);
	(*total)++;
	if (test_ptr[0] == 0xC001BABE && test_ptr[1] == 0xD00DFEED) {
		(*pass)++;
	} else {
		(*fail)++;
	}
	test_ptr = (u32 *)((free_sec0 << 
			    TTBL_L1TBL_TTE_BASE20_SHIFT) + 
			    TTBL_L1TBL_SECTION_PAGE_SIZE - 8);
	(*total)++;
	if (test_ptr[0] == 0xC001BABE && test_ptr[1] == 0xD00DFEED) {
		(*pass)++;
	} else {
		(*fail)++;
	}
	test_ptr = (u32 *)(free_sec1 << TTBL_L1TBL_TTE_BASE20_SHIFT);
	(*total)++;
	if (test_ptr[0] == 0xC001BABE && test_ptr[1] == 0xD00DFEED) {
		(*pass)++;
	} else {
		(*fail)++;
	}
	test_ptr = (u32 *)((free_sec1 << 
			    TTBL_L1TBL_TTE_BASE20_SHIFT) + 
			   (TTBL_L1TBL_SECTION_PAGE_SIZE / 2) - 8);
	(*total)++;
	if (test_ptr[0] == 0xC001BABE && test_ptr[1] == 0xD00DFEED) {
		(*pass)++;
	} else {
		(*fail)++;
	}
	test_ptr = (u32 *)((free_sec1 << 
			    TTBL_L1TBL_TTE_BASE20_SHIFT) + 
			    TTBL_L1TBL_SECTION_PAGE_SIZE - 8);
	(*total)++;
	if (test_ptr[0] == 0xC001BABE && test_ptr[1] == 0xD00DFEED) {
		(*pass)++;
	} else {
		(*fail)++;
	}
	l1[free_sec0] = 0x0;
	l1[free_sec1] = 0x0;
	invalid_tlb();

	/* 3. Domain Access test */
	/* 3.1 */
	tmp = 0x0;
	tmp |= (TTBL_L1TBL_TTE_DOM_CHECKAP << TTBL_L1TBL_TTE_DOM_SHIFT);
	tmp |= (TTBL_AP_S_U << TTBL_L1TBL_TTE_AP_SHIFT);
	tmp |= TTBL_L1TBL_TTE_C_MASK;
	tmp |= TTBL_L1TBL_TTE_REQ_MASK;
	tmp |= TTBL_L1TBL_TTE_TYPE_SECTION;
	l1[free_sec0] = tmp | (test_area_pa);
	test_ptr = (u32 *)(free_sec0 << TTBL_L1TBL_TTE_BASE20_SHIFT);
	test_data_abort_fs = DFSR_FS_PERM_FAULT_SECTION;
	test_data_abort_far = (u32)test_ptr;
	test_data_abort_dom = TTBL_L1TBL_TTE_DOM_CHECKAP;
	test_data_abort_result = 0;
	*(test_ptr) = 0xC001BABE;
	(*total)++;
	if (test_data_abort_result) {
		(*pass)++;
	} else {
		(*fail)++;
	}
	test_data_abort_result = 0;
	tmp = *(test_ptr);
	(*total)++;
	if (test_data_abort_result) {
		(*pass)++;
	} else {
		(*fail)++;
	}
	l1[free_sec0] = 0x0;
	invalid_tlb();
	/* 3.2 */
	tmp = 0x0;
	tmp |= (TTBL_L1TBL_TTE_DOM_BYPASSAP << TTBL_L1TBL_TTE_DOM_SHIFT);
	tmp |= (TTBL_AP_S_U << TTBL_L1TBL_TTE_AP_SHIFT);
	tmp |= TTBL_L1TBL_TTE_C_MASK;
	tmp |= TTBL_L1TBL_TTE_TYPE_SECTION;
	l1[free_sec0] = tmp | (test_area_pa);
	test_ptr = (u32 *)(free_sec0 << TTBL_L1TBL_TTE_BASE20_SHIFT);
	test_ptr[0] = 0xD00DFEED;
	test_ptr[1] = 0xC001BABE;
	test_ptr = (u32 *)((free_sec0 << 
			    TTBL_L1TBL_TTE_BASE20_SHIFT) + 
			    TTBL_L1TBL_SECTION_PAGE_SIZE - 8);
	test_ptr[0] = 0xC001BABE;
	test_ptr[1] = 0xD00DFEED;
	test_ptr = (u32 *)(free_sec0 << TTBL_L1TBL_TTE_BASE20_SHIFT);
	(*total)++;
	if (test_ptr[1] == 0xC001BABE && test_ptr[0] == 0xD00DFEED) {
		(*pass)++;
	} else {
		(*fail)++;
	}
	test_ptr = (u32 *)((free_sec0 << 
			    TTBL_L1TBL_TTE_BASE20_SHIFT) + 
			    TTBL_L1TBL_SECTION_PAGE_SIZE - 8);
	(*total)++;
	if (test_ptr[0] == 0xC001BABE && test_ptr[1] == 0xD00DFEED) {
		(*pass)++;
	} else {
		(*fail)++;
	}
	l1[free_sec0] = 0x0;
	invalid_tlb();
	/* 3.3 */
	tmp = 0x0;
	tmp |= (TTBL_L1TBL_TTE_DOM_NOACCESS << TTBL_L1TBL_TTE_DOM_SHIFT);
	tmp |= (TTBL_AP_SRW_URW << TTBL_L1TBL_TTE_AP_SHIFT);
	tmp |= TTBL_L1TBL_TTE_C_MASK;
	tmp |= TTBL_L1TBL_TTE_REQ_MASK;
	tmp |= TTBL_L1TBL_TTE_TYPE_SECTION;
	l1[free_sec0] = tmp | (test_area_pa);
	test_ptr = (u32 *)(free_sec0 << TTBL_L1TBL_TTE_BASE20_SHIFT);
	test_data_abort_fs = DFSR_FS_DOMAIN_FAULT_SECTION;
	test_data_abort_far = (u32)test_ptr;
	test_data_abort_dom = TTBL_L1TBL_TTE_DOM_NOACCESS;
	test_data_abort_result = 0;
	*(test_ptr) = 0xC001BABE;
	(*total)++;
	if (test_data_abort_result) {
		(*pass)++;
	} else {
		(*fail)++;
	}
	test_data_abort_result = 0;
	tmp = *(test_ptr);
	(*total)++;
	if (test_data_abort_result) {
		(*pass)++;
	} else {
		(*fail)++;
	}
	test_ptr = (u32 *)((free_sec0 << 
			    TTBL_L1TBL_TTE_BASE20_SHIFT) + 
			   (TTBL_L1TBL_SECTION_PAGE_SIZE / 2) - 8);
	test_data_abort_far = (u32)test_ptr;
	test_data_abort_result = 0;
	*(test_ptr) = 0xC001BABE;
	(*total)++;
	if (test_data_abort_result) {
		(*pass)++;
	} else {
		(*fail)++;
	}
	test_data_abort_result = 0;
	tmp = *(test_ptr);
	(*total)++;
	if (test_data_abort_result) {
		(*pass)++;
	} else {
		(*fail)++;
	}
	test_ptr = (u32 *)((free_sec0 << 
			    TTBL_L1TBL_TTE_BASE20_SHIFT) + 
			    TTBL_L1TBL_SECTION_PAGE_SIZE - 8);
	test_data_abort_far = (u32)test_ptr;
	test_data_abort_result = 0;
	*(test_ptr) = 0xC001BABE;
	(*total)++;
	if (test_data_abort_result) {
		(*pass)++;
	} else {
		(*fail)++;
	}
	test_data_abort_result = 0;
	tmp = *(test_ptr);
	(*total)++;
	if (test_data_abort_result) {
		(*pass)++;
	} else {
		(*fail)++;
	}
	l1[free_sec0] = 0x0;
	invalid_tlb();

	/* 4. Permission Access test */
	/* 4.1 TTBL_AP_S_U */
	tmp = 0x0;
	tmp |= (TTBL_L1TBL_TTE_DOM_CHECKAP << TTBL_L1TBL_TTE_DOM_SHIFT);
	tmp |= (TTBL_AP_S_U << TTBL_L1TBL_TTE_AP_SHIFT);
	tmp |= TTBL_L1TBL_TTE_C_MASK;
	tmp |= TTBL_L1TBL_TTE_REQ_MASK;
	tmp |= TTBL_L1TBL_TTE_TYPE_SECTION;
	l1[free_sec0] = tmp | (test_area_pa);
	test_ptr = (u32 *)(free_sec0 << TTBL_L1TBL_TTE_BASE20_SHIFT);
	test_data_abort_fs = DFSR_FS_PERM_FAULT_SECTION;
	test_data_abort_far = (u32)test_ptr;
	test_data_abort_dom = TTBL_L1TBL_TTE_DOM_CHECKAP;
	test_data_abort_result = 0;
	*(test_ptr) = 0xC001BABE;
	(*total)++;
	if (test_data_abort_result) {
		(*pass)++;
	} else {
		(*fail)++;
	}
	test_data_abort_result = 0;
	tmp = *(test_ptr);
	(*total)++;
	if (test_data_abort_result) {
		(*pass)++;
	} else {
		(*fail)++;
	}

	ARM_MMU_TEST_SWITCH_TO_USER();

	test_data_abort_result = 0;
	*(test_ptr) = 0xC001BABE;
	(*total)++;
	if (test_data_abort_result) {
		(*pass)++;
	} else {
		(*fail)++;
	}
	test_data_abort_result = 0;
	tmp = *(test_ptr);
	(*total)++;
	if (test_data_abort_result) {
		(*pass)++;
	} else {
		(*fail)++;
	}
	ARM_MMU_TEST_SWITCH_TO_SUPER();
	l1[free_sec0] = 0x0;
	invalid_tlb();

	/* 4.2 TTBL_AP_SRW_U */
	tmp = 0x0;
	tmp |= (TTBL_L1TBL_TTE_DOM_CHECKAP << TTBL_L1TBL_TTE_DOM_SHIFT);
	tmp |= (TTBL_AP_SRW_U << TTBL_L1TBL_TTE_AP_SHIFT);
	tmp |= TTBL_L1TBL_TTE_C_MASK;
	tmp |= TTBL_L1TBL_TTE_REQ_MASK;
	tmp |= TTBL_L1TBL_TTE_TYPE_SECTION;
	l1[free_sec0] = tmp | (test_area_pa);
	test_ptr = (u32 *)(free_sec0 << TTBL_L1TBL_TTE_BASE20_SHIFT);
	test_ptr[0] = 0xC001BABE;
	test_ptr[1] = 0xD00DFEED;
	(*total)++;
	if (test_ptr[0] == 0xC001BABE && test_ptr[1] == 0xD00DFEED) {
		(*pass)++;
	} else {
		(*fail)++;
	}
	test_ptr[0] = 0x0;
	test_ptr[1] = 0x0;
	(*total)++;
	if (test_ptr[0] == 0x0 && test_ptr[1] == 0x0) {
		(*pass)++;
	} else {
		(*fail)++;
	}
	ARM_MMU_TEST_SWITCH_TO_USER();
	test_data_abort_fs = DFSR_FS_PERM_FAULT_SECTION;
	test_data_abort_far = (u32)test_ptr;
	test_data_abort_dom = TTBL_L1TBL_TTE_DOM_CHECKAP;
	test_data_abort_result = 0;
	*(test_ptr) = 0xC001BABE;
	(*total)++;
	if (test_data_abort_result) {
		(*pass)++;
	} else {
		(*fail)++;
	}
	test_data_abort_result = 0;
	tmp = *(test_ptr);
	(*total)++;
	if (test_data_abort_result) {
		(*pass)++;
	} else {
		(*fail)++;
	}
	ARM_MMU_TEST_SWITCH_TO_SUPER();
	l1[free_sec0] = 0x0;
	invalid_tlb();

	/* 4.3 TTBL_AP_SRW_UR */
	tmp = 0x0;
	tmp |= (TTBL_L1TBL_TTE_DOM_CHECKAP << TTBL_L1TBL_TTE_DOM_SHIFT);
	tmp |= (TTBL_AP_SRW_UR << TTBL_L1TBL_TTE_AP_SHIFT);
	tmp |= TTBL_L1TBL_TTE_C_MASK;
	tmp |= TTBL_L1TBL_TTE_REQ_MASK;
	tmp |= TTBL_L1TBL_TTE_TYPE_SECTION;
	l1[free_sec0] = tmp | (test_area_pa);
	test_ptr = (u32 *)(free_sec0 << TTBL_L1TBL_TTE_BASE20_SHIFT);
	test_ptr[0] = 0x0;
	test_ptr[1] = 0x0;

	(*total)++;

	if (test_ptr[0] == 0x0 && test_ptr[1] == 0x0) {
		(*pass)++;
	} else {
		(*fail)++;
	}
	test_ptr[0] = 0xC001BABE;
	test_ptr[1] = 0xD00DFEED;
	(*total)++;
	if (test_ptr[0] == 0xC001BABE && test_ptr[1] == 0xD00DFEED) {
		(*pass)++;
	} else {
		(*fail)++;
	}

	ARM_MMU_TEST_SWITCH_TO_USER();
	test_data_abort_fs = DFSR_FS_PERM_FAULT_SECTION;
	test_data_abort_far = (u32)test_ptr;
	test_data_abort_dom = TTBL_L1TBL_TTE_DOM_CHECKAP;
	test_data_abort_result = 0;
	test_ptr[0] = 0x0;

	(*total)++;
	if (test_data_abort_result) {
		(*pass)++;
	} else {
		(*fail)++;
	}
	(*total)++;
	if (test_ptr[0] == 0xC001BABE) {
		(*pass)++;
	} else {
		(*fail)++;
	}

	ARM_MMU_TEST_SWITCH_TO_SUPER();

	l1[free_sec0] = 0x0;
	invalid_tlb();

	/* 4.4 TTBL_AP_SRW_URW */

	tmp = 0x0;
	tmp |= (TTBL_L1TBL_TTE_DOM_CHECKAP << TTBL_L1TBL_TTE_DOM_SHIFT);
	tmp |= (TTBL_AP_SRW_URW << TTBL_L1TBL_TTE_AP_SHIFT);
	tmp |= TTBL_L1TBL_TTE_C_MASK;
	tmp |= TTBL_L1TBL_TTE_REQ_MASK;
	tmp |= TTBL_L1TBL_TTE_TYPE_SECTION;
	l1[free_sec0] = tmp | (test_area_pa);
	test_ptr = (u32 *)(free_sec0 << TTBL_L1TBL_TTE_BASE20_SHIFT);
	test_ptr[0] = 0xC001BABE;
	test_ptr[1] = 0xD00DFEED;
	(*total)++;
	if (test_ptr[0] == 0xC001BABE && test_ptr[1] == 0xD00DFEED) {
		(*pass)++;
	} else {
		(*fail)++;
	}
	test_ptr[0] = 0x0;
	test_ptr[1] = 0x0;
	(*total)++;
	if (test_ptr[0] == 0x0 && test_ptr[1] == 0x0) {
		(*pass)++;
	} else {
		(*fail)++;
	}
	ARM_MMU_TEST_SWITCH_TO_USER();
	test_ptr[0] = 0xC001BABE;
	test_ptr[1] = 0xD00DFEED;
	(*total)++;
	if (test_ptr[0] == 0xC001BABE && test_ptr[1] == 0xD00DFEED) {
		(*pass)++;
	} else {
		(*fail)++;
	}
	test_ptr[0] = 0x0;
	test_ptr[1] = 0x0;
	(*total)++;
	if (test_ptr[0] == 0x0 && test_ptr[1] == 0x0) {
		(*pass)++;
	} else {
		(*fail)++;
	}
	ARM_MMU_TEST_SWITCH_TO_SUPER();
	l1[free_sec0] = 0x0;
	invalid_tlb();

	return;
}

static void arm_mmu_page_test_iter(u32 free_page0, u32 free_page1, 
				   u32 * total, u32 * pass, u32 * fail)
{
	volatile u32 * test_ptr = NULL;
	u32 tmp, tmp1;

	/* 1. Unmapped Read/Write test */
	/* 1.1 */
	test_ptr = (u32 *)(free_page0 << TTBL_L2TBL_TTE_BASE12_SHIFT);
	test_ptr = (u32 *)((u32)test_ptr + l2_mapva);
	test_data_abort_fs = DFSR_FS_TRANS_FAULT_PAGE;
	test_data_abort_far = (u32)test_ptr;
	test_data_abort_dom = 0x0;
	test_data_abort_result = 0;
	*(test_ptr) = 0xC001BABE;
	(*total)++;
	if (test_data_abort_result) {
		(*pass)++;
	} else {
		(*fail)++;
	}
	test_data_abort_result = 0;
	tmp = *(test_ptr);
	(*total)++;
	if (test_data_abort_result) {
		(*pass)++;
	} else {
		(*fail)++;
	}
	/* 1.2 */
	test_ptr = (u32 *)((free_page0 << 
			    TTBL_L2TBL_TTE_BASE12_SHIFT) + 
			   (TTBL_L2TBL_SMALL_PAGE_SIZE / 2) - 4);	
	test_ptr = (u32 *)((u32)test_ptr + l2_mapva);
	test_data_abort_fs = DFSR_FS_TRANS_FAULT_PAGE;
	test_data_abort_far = (u32)test_ptr;
	test_data_abort_dom = 0x0;
	test_data_abort_result = 0;
	*(test_ptr) = 0xC001BABE;
	(*total)++;
	if (test_data_abort_result) {
		(*pass)++;
	} else {
		(*fail)++;
	}
	test_data_abort_result = 0;
	tmp = *(test_ptr);
	(*total)++;
	if (test_data_abort_result) {
		(*pass)++;
	} else {
		(*fail)++;
	}
	/* 1.3 */
	test_ptr = (u32 *)((free_page0 << 
			    TTBL_L2TBL_TTE_BASE12_SHIFT) + 
			    TTBL_L2TBL_SMALL_PAGE_SIZE - 4);
	test_ptr = (u32 *)((u32)test_ptr + l2_mapva);
	test_data_abort_fs = DFSR_FS_TRANS_FAULT_PAGE;
	test_data_abort_far = (u32)test_ptr;
	test_data_abort_dom = 0x0;
	test_data_abort_result = 0;
	*(test_ptr) = 0xC001BABE;
	(*total)++;
	if (test_data_abort_result) {
		(*pass)++;
	} else {
		(*fail)++;
	}
	test_data_abort_result = 0;
	tmp = *(test_ptr);
	(*total)++;
	if (test_data_abort_result) {
		(*pass)++;
	} else {
		(*fail)++;
	}

	/* 2. Mapped Read/Write test */
	tmp = 0x0;
	tmp |= (TTBL_AP_SRW_URW << TTBL_L2TBL_TTE_AP00_SHIFT);
	tmp |= (TTBL_AP_SRW_URW << TTBL_L2TBL_TTE_AP01_SHIFT);
	tmp |= (TTBL_AP_SRW_URW << TTBL_L2TBL_TTE_AP02_SHIFT);
	tmp |= (TTBL_AP_SRW_URW << TTBL_L2TBL_TTE_AP03_SHIFT);
	tmp |= TTBL_L2TBL_TTE_C_MASK;
	tmp |= TTBL_L2TBL_TTE_TYPE_SMALL_X;
	l2[free_page0] = tmp | (test_area_pa);
	l2[free_page1] = tmp | (test_area_pa);
	test_ptr = (u32 *)(free_page0 << TTBL_L2TBL_TTE_BASE12_SHIFT);
	test_ptr = (u32 *)((u32)test_ptr + l2_mapva);
	test_ptr[0] = 0xC001BABE;
	test_ptr = (u32 *)((free_page0 << 
			    TTBL_L2TBL_TTE_BASE12_SHIFT) + 
			   (TTBL_L2TBL_SMALL_PAGE_SIZE / 2) - 8);
	test_ptr = (u32 *)((u32)test_ptr + l2_mapva);
	test_ptr[0] = 0xC001BABE;
	test_ptr = (u32 *)((free_page0 << 
			    TTBL_L2TBL_TTE_BASE12_SHIFT) + 
			    TTBL_L2TBL_SMALL_PAGE_SIZE - 8);
	test_ptr = (u32 *)((u32)test_ptr + l2_mapva);
	test_ptr[0] = 0xC001BABE;
	test_ptr = (u32 *)(free_page1 << TTBL_L2TBL_TTE_BASE12_SHIFT);
	test_ptr = (u32 *)((u32)test_ptr + l2_mapva);
	test_ptr[1] = 0xD00DFEED;
	test_ptr = (u32 *)((free_page1 << 
			    TTBL_L2TBL_TTE_BASE12_SHIFT) + 
			   (TTBL_L2TBL_SMALL_PAGE_SIZE / 2) - 8);
	test_ptr = (u32 *)((u32)test_ptr + l2_mapva);
	test_ptr[1] = 0xD00DFEED;
	test_ptr = (u32 *)((free_page1 << 
			    TTBL_L2TBL_TTE_BASE12_SHIFT) + 
			    TTBL_L2TBL_SMALL_PAGE_SIZE - 8);
	test_ptr = (u32 *)((u32)test_ptr + l2_mapva);
	test_ptr[1] = 0xD00DFEED;
	test_ptr = (u32 *)(free_page0 << TTBL_L2TBL_TTE_BASE12_SHIFT);
	test_ptr = (u32 *)((u32)test_ptr + l2_mapva);
	(*total)++;
	if (test_ptr[0] == 0xC001BABE && test_ptr[1] == 0xD00DFEED) {
		(*pass)++;
	} else {
		(*fail)++;
	}
	test_ptr = (u32 *)((free_page0 << 
			    TTBL_L2TBL_TTE_BASE12_SHIFT) + 
			   (TTBL_L2TBL_SMALL_PAGE_SIZE / 2) - 8);
	test_ptr = (u32 *)((u32)test_ptr + l2_mapva);
	(*total)++;
	if (test_ptr[0] == 0xC001BABE && test_ptr[1] == 0xD00DFEED) {
		(*pass)++;
	} else {
		(*fail)++;
	}
	test_ptr = (u32 *)((free_page0 << 
			    TTBL_L2TBL_TTE_BASE12_SHIFT) + 
			    TTBL_L2TBL_SMALL_PAGE_SIZE - 8);
	test_ptr = (u32 *)((u32)test_ptr + l2_mapva);
	(*total)++;
	if (test_ptr[0] == 0xC001BABE && test_ptr[1] == 0xD00DFEED) {
		(*pass)++;
	} else {
		(*fail)++;
	}
	test_ptr = (u32 *)(free_page1 << TTBL_L2TBL_TTE_BASE12_SHIFT);
	test_ptr = (u32 *)((u32)test_ptr + l2_mapva);
	(*total)++;
	if (test_ptr[0] == 0xC001BABE && test_ptr[1] == 0xD00DFEED) {
		(*pass)++;
	} else {
		(*fail)++;
	}
	test_ptr = (u32 *)((free_page1 << 
			    TTBL_L2TBL_TTE_BASE12_SHIFT) + 
			   (TTBL_L2TBL_SMALL_PAGE_SIZE / 2) - 8);
	test_ptr = (u32 *)((u32)test_ptr + l2_mapva);
	(*total)++;
	if (test_ptr[0] == 0xC001BABE && test_ptr[1] == 0xD00DFEED) {
		(*pass)++;
	} else {
		(*fail)++;
	}
	test_ptr = (u32 *)((free_page1 << 
			    TTBL_L2TBL_TTE_BASE12_SHIFT) + 
			    TTBL_L2TBL_SMALL_PAGE_SIZE - 8);
	test_ptr = (u32 *)((u32)test_ptr + l2_mapva);
	(*total)++;
	if (test_ptr[0] == 0xC001BABE && test_ptr[1] == 0xD00DFEED) {
		(*pass)++;
	} else {
		(*fail)++;
	}
	l2[free_page0] = 0x0;
	l2[free_page1] = 0x0;
	invalid_tlb();

	/* 3. Domain Access test */
	tmp1 = l1[l2_mapva >> TTBL_L1TBL_TTE_BASE20_SHIFT];
	/* 3.1 */
	tmp = l1[l2_mapva >> TTBL_L1TBL_TTE_BASE20_SHIFT];
	tmp &= ~TTBL_L1TBL_TTE_DOM_MASK;
	tmp |= (TTBL_L1TBL_TTE_DOM_CHECKAP << TTBL_L1TBL_TTE_DOM_SHIFT);
	tmp |= TTBL_L1TBL_TTE_REQ_MASK;
	l1[l2_mapva >> TTBL_L1TBL_TTE_BASE20_SHIFT] = tmp;
	tmp = 0x0;
	tmp |= (TTBL_AP_S_U << TTBL_L2TBL_TTE_AP00_SHIFT);
	tmp |= (TTBL_AP_S_U << TTBL_L2TBL_TTE_AP01_SHIFT);
	tmp |= (TTBL_AP_S_U << TTBL_L2TBL_TTE_AP02_SHIFT);
	tmp |= (TTBL_AP_S_U << TTBL_L2TBL_TTE_AP03_SHIFT);
	tmp |= TTBL_L2TBL_TTE_C_MASK;
	tmp |= TTBL_L2TBL_TTE_TYPE_SMALL_X;
	l2[free_page0] = tmp | (test_area_pa);
	test_ptr = (u32 *)(free_page0 << TTBL_L2TBL_TTE_BASE12_SHIFT);
	test_ptr = (u32 *)((u32)test_ptr + l2_mapva);
	test_data_abort_fs = DFSR_FS_PERM_FAULT_PAGE;
	test_data_abort_far = (u32)test_ptr;
	test_data_abort_dom = TTBL_L1TBL_TTE_DOM_CHECKAP;
	test_data_abort_result = 0;
	*(test_ptr) = 0xC001BABE;
	(*total)++;
	if (test_data_abort_result) {
		(*pass)++;
	} else {
		(*fail)++;
	}
	test_data_abort_result = 0;
	tmp = *(test_ptr);
	(*total)++;
	if (test_data_abort_result) {
		(*pass)++;
	} else {
		(*fail)++;
	}
	l1[l2_mapva >> TTBL_L1TBL_TTE_BASE20_SHIFT] = tmp1;
	l2[free_page0] = 0x0;
	invalid_tlb();
	/* 3.2 */
	tmp = l1[l2_mapva >> TTBL_L1TBL_TTE_BASE20_SHIFT];
	tmp &= ~TTBL_L1TBL_TTE_DOM_MASK;
	tmp |= (TTBL_L1TBL_TTE_DOM_BYPASSAP << TTBL_L1TBL_TTE_DOM_SHIFT);
	tmp |= TTBL_L1TBL_TTE_REQ_MASK;
	l1[l2_mapva >> TTBL_L1TBL_TTE_BASE20_SHIFT] = tmp;
	tmp = 0x0;
	tmp |= (TTBL_AP_S_U << TTBL_L2TBL_TTE_AP00_SHIFT);
	tmp |= (TTBL_AP_S_U << TTBL_L2TBL_TTE_AP01_SHIFT);
	tmp |= (TTBL_AP_S_U << TTBL_L2TBL_TTE_AP02_SHIFT);
	tmp |= (TTBL_AP_S_U << TTBL_L2TBL_TTE_AP03_SHIFT);
	tmp |= TTBL_L2TBL_TTE_C_MASK;
	tmp |= TTBL_L2TBL_TTE_TYPE_SMALL_X;
	l2[free_page0] = tmp | (test_area_pa);
	test_ptr = (u32 *)(free_page0 << TTBL_L2TBL_TTE_BASE12_SHIFT);
	test_ptr = (u32 *)((u32)test_ptr + l2_mapva);
	test_ptr[0] = 0xC001BABE;
	test_ptr[1] = 0xD00DFEED;
	test_ptr = (u32 *)((free_page0 << 
			    TTBL_L2TBL_TTE_BASE12_SHIFT) + 
			    TTBL_L2TBL_SMALL_PAGE_SIZE - 8);
	test_ptr = (u32 *)((u32)test_ptr + l2_mapva);
	test_ptr[0] = 0xC001BABE;
	test_ptr[1] = 0xD00DFEED;
	test_ptr = (u32 *)(free_page0 << TTBL_L2TBL_TTE_BASE12_SHIFT);
	test_ptr = (u32 *)((u32)test_ptr + l2_mapva);
	(*total)++;
	if (test_ptr[0] == 0xC001BABE && test_ptr[1] == 0xD00DFEED) {
		(*pass)++;
	} else {
		(*fail)++;
	}
	test_ptr = (u32 *)((free_page0 << 
			    TTBL_L2TBL_TTE_BASE12_SHIFT) + 
			    TTBL_L2TBL_SMALL_PAGE_SIZE - 8);
	test_ptr = (u32 *)((u32)test_ptr + l2_mapva);
	(*total)++;
	if (test_ptr[0] == 0xC001BABE && test_ptr[1] == 0xD00DFEED) {
		(*pass)++;
	} else {
		(*fail)++;
	}
	l1[l2_mapva >> TTBL_L1TBL_TTE_BASE20_SHIFT] = tmp1;
	l2[free_page0] = 0x0;
	invalid_tlb();
	/* 3.3 */
	tmp = l1[l2_mapva >> TTBL_L1TBL_TTE_BASE20_SHIFT];
	tmp &= ~TTBL_L1TBL_TTE_DOM_MASK;
	tmp |= (TTBL_L1TBL_TTE_DOM_NOACCESS << TTBL_L1TBL_TTE_DOM_SHIFT);
	tmp |= TTBL_L1TBL_TTE_REQ_MASK;
	l1[l2_mapva >> TTBL_L1TBL_TTE_BASE20_SHIFT] = tmp;
	tmp = 0x0;
	tmp |= (TTBL_AP_SRW_URW << TTBL_L2TBL_TTE_AP00_SHIFT);
	tmp |= (TTBL_AP_SRW_URW << TTBL_L2TBL_TTE_AP01_SHIFT);
	tmp |= (TTBL_AP_SRW_URW << TTBL_L2TBL_TTE_AP02_SHIFT);
	tmp |= (TTBL_AP_SRW_URW << TTBL_L2TBL_TTE_AP03_SHIFT);
	tmp |= TTBL_L2TBL_TTE_C_MASK;
	tmp |= TTBL_L2TBL_TTE_TYPE_SMALL_X;
	l2[free_page0] = tmp | (test_area_pa);
	test_ptr = (u32 *)(free_page0 << TTBL_L2TBL_TTE_BASE12_SHIFT);
	test_ptr = (u32 *)((u32)test_ptr + l2_mapva);
	test_data_abort_fs = DFSR_FS_DOMAIN_FAULT_PAGE;
	test_data_abort_far = (u32)test_ptr;
	test_data_abort_dom = TTBL_L1TBL_TTE_DOM_NOACCESS;
	test_data_abort_result = 0;
	*(test_ptr) = 0xC001BABE;
	(*total)++;
	if (test_data_abort_result) {
		(*pass)++;
	} else {
		(*fail)++;
	}
	test_data_abort_result = 0;
	tmp = *(test_ptr);
	(*total)++;
	if (test_data_abort_result) {
		(*pass)++;
	} else {
		(*fail)++;
	}
	test_ptr = (u32 *)((free_page0 << 
			    TTBL_L2TBL_TTE_BASE12_SHIFT) + 
			   (TTBL_L2TBL_SMALL_PAGE_SIZE / 2) - 8);
	test_ptr = (u32 *)((u32)test_ptr + l2_mapva);
	test_data_abort_far = (u32)test_ptr;
	test_data_abort_result = 0;
	*(test_ptr) = 0xC001BABE;
	(*total)++;
	if (test_data_abort_result) {
		(*pass)++;
	} else {
		(*fail)++;
	}
	test_data_abort_result = 0;
	tmp = *(test_ptr);
	(*total)++;
	if (test_data_abort_result) {
		(*pass)++;
	} else {
		(*fail)++;
	}
	test_ptr = (u32 *)((free_page0 << 
			    TTBL_L2TBL_TTE_BASE12_SHIFT) + 
			    TTBL_L2TBL_SMALL_PAGE_SIZE - 8);
	test_ptr = (u32 *)((u32)test_ptr + l2_mapva);
	test_data_abort_far = (u32)test_ptr;
	test_data_abort_result = 0;
	*(test_ptr) = 0xC001BABE;
	(*total)++;
	if (test_data_abort_result) {
		(*pass)++;
	} else {
		(*fail)++;
	}
	test_data_abort_result = 0;
	tmp = *(test_ptr);
	(*total)++;
	if (test_data_abort_result) {
		(*pass)++;
	} else {
		(*fail)++;
	}
	l1[l2_mapva >> TTBL_L1TBL_TTE_BASE20_SHIFT] = tmp1;
	l2[free_page0] = 0x0;
	invalid_tlb();

	/* 4. Permission Access test */
	/* 4.1 TTBL_AP_S_U */
	tmp = 0x0;
	tmp |= (TTBL_AP_S_U << TTBL_L2TBL_TTE_AP00_SHIFT);
	tmp |= (TTBL_AP_S_U << TTBL_L2TBL_TTE_AP01_SHIFT);
	tmp |= (TTBL_AP_S_U << TTBL_L2TBL_TTE_AP02_SHIFT);
	tmp |= (TTBL_AP_S_U << TTBL_L2TBL_TTE_AP03_SHIFT);
	tmp |= TTBL_L2TBL_TTE_C_MASK;
	tmp |= TTBL_L2TBL_TTE_TYPE_SMALL_X;
	l2[free_page0] = tmp | (test_area_pa);
	test_ptr = (u32 *)(free_page0 << TTBL_L2TBL_TTE_BASE12_SHIFT);
	test_ptr = (u32 *)((u32)test_ptr + l2_mapva);
	test_data_abort_fs = DFSR_FS_PERM_FAULT_PAGE;
	test_data_abort_far = (u32)test_ptr;
	test_data_abort_dom = TTBL_L1TBL_TTE_DOM_CHECKAP;
	test_data_abort_result = 0;
	*(test_ptr) = 0xC001BABE;
	(*total)++;
	if (test_data_abort_result) {
		(*pass)++;
	} else {
		(*fail)++;
	}
	test_data_abort_result = 0;
	tmp = *(test_ptr);
	(*total)++;
	if (test_data_abort_result) {
		(*pass)++;
	} else {
		(*fail)++;
	}
	ARM_MMU_TEST_SWITCH_TO_USER();
	test_data_abort_result = 0;
	*(test_ptr) = 0xC001BABE;
	(*total)++;
	if (test_data_abort_result) {
		(*pass)++;
	} else {
		(*fail)++;
	}
	test_data_abort_result = 0;
	tmp = *(test_ptr);
	(*total)++;
	if (test_data_abort_result) {
		(*pass)++;
	} else {
		(*fail)++;
	}
	ARM_MMU_TEST_SWITCH_TO_SUPER();
	l2[free_page0] = 0x0;
	invalid_tlb();
	/* 4.2 TTBL_AP_SRW_U */
	tmp = 0x0;
	tmp |= (TTBL_AP_SRW_U << TTBL_L2TBL_TTE_AP00_SHIFT);
	tmp |= (TTBL_AP_SRW_U << TTBL_L2TBL_TTE_AP01_SHIFT);
	tmp |= (TTBL_AP_SRW_U << TTBL_L2TBL_TTE_AP02_SHIFT);
	tmp |= (TTBL_AP_SRW_U << TTBL_L2TBL_TTE_AP03_SHIFT);
	tmp |= TTBL_L2TBL_TTE_C_MASK;
	tmp |= TTBL_L2TBL_TTE_TYPE_SMALL_X;
	l2[free_page0] = tmp | (test_area_pa);
	test_ptr = (u32 *)(free_page0 << TTBL_L2TBL_TTE_BASE12_SHIFT);
	test_ptr = (u32 *)((u32)test_ptr + l2_mapva);
	test_ptr[0] = 0xC001BABE;
	test_ptr[1] = 0xD00DFEED;
	(*total)++;
	if (test_ptr[0] == 0xC001BABE && test_ptr[1] == 0xD00DFEED) {
		(*pass)++;
	} else {
		(*fail)++;
	}
	test_ptr[0] = 0x0;
	test_ptr[1] = 0x0;
	(*total)++;
	if (test_ptr[0] == 0x0 && test_ptr[1] == 0x0) {
		(*pass)++;
	} else {
		(*fail)++;
	}
	ARM_MMU_TEST_SWITCH_TO_USER();
	test_data_abort_fs = DFSR_FS_PERM_FAULT_PAGE;
	test_data_abort_far = (u32)test_ptr;
	test_data_abort_dom = TTBL_L1TBL_TTE_DOM_CHECKAP;
	test_data_abort_result = 0;
	*(test_ptr) = 0xC001BABE;
	(*total)++;
	if (test_data_abort_result) {
		(*pass)++;
	} else {
		(*fail)++;
	}
	test_data_abort_result = 0;
	tmp = *(test_ptr);
	(*total)++;
	if (test_data_abort_result) {
		(*pass)++;
	} else {
		(*fail)++;
	}
	ARM_MMU_TEST_SWITCH_TO_SUPER();
	l2[free_page0] = 0x0;
	invalid_tlb();

	/* 4.3 TTBL_AP_SRW_UR */
	tmp = 0x0;
	tmp |= (TTBL_AP_SRW_UR << TTBL_L2TBL_TTE_AP00_SHIFT);
	tmp |= (TTBL_AP_SRW_UR << TTBL_L2TBL_TTE_AP01_SHIFT);
	tmp |= (TTBL_AP_SRW_UR << TTBL_L2TBL_TTE_AP02_SHIFT);
	tmp |= (TTBL_AP_SRW_UR << TTBL_L2TBL_TTE_AP03_SHIFT);
	tmp |= TTBL_L2TBL_TTE_C_MASK;
	tmp |= TTBL_L2TBL_TTE_TYPE_SMALL_X;
	l2[free_page0] = tmp | (test_area_pa);
	test_ptr = (u32 *)(free_page0 << TTBL_L2TBL_TTE_BASE12_SHIFT);
	test_ptr = (u32 *)((u32)test_ptr + l2_mapva);
	test_ptr[0] = 0x0;
	test_ptr[1] = 0x0;
	(*total)++;
	if (test_ptr[0] == 0x0 && test_ptr[1] == 0x0) {
		(*pass)++;
	} else {
		(*fail)++;
	}
	test_ptr[0] = 0xC001BABE;
	test_ptr[1] = 0xD00DFEED;
	(*total)++;
	if (test_ptr[0] == 0xC001BABE && test_ptr[1] == 0xD00DFEED) {
		(*pass)++;
	} else {
		(*fail)++;
	}
	ARM_MMU_TEST_SWITCH_TO_USER();
	test_data_abort_fs = DFSR_FS_PERM_FAULT_PAGE;
	test_data_abort_far = (u32)test_ptr;
	test_data_abort_dom = TTBL_L1TBL_TTE_DOM_CHECKAP;
	test_data_abort_result = 0;
	test_ptr[0] = 0x0;
	(*total)++;
	if (test_data_abort_result) {
		(*pass)++;
	} else {
		(*fail)++;
	}
	(*total)++;
	if (test_ptr[0] == 0xC001BABE) {
		(*pass)++;
	} else {
		(*fail)++;
	}
	ARM_MMU_TEST_SWITCH_TO_SUPER();
	l2[free_page0] = 0x0;
	invalid_tlb();
	/* 4.4 TTBL_AP_SRW_URW */
	tmp = 0x0;
	tmp |= (TTBL_AP_SRW_URW << TTBL_L2TBL_TTE_AP00_SHIFT);
	tmp |= (TTBL_AP_SRW_URW << TTBL_L2TBL_TTE_AP01_SHIFT);
	tmp |= (TTBL_AP_SRW_URW << TTBL_L2TBL_TTE_AP02_SHIFT);
	tmp |= (TTBL_AP_SRW_URW << TTBL_L2TBL_TTE_AP03_SHIFT);
	tmp |= TTBL_L2TBL_TTE_C_MASK;
	tmp |= TTBL_L2TBL_TTE_TYPE_SMALL_X;
	l2[free_page0] = tmp | (test_area_pa);
	test_ptr = (u32 *)(free_page0 << TTBL_L2TBL_TTE_BASE12_SHIFT);
	test_ptr = (u32 *)((u32)test_ptr + l2_mapva);
	test_ptr[0] = 0xC001BABE;
	test_ptr[1] = 0xD00DFEED;
	(*total)++;
	if (test_ptr[0] == 0xC001BABE && test_ptr[1] == 0xD00DFEED) {
		(*pass)++;
	} else {
		(*fail)++;
	}
	test_ptr[0] = 0x0;
	test_ptr[1] = 0x0;
	(*total)++;
	if (test_ptr[0] == 0x0 && test_ptr[1] == 0x0) {
		(*pass)++;
	} else {
		(*fail)++;
	}
	ARM_MMU_TEST_SWITCH_TO_USER();
	test_ptr[0] = 0xC001BABE;
	test_ptr[1] = 0xD00DFEED;
	(*total)++;
	if (test_ptr[0] == 0xC001BABE && test_ptr[1] == 0xD00DFEED) {
		(*pass)++;
	} else {
		(*fail)++;
	}
	test_ptr[0] = 0x0;
	test_ptr[1] = 0x0;
	(*total)++;
	if (test_ptr[0] == 0x0 && test_ptr[1] == 0x0) {
		(*pass)++;
	} else {
		(*fail)++;
	}
	ARM_MMU_TEST_SWITCH_TO_SUPER();
	l2[free_page0] = 0x0;
	invalid_tlb();

	return;
}

#define TEST_SECTION_COUNT		10

void arm_mmu_section_test(u32 * total, u32 * pass, u32 * fail)
{
	int setup_required = 0;
	u32 ite, pos, free_sec[TEST_SECTION_COUNT];
	u32 sctlr = read_sctlr();

	if (!(sctlr & SCTLR_M_MASK)) {
		setup_required = 1;
	}

	if (setup_required) {
		arm_mmu_setup();
	}

	/* Initialize statistics */
	*total = 0x0;
	*pass = 0x0;
	*fail = 0x0;

	/* Prepare list of free sections */
	pos = 0;
	for (ite = 0; ite < (TTBL_L1TBL_SIZE / 4); ite++) {
		if ((l1[ite] & TTBL_L1TBL_TTE_TYPE_MASK) == 
						TTBL_L1TBL_TTE_TYPE_FAULT) {
			free_sec[pos] = ite;
			pos++;
		}
		if (pos == TEST_SECTION_COUNT) {
			break;
		}
	}

	/* Run a fixed set of test for all free sections */
	for (ite = 0; ite < TEST_SECTION_COUNT; ite++) {
		arm_mmu_section_test_iter(free_sec[ite], 
					  free_sec[(ite + 1) % TEST_SECTION_COUNT],
					  total, pass, fail);
	}

	if (setup_required) {
		arm_mmu_cleanup();
	}

	return;
}

#define TEST_PAGE_COUNT		10

void arm_mmu_page_test(u32 * total, u32 * pass, u32 * fail)
{
	int setup_required = 0;
	u32 ite, pos, free_page[TEST_PAGE_COUNT];
	u32 sctlr = read_sctlr();

	if (!(sctlr & SCTLR_M_MASK)) {
		setup_required = 1;
	}

	if (setup_required) {
		arm_mmu_setup();
	}

	/* Initialize statistics */
	*total = 0x0;
	*pass = 0x0;
	*fail = 0x0;

	/* Prepare list of free sections */
	pos = 0;
	for (ite = 0; ite < (TTBL_L2TBL_SIZE / 4); ite++) {
		if ((l2[ite] & TTBL_L2TBL_TTE_TYPE_MASK) == 
						TTBL_L2TBL_TTE_TYPE_FAULT) {
			free_page[pos] = ite;
			pos++;
		}
		if (pos == TEST_PAGE_COUNT) {
			break;
		}
	}

	/* Run a fixed set of test for all free sections */
	for (ite = 0; ite < TEST_PAGE_COUNT; ite++) {
		arm_mmu_page_test_iter(free_page[ite], 
					free_page[(ite + 1) % TEST_PAGE_COUNT],
					total, pass, fail);
	}

	if (setup_required) {
		arm_mmu_cleanup();
	}

	return;
}

bool arm_mmu_is_enabled(void)
{
	u32 sctlr = read_sctlr();

	if (sctlr & SCTLR_M_MASK) {
		return TRUE;
	}

	return FALSE;
}

extern u8 _code_start;
extern u8 _code_end;

void arm_mmu_setup(void)
{
	u32 sec, sec_tmpl, sec_start, sec_end;

	u32 sctlr = read_sctlr();

	/* If MMU already enabled then return */
	if (sctlr & SCTLR_M_MASK) {
		return;
	}

	/* Reset memory for L2 */
	for (sec = 0; sec < (TTBL_L2TBL_SIZE / 4); sec++) {
		l2[sec] = 0x0;
	}

	/* Reset memory for L1 */
	for (sec = 0; sec < (TTBL_L1TBL_SIZE / 4); sec++) {
		l1[sec] = 0x0;
	}

	/* Section entry template for code */
	sec_tmpl = TTBL_L1TBL_TTE_REQ_MASK;
	sec_tmpl |= (TTBL_L1TBL_TTE_DOM_CHECKAP << TTBL_L1TBL_TTE_DOM_SHIFT);
	sec_tmpl |= (TTBL_AP_SRW_URW << TTBL_L1TBL_TTE_AP_SHIFT);
	sec_tmpl |= TTBL_L1TBL_TTE_C_MASK;
	sec_tmpl |= TTBL_L1TBL_TTE_TYPE_SECTION;

	/* compute section start and end */
	sec_start = ((u32)&_code_start) & ~(TTBL_L1TBL_SECTION_PAGE_SIZE - 1);
	sec_end = ((u32)&_code_end) & ~(TTBL_L1TBL_SECTION_PAGE_SIZE - 1);

	/* Creation section entries for exception vectors */
	if (sec_start > 0x0) {
		l1[0] = sec_tmpl | 0x0;
	}

	/* Create section entries for code */
	for (sec = sec_start; sec <= sec_end; sec += TTBL_L1TBL_SECTION_PAGE_SIZE) {
		l1[sec / TTBL_L1TBL_SECTION_PAGE_SIZE] = sec_tmpl | sec;
	}
	sec_end += TTBL_L1TBL_SECTION_PAGE_SIZE;

	/* Map an additional section after code */
	l1[sec_end / TTBL_L1TBL_SECTION_PAGE_SIZE] = sec_tmpl | sec_end;
	sec_end += TTBL_L1TBL_SECTION_PAGE_SIZE;

	/* Section entry template for I/O */
	sec_tmpl &= ~TTBL_L1TBL_TTE_C_MASK;

	/* Create section entries for Sysctl, GIC, Timer, PL01x, and Flash */
	sec = VERSATILE_SYS_BASE;
	l1[sec / TTBL_L1TBL_SECTION_PAGE_SIZE] = sec_tmpl | sec;

	sec = VERSATILE_VIC_BASE;
	l1[sec / TTBL_L1TBL_SECTION_PAGE_SIZE] = sec_tmpl | sec;

	for (sec = VERSATILE_FLASH_BASE; 
	     sec <= (VERSATILE_FLASH_BASE + 4 * TTBL_L1TBL_SECTION_PAGE_SIZE); 
	     sec += TTBL_L1TBL_SECTION_PAGE_SIZE) {
		l1[sec / TTBL_L1TBL_SECTION_PAGE_SIZE] = sec_tmpl | sec;
	}

	/* Map an l2 table after (code + additional section) */
	sec_tmpl = TTBL_L1TBL_TTE_REQ_MASK;
	sec_tmpl |= TTBL_L1TBL_TTE_TYPE_L2TBL;
	l2_mapva = sec_end;
	l1[l2_mapva / TTBL_L1TBL_SECTION_PAGE_SIZE] = sec_tmpl | (u32)(&l2);

	/* Setup test area in physical RAM */
	test_area_pa = sec_end;
	test_area_size = TTBL_L1TBL_SECTION_PAGE_SIZE;

	/* Write DACR */
	sec = 0x0;
	sec |= (TTBL_DOM_CLIENT << (2 * TTBL_L1TBL_TTE_DOM_CHECKAP));
	sec |= (TTBL_DOM_MANAGER << (2 * TTBL_L1TBL_TTE_DOM_BYPASSAP));
	sec |= (TTBL_DOM_NOACCESS << (2 * TTBL_L1TBL_TTE_DOM_NOACCESS));
	write_dacr(sec);

	/* Write TTBR0 */
	write_ttbr0((u32)&l1);

	/* Enable MMU */
	sctlr |= SCTLR_M_MASK;
	write_sctlr(sctlr);

	return;
}

void arm_mmu_cleanup(void)
{
	u32 sctlr = read_sctlr();

	/* If MMU already disabled then return */
	if (!(sctlr & SCTLR_M_MASK)) {
		return;
	}

	/* Disable MMU */
	sctlr &= ~SCTLR_M_MASK;
	write_sctlr(sctlr);

	return;
}

