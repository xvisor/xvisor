/**
 * Copyright (c) 2011 Anup Patel.
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
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief source file for MMU functions
 */

#include <arm_config.h>
#include <arm_plat.h>
#include <arm_inline_asm.h>
#include <arm_regs.h>
#include <arm_mmu.h>

/* Translation table related macors & defines */
#define TTBL_MIN_SIZE					0x1000
#define TTBL_MIN_PAGE_SIZE				0x1000
#define TTBL_MAX_SIZE					0x4000
#define TTBL_MAX_PAGE_SIZE				0x1000000
#define TTBL_AP_S_U					0x0
#define TTBL_AP_SRW_U					0x1
#define TTBL_AP_SRW_UR					0x2
#define TTBL_AP_SRW_URW					0x3
#define TTBL_AP_SR_U					0x5
#define TTBL_AP_SR_UR					0x7
#define TTBL_DOM_MANAGER				0x3
#define TTBL_DOM_RESERVED				0x2
#define TTBL_DOM_CLIENT					0x1
#define TTBL_DOM_NOACCESS				0x0
#define TTBL_L1TBL_SIZE					0x4000
#define TTBL_L1TBL_SECTION_PAGE_SIZE			0x100000
#define TTBL_L1TBL_SUPSECTION_PAGE_SIZE			0x1000000
#define TTBL_L1TBL_TTE_OFFSET_MASK			0xFFF00000
#define TTBL_L1TBL_TTE_OFFSET_SHIFT			20
#define TTBL_L1TBL_TTE_BASE24_MASK			0xFF000000
#define TTBL_L1TBL_TTE_BASE24_SHIFT			24
#define TTBL_L1TBL_TTE_BASE20_MASK			0xFFF00000
#define TTBL_L1TBL_TTE_BASE20_SHIFT			20
#define TTBL_L1TBL_TTE_BASE10_MASK			0xFFFFFC00
#define TTBL_L1TBL_TTE_BASE10_SHIFT			10
#define TTBL_L1TBL_TTE_NS2_MASK				0x00080000
#define TTBL_L1TBL_TTE_NS2_SHIFT			19
#define TTBL_L1TBL_TTE_SECTYPE_MASK			0x00040000
#define TTBL_L1TBL_TTE_SECTYPE_SHIFT			18
#define TTBL_L1TBL_TTE_NG_MASK				0x00020000
#define TTBL_L1TBL_TTE_NG_SHIFT				17
#define TTBL_L1TBL_TTE_S_MASK				0x00010000
#define TTBL_L1TBL_TTE_S_SHIFT				16
#define TTBL_L1TBL_TTE_AP2_MASK				0x00008000
#define TTBL_L1TBL_TTE_AP2_SHIFT			15
#define TTBL_L1TBL_TTE_TEX_MASK				0x00007000
#define TTBL_L1TBL_TTE_TEX_SHIFT			12
#define TTBL_L1TBL_TTE_AP_MASK				0x00000C00
#define TTBL_L1TBL_TTE_AP_SHIFT				10
#define TTBL_L1TBL_TTE_IMP_MASK				0x00000200
#define TTBL_L1TBL_TTE_IMP_SHIFT			9
#define TTBL_L1TBL_TTE_DOM_MASK				0x000001E0
#define TTBL_L1TBL_TTE_DOM_SHIFT			5
#define TTBL_L1TBL_TTE_XN_MASK				0x00000010
#define TTBL_L1TBL_TTE_XN_SHIFT				4
#define TTBL_L1TBL_TTE_NS1_MASK				0x00000008
#define TTBL_L1TBL_TTE_NS1_SHIFT			3
#define TTBL_L1TBL_TTE_C_MASK				0x00000008
#define TTBL_L1TBL_TTE_C_SHIFT				3
#define TTBL_L1TBL_TTE_B_MASK				0x00000004
#define TTBL_L1TBL_TTE_B_SHIFT				2
#define TTBL_L1TBL_TTE_TYPE_MASK			0x00000003
#define TTBL_L1TBL_TTE_TYPE_SHIFT			0
#define TTBL_L1TBL_TTE_TYPE_FAULT			0x0
#define TTBL_L1TBL_TTE_TYPE_L2TBL			0x1
#define TTBL_L1TBL_TTE_TYPE_SECTION			0x2
#define TTBL_L1TBL_TTE_TYPE_RESERVED			0x3
#define TTBL_L2TBL_SIZE					0x400
#define TTBL_L2TBL_LARGE_PAGE_SIZE			0x10000
#define TTBL_L2TBL_SMALL_PAGE_SIZE			0x1000
#define TTBL_L2TBL_TTE_OFFSET_MASK			0x000FF000
#define TTBL_L2TBL_TTE_OFFSET_SHIFT			12
#define TTBL_L2TBL_TTE_BASE16_MASK			0xFFFF0000
#define TTBL_L2TBL_TTE_BASE16_SHIFT			16
#define TTBL_L2TBL_TTE_LXN_MASK				0x00008000
#define TTBL_L2TBL_TTE_LXN_SHIFT			15
#define TTBL_L2TBL_TTE_BASE12_MASK			0xFFFFF000
#define TTBL_L2TBL_TTE_BASE12_SHIFT			12
#define TTBL_L2TBL_TTE_LTEX_MASK			0x00007000
#define TTBL_L2TBL_TTE_LTEX_SHIFT			12
#define TTBL_L2TBL_TTE_NG_MASK				0x00000800
#define TTBL_L2TBL_TTE_NG_SHIFT				11
#define TTBL_L2TBL_TTE_S_MASK				0x00000400
#define TTBL_L2TBL_TTE_S_SHIFT				10
#define TTBL_L2TBL_TTE_AP2_MASK				0x00000200
#define TTBL_L2TBL_TTE_AP2_SHIFT			9
#define TTBL_L2TBL_TTE_STEX_MASK			0x000001C0
#define TTBL_L2TBL_TTE_STEX_SHIFT			6
#define TTBL_L2TBL_TTE_AP_MASK				0x00000030
#define TTBL_L2TBL_TTE_AP_SHIFT				4
#define TTBL_L2TBL_TTE_C_MASK				0x00000008
#define TTBL_L2TBL_TTE_C_SHIFT				3
#define TTBL_L2TBL_TTE_B_MASK				0x00000004
#define TTBL_L2TBL_TTE_B_SHIFT				2
#define TTBL_L2TBL_TTE_SXN_MASK				0x00000001
#define TTBL_L2TBL_TTE_SXN_SHIFT			0
#define TTBL_L2TBL_TTE_TYPE_MASK			0x00000003
#define TTBL_L2TBL_TTE_TYPE_SHIFT			0
#define TTBL_L2TBL_TTE_TYPE_FAULT			0x0
#define TTBL_L2TBL_TTE_TYPE_LARGE			0x1
#define TTBL_L2TBL_TTE_TYPE_SMALL_X			0x2
#define TTBL_L2TBL_TTE_TYPE_SMALL_XN			0x3

