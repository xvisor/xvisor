/**
 * Copyright (c) 2012 Himanshu Chauhan.
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
 * @file cpu_interrupt_handler.S
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief Low-level interrupt handling routines.
 */

#include <cpu_asm_macros.h>

.section ".text", "ax"

/*
 * Stack State at the entry of exception.
 *
 *      |               |
 *      |               |
 *      +---------------+
 *      |      SS       | +40
 *      +---------------+
 *      |     RSP       | +32
 *      +---------------+
 *      |    RFLAGS     | +24
 *      +---------------+
 *      |      CS       | +16
 *      +---------------+
 *      |     RIP       | +08
 *      +---------------+
 *      |  HW Err Code  | +00
 *      +---------------+ <--- Here and above CU saves.
 *      |     r15       |
 *      |     r14       |
 *      |     r13       |
 *      |     r12       |
 *      |     r11       |
 *      |     r10       |
 *      |     r9        |
 *      |     r8        |
 *      |     rbp       |
 *      |     rsi       |
 *      |     rdi       |
 *      |     rdx       |
 *      |     rcx       |
 *      |     rbx       |
 *      |     rax       |
 *      +---------------+
 */
IRQ_HANDLER(_irq0) /* division error */
END_IRQ_HANDLER

IRQ_HANDLER(_irq1) /* debug exception */
END_IRQ_HANDLER

IRQ_HANDLER(_irq2) /* NMI */
END_IRQ_HANDLER

.extern do_breakpoint
IRQ_HANDLER(_irq3) /* Breat point */
	movq	$3, %rdi /* first arg as int num */
	movq	%rsp, %rsi /* pt_regs */
	callq	do_breakpoint
END_IRQ_HANDLER

IRQ_HANDLER(_irq4) /* overflow */
END_IRQ_HANDLER

IRQ_HANDLER(_irq5) /* bounds */
END_IRQ_HANDLER

IRQ_HANDLER(_irq6) /* invalid opc. */
END_IRQ_HANDLER

IRQ_HANDLER(_irq7) /* dev not avail */
END_IRQ_HANDLER

IRQ_HANDLER(_irq8) /* double fault */
END_IRQ_HANDLER

IRQ_HANDLER(_irq9) /* coproc seg overrn */
END_IRQ_HANDLER

IRQ_HANDLER(_irq10) /* invalid tss */
END_IRQ_HANDLER

IRQ_HANDLER(_irq11) /* seg not present */
END_IRQ_HANDLER

IRQ_HANDLER(_irq12) /* stack seg. */
END_IRQ_HANDLER

IRQ_HANDLER(_irq13) /* GPF */
END_IRQ_HANDLER

IRQ_HANDLER(_irq14) /* page fault */
END_IRQ_HANDLER

IRQ_HANDLER(_irq16) /* coproc error */
END_IRQ_HANDLER

IRQ_HANDLER(_irq17) /* alignment check */
END_IRQ_HANDLER

IRQ_HANDLER(_irq18) /* machine check */
END_IRQ_HANDLER

IRQ_HANDLER(_irq19) /* SIMD coproc error */
END_IRQ_HANDLER

IRQ_HANDLER(_irq128) /* sys call */
END_IRQ_HANDLER

IRQ_HANDLER(_generic_handler) /* other interrupts */
END_IRQ_HANDLER