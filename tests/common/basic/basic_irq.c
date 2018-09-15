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
 * @file basic_irq.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief source code for common interrupt handling
 */

#include <arch_board.h>
#include <arch_irq.h>
#include <basic_irq.h>

#define MAX_NR_IRQS		1024

irq_handler_t irq_hndls[MAX_NR_IRQS];

#define PIC_NR_IRQS		((arch_board_pic_nr_irqs() < MAX_NR_IRQS) ? \
				  arch_board_pic_nr_irqs() : MAX_NR_IRQS)

int basic_irq_exec_handler(struct pt_regs *uregs)
{
	int rc = 0;
	int irq = arch_board_pic_active_irq();

	if (-1 < irq) {
		rc = arch_board_pic_ack_irq(irq);
		if (rc) {
			return rc;
		}
		if (irq_hndls[irq]) {
			rc = irq_hndls[irq](irq, uregs);
			if (rc) {
				return rc;
			}
		}
		rc = arch_board_pic_eoi_irq(irq);
		if (rc) {
			return rc;
		}
	}

	return rc;
}

void basic_irq_setup(void)
{
	int vec;

	/*
	 * Arch specific irq setup
	 */
	arch_irq_setup();

	/*
	 * Reset irq handlers
	 */
	for (vec = 0; vec < PIC_NR_IRQS; vec++) {
		irq_hndls[vec] = NULL;
	}

	/*
	 * Initialize board PIC
	 */
	vec = arch_board_pic_init();
	if (vec) {
		while (1);
	}
}

void basic_irq_register(u32 irq, irq_handler_t hndl)
{
	int rc = 0;
	if (irq < PIC_NR_IRQS) {
		irq_hndls[irq] = hndl;
		if (irq_hndls[irq]) {
			rc = arch_board_pic_unmask(irq);
			if (rc) {
				while (1);
			}
		}
	}
}

void basic_irq_enable(void)
{
	arch_irq_enable();
}

void basic_irq_disable(void)
{
	arch_irq_disable();
}

void basic_irq_wfi(void)
{
	arch_irq_wfi();
}
