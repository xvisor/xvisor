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
 * @file arm_irq.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief source code for handling ARM test code interrupts
 */

#include <arm_board.h>
#include <arm_mmu.h>
#include <arm_irq.h>
#include <arm_stdio.h>

#define MAX_NR_IRQS		1024

arm_irq_handler_t irq_hndls[MAX_NR_IRQS];

#define PIC_NR_IRQS		((arm_board_pic_nr_irqs() < MAX_NR_IRQS) ? \
				  arm_board_pic_nr_irqs() : MAX_NR_IRQS)

void do_bad_mode(struct pt_regs *regs)
{
	arm_puts("Bad exception\n");
	while(1);
}

void do_sync(struct pt_regs *regs)
{
	arm_sync_abort(regs);
}

void do_irq(struct pt_regs *regs)
{
	int rc = 0;
	int irq = arm_board_pic_active_irq();

	if (-1 < irq) {
		rc = arm_board_pic_ack_irq(irq);
		if (rc) {
			while (1);
		}
		if (irq_hndls[irq]) {
			rc = irq_hndls[irq](irq, regs);
			if (rc) {
				while (1);
			}
		}
		rc = arm_board_pic_eoi_irq(irq);
		if (rc) {
			while (1);
		}
	}
}

void do_fiq(struct pt_regs *regs)
{
}

void arm_irq_setup(void)
{
	int vec;

	/*
	 * Reset irq handlers
	 */
	for (vec = 0; vec < PIC_NR_IRQS; vec++) {
		irq_hndls[vec] = NULL;
	}

	/*
	 * Initialize board PIC
	 */
	vec = arm_board_pic_init();
	if (vec) {
		while (1);
	}
}

void arm_irq_register(u32 irq, arm_irq_handler_t hndl)
{
	int rc = 0;
	if (irq < PIC_NR_IRQS) {
		irq_hndls[irq] = hndl;
		if (irq_hndls[irq]) {
			rc = arm_board_pic_unmask(irq);
			if (rc) {
				while (1);
			}
		}
	}
}

void arm_irq_enable(void)
{
	asm volatile("msr daifclr, #2":::"memory");
}

void arm_irq_disable(void)
{
	asm volatile("msr daifset, #2":::"memory");
}

void arm_irq_wfi(void)
{
	__asm ("wfi\n");
}

