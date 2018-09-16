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
 * @file arch_mmu.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief source file for MMU functions
 */

#include <arm_inline_asm.h>
#include <arm_defines.h>
#include <arch_board.h>
#include <basic_stdio.h>

void arm_sync_abort(struct pt_regs *regs)
{
	int ite;
	u64 esr, far, ec, iss;

	esr = mrs(esr_el1);
	far = mrs(far_el1);

	ec = (esr & ESR_EC_MASK) >> ESR_EC_SHIFT;
	iss = (esr & ESR_ISS_MASK) >> ESR_ISS_SHIFT;

	basic_printf("Bad synchronous exception @ PC: 0x%lX\n",
		   (virtual_addr_t)regs->pc);
	basic_printf("ESR: 0x%08X (EC:0x%X, ISS:0x%X)\n",
		   (u32)esr, (u32)ec, (u32)iss);
	basic_printf("LR: 0x%lX, FAR: 0x%lX, PSTATE: 0x%X\n",
		   (virtual_addr_t)regs->lr, (virtual_addr_t)far,
		   (u32)regs->pstate);
	basic_printf("  General Purpose Registers");
	for (ite = 0; ite < 30; ite++) {
		if (ite % 2 == 0)
			basic_printf("\n");
		basic_printf("    X%02d=0x%016lx  ",
			   ite, (unsigned long)regs->gpr[ite]);
	}
	basic_printf("\n");
	while(1);
}

void arch_mmu_section_test(u32 * total, u32 * pass, u32 * fail)
{
	/* TODO: For now nothing to do here. */
	*total = 0;
	*pass = 0;
	*fail = 0;
	return;
}

void arch_mmu_page_test(u32 * total, u32 * pass, u32 * fail)
{
	/* TODO: For now nothing to do here. */
	*total = 0;
	*pass = 0;
	*fail = 0;
	return;
}

bool arch_mmu_is_enabled(void)
{
	/* TODO: For now nothing to do here. */
	return FALSE;
}

void arch_mmu_setup(void)
{
	/* TODO: For now nothing to do here. */
	return;
}

void arch_mmu_cleanup(void)
{
	/* TODO: For now nothing to do here. */
	return;
}
