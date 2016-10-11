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
 * @file arm_mmu_v7.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief source file for MMU functions
 */

#include <arm_board.h>
#include <arm_inline_asm.h>
#include <arm_defines.h>
#include <arm_mmu.h>
#include <arm_stdio.h>

#if 0
u32 __attribute__((aligned(TTBL_L1TBL_SIZE))) l1[TTBL_L1TBL_SIZE / 4];
u32 __attribute__((aligned(TTBL_L2TBL_SIZE))) l2[TTBL_L2TBL_SIZE / 4];
u32 l2_mapva;
#endif

u32 test_area_pa;
u32 test_area_size;

extern void _switch_to_user_mode (u32, u32);

#define ARM_MMU_TEST_SWITCH_TO_USER()	_switch_to_user_mode(0x0, 0x0)
#define ARM_MMU_TEST_SWITCH_TO_SUPER()	asm volatile("svc 0x1":::"memory", "cc")

u64 test_data_abort_fsc;
u64 test_data_abort_far;
u64 test_data_abort_wnr;
u64 test_data_abort_dom;
u32 test_data_abort_result;

void arm_sync_abort(struct pt_regs *regs)
{
	u64 esr, far, ec, iss, wnr;
	u64 fsc;

	esr = mrs(esr_el1);
	far = mrs(far_el1);

	ec = (esr & ESR_EC_MASK) >> ESR_EC_SHIFT;
	iss = (esr & ESR_ISS_MASK) >> ESR_ISS_SHIFT;

	switch (ec) {
		case EC_TRAP_SVC_A64:
			if((iss & 0xffff) == 1) {
				/* Request to switch to supervisor mode */
				regs->pstate &= ~PSR_MODE_MASK;
				regs->pstate |= PSR_MODE_EL1t;
				regs->pc += 4;
				return;
			}
			break;
		case EC_TRAP_LWREL_DATA_ABORT:
			fsc = (iss & ISS_ABORT_FSC_MASK) >> ISS_ABORT_FSC_SHIFT;
			wnr = (iss & ISS_ABORT_WNR_MASK) >> ISS_ABORT_WNR_SHIFT;
			if ((fsc == test_data_abort_fsc) &&
			    (far == test_data_abort_far) &&
			    (wnr == test_data_abort_wnr)) {
				test_data_abort_result = 1;
				regs->pc += 4;
				return;
			}
			break;
	}
	arm_printf("Bad synchronous exception @ PC: 0x%lX\n",
		   (virtual_addr_t)regs->pc);
	arm_printf("ESR: 0x%08X (EC:0x%X, ISS:0x%X)\n",
		   (u32)esr, (u32)ec, (u32)iss);
	arm_printf("LR: 0x%lX, FAR: 0x%lX, PSTATE: 0x%X\n",
		   (virtual_addr_t)regs->lr, (virtual_addr_t)far,
		   (u32)regs->pstate);
	arm_printf("  General Purpose Registers");
	{ 
		int ite;
		for (ite = 0; ite < 30; ite++) {
			if (ite % 2 == 0)
				arm_printf("\n");
			arm_printf("    X%02d=0x%016lx  ",
				   ite, (unsigned long)regs->gpr[ite]);
		}
	}
	arm_printf("\n");
	while(1);
}

bool arm_mmu_is_enabled(void)
{
	u32 sctlr = mrs(sctlr_el1);

	if (sctlr & SCTLR_M_MASK) {
		return TRUE;
	}

	return FALSE;
}

extern u8 _code_start;
extern u8 _code_end;

