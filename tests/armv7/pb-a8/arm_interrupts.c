/**
 * Copyright (c) 2010 Anup Patel.
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
 * @file arm_interrupts.c
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief source code for handling ARM test code interrupts
 */

#include <arm_types.h>
#include <arm_regs.h>
#include <arm_interrupts.h>

void do_undefined_instruction(pt_regs_t *regs)
{
}

void do_software_interrupt(pt_regs_t *regs)
{
}

void do_prefetch_abort(pt_regs_t *regs)
{
}

void do_data_abort(pt_regs_t *regs)
{
}

void do_not_used(pt_regs_t *regs)
{
}

void do_irq(pt_regs_t *uregs)
{
}

void do_fiq(pt_regs_t *uregs)
{
}

void arm_irq_setup(void)
{
	extern u32 _start_vect[];
	u32 *vectors = (u32 *)NULL;
	u32 *vectors_data = vectors + CPU_IRQ_NR;
	int vec;
 
	/*
	 * Loop through the vectors we're taking over, and copy the
	 * vector's insn and data word.
	 */
	for (vec = 0; vec < CPU_IRQ_NR; vec++) {
		vectors[vec] = _start_vect[vec];
		vectors_data[vec] = _start_vect[vec+CPU_IRQ_NR];
	}

	/*
	 * Check if verctors are set properly
	 */
	for (vec = 0; vec < CPU_IRQ_NR; vec++) {
		if ((vectors[vec] != _start_vect[vec]) ||
		    (vectors_data[vec] != _start_vect[vec+CPU_IRQ_NR])) {
			/* Hang */
			while(1);
		}
	}
}

void arm_irq_enable(void)
{
	__asm( "cpsie if" );
}

void arm_irq_disable(void)
{
	__asm( "cpsid if" );
}

