/**
 * Copyright (c) 2012 Sukanto Ghosh.
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
 * @file arm_irq.c
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief source code for handling ARM test code interrupts
 *
 * Adapted from tests/arm32/pb-a8/basic/arm_irq.c
 *
 */

#include <arm_config.h>
#include <arm_gic.h>
#include <arm_mmu.h>
#include <arm_irq.h>

arm_irq_handler_t irq_hndls[NR_IRQS_CA9X4];

void do_undefined_instruction(struct pt_regs *regs)
{
}

void do_software_interrupt(struct pt_regs *regs)
{
	arm_mmu_syscall(regs);
}

void do_prefetch_abort(struct pt_regs *regs)
{
	arm_mmu_prefetch_abort(regs);
}

void do_data_abort(struct pt_regs *regs)
{
	arm_mmu_data_abort(regs);
}

void do_not_used(struct pt_regs *regs)
{
}

void do_irq(struct pt_regs *uregs)
{
	int rc = 0;
	int irq = arm_gic_active_irq(0);

	if (-1 < irq) {
		if (irq_hndls[irq]) {
			rc = irq_hndls[irq](irq, uregs);
			if (rc) {
				while (1);
			}
		}
		rc = arm_gic_ack_irq(0, irq);
		if (rc) {
			while (1);
		}
	}
}

void do_fiq(struct pt_regs *uregs)
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

	/*
	 * Reset irq handlers
	 */
	for (vec = 0; vec < NR_IRQS_CA9X4; vec++) {
		irq_hndls[vec] = NULL;
	}

	/*
	 * Initialize Generic Interrupt Controller
	 */
	vec = arm_gic_dist_init(0, A9_MPCORE_GIC_DIST, IRQ_CA9X4_GIC_START);
	if (vec) {
		while(1);
	}
	vec = arm_gic_cpu_init(0, A9_MPCORE_GIC_CPU);
	if (vec) {
		while(1);
	}
}

void arm_irq_register(u32 irq, arm_irq_handler_t hndl)
{
	int rc = 0;
	if (irq < NR_IRQS_CA9X4) {
		irq_hndls[irq] = hndl;
		if (irq_hndls[irq]) {
			rc = arm_gic_unmask(0, irq);
			if (rc) {
				while (1);
			}
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

