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
 * @file arm_irq.c
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @brief source code for handling ARM test code interrupts
 */

#include <arm_pl190.h>
#include <arm_mmu.h>
#include <arm_irq.h>
#include <arm_plat.h>

#include <arm_stdio.h>

#define NR_IRQS_VERSATILE	64

arm_irq_handler_t irq_hndls[NR_IRQS_VERSATILE];

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
	int irq;

	irq = arm_pl190_active_irq(0);

	if (irq > -1) {
		if (irq_hndls[irq]) {
			rc = irq_hndls[irq](irq, uregs);
			if (rc) {
				while (1);
			}
		}
		rc = arm_pl190_ack_irq(0, irq);
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
	for (vec = 0; vec < NR_IRQS_VERSATILE; vec++) {
		irq_hndls[vec] = NULL;
	}
 
	vec = arm_pl190_cpu_init(0, VERSATILE_VIC_BASE);
	if (vec) {
		while(1);
	}
}

void arm_irq_register(u32 irq, arm_irq_handler_t hndl)
{
	int rc = 0;

	if (irq < NR_IRQS_VERSATILE) {
		irq_hndls[irq] = hndl;
		if (irq_hndls[irq]) {
			rc = arm_pl190_unmask(0, irq);
			if (rc) {
				while (1);
			}
		}
	}
}

void arm_irq_enable(void)
{
	unsigned long temp;

	asm volatile(
		"       mrs     %0, cpsr\n"
		"       bic     %0, %0, #128\n"
		"       msr     cpsr_c, %0"
		: "=r" (temp)
		:
		: "memory", "cc");
}

void arm_irq_disable(void)
{
	unsigned long temp;

	asm volatile(
		"       mrs     %0, cpsr\n"
		"       orr     %0, %0, #128\n"
		"       msr     cpsr_c, %0"
		: "=r" (temp)
		:
		: "memory", "cc");
}

void arm_irq_wfi()
{
	unsigned long reg_r0, reg_r1, reg_r2, reg_r3, reg_ip;

	asm volatile (
			"       mov     %0, #0\n"
			"       mrc     p15, 0, %1, c1, c0, 0   @ Read control register\n"
			"       mcr     p15, 0, %0, c7, c10, 4  @ Drain write buffer\n"
			"       bic     %2, %1, #1 << 12\n"
			"       mrs     %3, cpsr		@ Disable FIQs while Icache\n"
			"       orr     %4, %3, #0x00000040     @ is disabled\n"
			"       msr     cpsr_c, %4\n"
			"       mcr     p15, 0, %2, c1, c0, 0   @ Disable I cache\n"
			"       mcr     p15, 0, %0, c7, c0, 4   @ Wait for interrupt\n"
			"       mcr     p15, 0, %1, c1, c0, 0   @ Restore ICache enable\n"
			"       msr     cpsr_c, %3	    @ Restore FIQ state"
			:"=r" (reg_r0), "=r" (reg_r1), "=r" (reg_r2), "=r" (reg_r3), "=r" (reg_ip)::"memory", "cc" );
}