u32 __attribute__((aligned(TTBL_L1TBL_SIZE))) l1[TTBL_L1TBL_SIZE / 4];
u32 __attribute__((aligned(TTBL_L2TBL_SIZE))) l2[TTBL_L2TBL_SIZE / 4];
u32 l2_mapva;

u32 test_area_pa;
u32 test_area_size;

void arm_mmu_syscall(pt_regs_t *regs)
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

void arm_mmu_prefetch_abort(pt_regs_t *regs)
{
	regs->pc += 4;
}

void arm_mmu_data_abort(pt_regs_t *regs)
{
	regs->pc += 4;
}

extern void _switch_to_user_mode (u32, u32);

void arm_mmu_test(u32 * total, u32 * pass, u32 * fail)
{
	int setup_required = 0;
	u32 sctlr = read_sctlr();

	if (!(sctlr & SCTLR_M_MASK)) {
		setup_required = 1;
	}

	if (setup_required) {
		arm_mmu_setup();
	}

	/* FIXME: Do some test in supervisor mode */
	*total = 0x3;
	*pass = 0x2;
	*fail = 0x1;

	/* Switch to user mode */
	_switch_to_user_mode(0x0, 0x0);

	/* FIXME: Do some test in user mode */
	(*total) += 2;
	(*pass)++;
	(*fail)++;

	/* Switch back to supervisor mode */
	asm ("svc 0x1");

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
	u32 sec, sec_tmpl = 0x0, sec_start = 0x0, sec_end = 0x0;
	u32 sctlr = read_sctlr();

	/* If MMU already enabled then return */
	if (sctlr & SCTLR_M_MASK) {
		return;
	}

	/* Reset memory for L1 */
	for (sec = 0; sec < (TTBL_L2TBL_SIZE / 4); sec++) {
		l2[sec] = 0x0;
	}

	/* Reset memory for L2 */
	for (sec = 0; sec < (TTBL_L1TBL_SIZE / 4); sec++) {
		l1[sec] = 0x0;
	}

	/* Section entry template for code */
	sec_tmpl = 0x0;
	sec_tmpl = (TTBL_AP_SRW_URW << TTBL_L1TBL_TTE_AP_SHIFT);
	sec_tmpl |= TTBL_L1TBL_TTE_C_MASK;
	sec_tmpl |= TTBL_L1TBL_TTE_TYPE_SECTION;

	/* Create section entries for code */
	sec_start = ((u32)&_code_start) & ~(TTBL_L1TBL_SECTION_PAGE_SIZE - 1);
	sec_end = ((u32)&_code_end) & ~(TTBL_L1TBL_SECTION_PAGE_SIZE - 1);
	for (sec = sec_start; 
	     sec <= sec_end; 
	     sec += TTBL_L1TBL_SECTION_PAGE_SIZE) {
		l1[sec / TTBL_L1TBL_SECTION_PAGE_SIZE] = sec_tmpl | sec;
	}
	sec_end += TTBL_L1TBL_SECTION_PAGE_SIZE;

	/* Map an additional section after code */
	sec = sec_end;
	l1[sec / TTBL_L1TBL_SECTION_PAGE_SIZE] = sec_tmpl | sec;
	sec_end += TTBL_L1TBL_SECTION_PAGE_SIZE;

	/* Section entry template for I/O */
	sec_tmpl &= ~TTBL_L1TBL_TTE_C_MASK;
	sec_tmpl |= TTBL_L1TBL_TTE_XN_MASK;

	/* Create section entries for Sysctl, GIC, Timer, PL01x, and Flash */
	sec = REALVIEW_SYS_BASE;
	l1[sec / TTBL_L1TBL_SECTION_PAGE_SIZE] = sec_tmpl | sec;
	sec = REALVIEW_PBA8_GIC_CPU_BASE;
	l1[sec / TTBL_L1TBL_SECTION_PAGE_SIZE] = sec_tmpl | sec;
	sec = REALVIEW_PBA8_FLASH0_BASE;
	l1[sec / TTBL_L1TBL_SECTION_PAGE_SIZE] = sec_tmpl | sec;
	sec += TTBL_L1TBL_SECTION_PAGE_SIZE;
	l1[sec / TTBL_L1TBL_SECTION_PAGE_SIZE] = sec_tmpl | sec;
	sec += TTBL_L1TBL_SECTION_PAGE_SIZE;
	l1[sec / TTBL_L1TBL_SECTION_PAGE_SIZE] = sec_tmpl | sec;
	sec += TTBL_L1TBL_SECTION_PAGE_SIZE;
	l1[sec / TTBL_L1TBL_SECTION_PAGE_SIZE] = sec_tmpl | sec;

	/* Map an l2 table after (code + additional section) */
	sec_tmpl = 0x0;
	sec_tmpl |= TTBL_L1TBL_TTE_TYPE_L2TBL;
	l2_mapva = sec_end;
	l1[l2_mapva / TTBL_L1TBL_SECTION_PAGE_SIZE] = sec_tmpl | (u32)(&l2);

	/* Setup test area in physical RAM */
	test_area_pa = sec_end;
	test_area_size = 2 * TTBL_L1TBL_SECTION_PAGE_SIZE;

	/* Write DACR */
	write_dacr(TTBL_DOM_CLIENT);

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