void arm_mmu_setup(void)
{
	u64 *l1;
	u64 val, sec_tmpl = 0x0;
	u32 sctlr = mrs(sctlr_el1);

	/* If MMU already enabled then return */
	if (sctlr & SCTLR_M_MASK) {
		return;
	}

	/* We place the L1 page table at a page-aligned address after _code_end */
	l1 = (u64 *)(((u64)&_code_end + TTBL_TABLE_SIZE) & ~(TTBL_TABLE_SIZE - 1));

	/* Reset memory for L1 */
	for (val = 0; val < (TTBL_TABLE_ENTCNT); val++) {
		l1[val] = 0x0;
	}

	/* SuperSection entry template for code */
	sec_tmpl = (TTBL_STAGE1_LOWER_AF_MASK | TTBL_VALID_MASK);
	sec_tmpl |= (TTBL_AP_SRW_U << TTBL_STAGE1_LOWER_AP_SHIFT);
	sec_tmpl |= (1 << 11);

	/* Create 1GB super-section entry for Device Region from 0x00_0000_0000 */
	val = ((AINDEX_SO << TTBL_STAGE1_LOWER_AINDEX_SHIFT) & TTBL_STAGE1_LOWER_AINDEX_MASK);
	val |= (0x0 << TTBL_STAGE1_LOWER_SH_SHIFT) | 0x00000000;	/* Non-shareable */
	l1[0x00000000/(1 << 30)] = (sec_tmpl | val);

	/* Create 1GB super-section entry for RAM from 0x00_8000_0000 */
	val = ((AINDEX_NORMAL_WB << TTBL_STAGE1_LOWER_AINDEX_SHIFT) & TTBL_STAGE1_LOWER_AINDEX_MASK);
	val |= (0x3 << TTBL_STAGE1_LOWER_SH_SHIFT) | 0x80000000;	/* Inner-shareable */
	l1[0x80000000/(1 << 30)] = (sec_tmpl | val);

	/* Setup Translation Control Register */
	val = 0;
	/* Upto 39bits of VA will allow us to continue with 3 levels of stage-1 page-tables
	 * so we program TCR_EL1[T0SZ] = 0x19 */
	val &= (~TCR_T0SZ_MASK);
	val |= (0x19 << TCR_T0SZ_SHIFT);
	val &= (~TCR_PS_MASK);
	val |= (0x2 << TCR_PS_SHIFT); /* PASize: 0x2 => PA[39:0] */
	val &= (~TCR_SH0_MASK);
	val |= (0x3 << TCR_SH0_SHIFT); /* Shareability: 0x3 - inner-shareable */
	val &= (~TCR_ORGN0_MASK);
	val |= (0x1 << TCR_ORGN0_SHIFT); /* Cacheability: 0x1 - write-back  */
	val &= (~TCR_IRGN0_MASK);
	val |= (0x1 << TCR_IRGN0_SHIFT); /* Cacheability: 0x1 - write-back  */
	msr(tcr_el1, val);

	/* Initialize MAIR */ 
	msr(mair_el1, MAIR_INITVAL);

	/* Write TTBR0 */
	msr(ttbr0_el1, (u64)l1);

	/* Enable MMU */
	sctlr |= SCTLR_I_MASK | SCTLR_C_MASK | SCTLR_M_MASK | SCTLR_AFE_MASK;
	msr(sctlr_el1, sctlr);

	asm("dsb sy\n\tisb\n\t":::"memory", "cc");

	return;
}

void data_cache_flush_all(void)
{
	register u32 val, set, way, line, i, j;
	register int slog, wlog, llog, loc;
	int clevel = 0, clidr;

	dsb();
	isb();
	clidr = mrs(CLIDR_EL1);
	loc = ((clidr >> 24) & 0x7);

	while(clevel < loc) {
		/* Select the cache-level in CCSELR_EL1 */
		val = (clevel << 1);
		msr(CSSELR_EL1, val);
		isb();
		/* Read the Cache Sized ID Register */
		val = mrs(CCSIDR_EL1);
		set = ((val & 0x0FFFE000) >> 13) + 1;
		way = ((val & 0x00001FF8) >> 3) + 1;
		line = (1 << (((val & 0x7) >> 0) + 2)) * sizeof(unsigned int);

#define log2n(n, val)	{ asm("clz %0, %1":"=r"(val):"r"(n)); val = (63 - val); }

		log2n(set, slog);
		log2n(way, wlog);
		log2n(line, llog);

		for (i=0; i<set; i++) {
			for (j=0; j<way; j++) {
				val = (j << (32-wlog)) | (i << llog) | (clevel << 1);
				asm volatile("dc cisw, %0"::"r"(val):"memory","cc");
			}
		}

		dsb();
		isb();
		clevel++;
	}
}

void arm_mmu_cleanup(void)
{
	u32 sctlr = mrs(sctlr_el1);

	/* If MMU already disabled then return */
	if (!(sctlr & SCTLR_M_MASK)) {
		return;
	}

	data_cache_flush_all();
	/* Disable MMU */
	sctlr &= ~SCTLR_M_MASK;
	msr_sync(sctlr_el1, sctlr);

	return;
}

