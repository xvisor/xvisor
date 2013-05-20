/**
 * Copyright (c) 2012 Anup Patel.
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
 * @file cpu_entry.S
 * @author Anup Patel (anup@brainfault.org)
 * @brief entry points (booting, reset, exceptions) for ARMv7a family
 */

#include <cpu_defines.h>

	/* 
	 * _start: Primary CPU startup code
	 * _start_secondary: Secondary CPU startup code
	 *
	 * Note: Xvisor could be loaded any where in memory by boot loaders.
	 * The _start ensures that Xvisor exectues from intended
	 * base address provided at compile time.
	 */
	.section .entry, "ax", %progbits
	.globl _start
	.globl _start_secondary
_start:
	/* r4 -> load start
	 * r5 -> load end
	 * r6 -> execution start
	 * r7 -> execution end
	 * r2 -> code size
	 */
	add	r4, pc, #-0x8
	ldr	r6, __exec_start
	ldr	r7, __exec_end
	sub	r2, r7, r6
	add	r5, r4, r2

	/* Ensure that we are in hypervisor mode */
	mrs	r0, cpsr_all
	and	r0, r0, #(CPSR_MODE_MASK)
	cmp	r0, #(CPSR_MODE_HYPERVISOR)
	blne	_start_hang

	/* Save load start and load end addresses */
	ldr	r0, __load_start
	sub	r0, r0, r6
	add	r0, r0, r4
	str	r4, [r0]
	ldr	r0, __load_end
	sub	r0, r0, r6
	add	r0, r0, r4
	str	r5, [r0]
	/* Hang if execution start is not 4 KB aligned */
	mov	r0, r6
	bfc	r0, #0, #12
	cmp	r0, r6
	blt	_start_hang
	/* Hang if execution end is not 4 KB aligned */
	mov	r0, r7
	bfc	r0, #0, #12
	cmp	r0, r7
	blt	_start_hang
	/* Relocate code if load start is not 4 KB aligned */
	mov	r0, r4
	bfc	r0, #0, #12
	cmp	r0, r4
	beq	_start_mmu_init
_start_relocate:
	/* Relocate copy function at end after load end address */
	ldr	r0, __copy_start
	ldr	r1, __copy_end
	sub	r2, r1, r0
	sub	r0, r0, r6
	add	r0, r0, r4
	mov	r1, r5
	bl	_copy
	/* Use newly relocated copy function to relocate entire code */
	mov	r0, r4
	mov	r1, r5
	sub	r2, r5, r4
	mov	r1, r4
	bfc	r1, #0, #12
	bl	_start_nextpc1
_start_nextpc1:
	add	lr, lr, #16
	sub	lr, lr, r4
	add	lr, lr, r1
	bx	r5
	/* Update load start and load end
	 * r4 -> new load start
	 * r5 -> new load end
	 */
	mov	r0, r4
	bfc	r0, #0, #12
	sub	r1, r4, r0
	sub	r4, r4, r1
	sub	r5, r5, r1
	ldr	r0, __load_start
	sub	r0, r0, r6
	add	r0, r0, r4
	str	r4, [r0]
	ldr	r0, __load_end
	sub	r0, r0, r6
	add	r0, r0, r4
	str	r5, [r0]

_start_mmu_init:
	/* Setup initial TTBL using C code and a temporary stack */
	ldr	sp, __hvc_stack_end
	sub	sp, sp, r6
	add	sp, sp, r4
	mov	r0, r4
	mov	r1, r5
	mov	r2, r6
	mov	r3, r7
	bl	_setup_initial_ttbl
	ldr	r0, __httbr_set
	sub	r0, r0, r6
	add	r0, r0, r4
	str	r0, __httbr_set

	/*
	 * Secondary CPU startup code 
	 *
	 * Note: From this point primary CPU startup is same as secondary CPU
	 */
_start_secondary:
	/* Ensure that we are in hypervisor mode */
	mrs	r0, cpsr_all
	and	r0, r0, #(CPSR_MODE_MASK)
	cmp	r0, #(CPSR_MODE_HYPERVISOR)
	blne	_start_hang
	/* Setup Hypervisor MAIR0 & MAIR1 */
	ldr	r0, __hmair0_set
	mcr     p15, 4, r0, c10, c2, 0
	ldr	r0, __hmair1_set
	mcr     p15, 4, r0, c10, c2, 1
	/* Setup Hypervisor Translation Control Register */
	ldr	r1, __htcr_set
	ldr	r2, __htcr_clear
	mrc     p15, 4, r3, c2, c0, 2
	and	r3, r3, r2
	orr	r3, r3, r1
	mcr     p15, 4, r3, c2, c0, 2
	/* Setup Hypervisor Translation Base Register */
	ldr	r0, __httbr_set
	mov	r1, #0x0
	mcrr     p15, 4, r0, r1, c2
	/* Setup Hypervisor Stage2 Translation Control Register */
	ldr	r1, __vtcr_set
	ldr	r2, __vtcr_clear
	mrc     p15, 4, r3, c2, c1, 2
	and	r3, r3, r2
	orr	r3, r3, r1
	mcr     p15, 4, r3, c2, c1, 2
	/* Setup Hypervisor System Control Register */
	ldr	r1, __hsctlr_set
	ldr	r2, __hsctlr_clear
	mrc     p15, 4, r3, c1, c0, 0
	and	r3, r3, r2
	orr	r3, r3, r1
	mcr     p15, 4, r3, c1, c0, 0
	/* Jump to reset code */
	ldr	pc, __reset

_start_hang:
	b	.


__hmair0_set:
	.word (HMAIR0_INITVAL)
__hmair1_set:
	.word (HMAIR1_INITVAL)
__htcr_clear:
	.word ~(HTCR_T0SZ_MASK | HTCR_ORGN0_MASK | HTCR_IRGN0_MASK)
__htcr_set:
	.word ((0x3 << HTCR_ORGN0_SHIFT) | (0x3 << HTCR_IRGN0_SHIFT))
__httbr_set:
	.word def_ttbl
__vtcr_clear:
	.word ~(0x0)
__vtcr_set:
	.word ((0x1 << VTCR_SL0_SHIFT) | (0x3 << VTCR_ORGN0_SHIFT) | (0x3 << VTCR_IRGN0_SHIFT))
__hsctlr_clear:
	.word ~(HSCTLR_A_MASK)
__hsctlr_set:
	.word (HSCTLR_M_MASK | HSCTLR_C_MASK | HSCTLR_I_MASK)
__exec_start:
	.word _code_start
__exec_end:
	.word _code_end
__load_start:
	.word _load_start
__load_end:
	.word _load_end
__copy_start:
	.word _copy
__copy_end:
	.word _copy_end

	/* 
	 * Load start address storage
	 */
	.globl _load_start
_load_start:
	.word 0x0

	/* 
	 * Load end address storage
	 */
	.globl _load_end
_load_end:
	.word 0x0

	/* 
	 * Copy data from source to destination
	 * Arguments:
	 *  r0 -> source address
	 *  r1 -> destination address
	 *  r2 -> byte count
	 * Return:
	 *  r0 -> bytes copied
	 */
	.section .entry, "ax", %progbits
	.globl _copy
_copy:
	mov	r3, r2
_copy_loop:
	cmp	r3, #0
	beq	_copy_done
	cmp	r3, #16
	bge	_copy_chunk
_copy_word:
	ldmia	r0!, {r8}
	stmia	r1!, {r8}
	sub	r3, r3, #4
	b	_copy_loop
_copy_chunk:
	ldmia	r0!, {r8 - r11}
	stmia	r1!, {r8 - r11}
	sub	r3, r3, #16
	b	_copy_loop
_copy_done:
	mov	r0, r2
	bx	lr
_copy_end:

	/* 
	 * Exception vector start.
	 */
	.section .entry, "ax", %progbits
	.balign	256
	.globl _start_vect
_start_vect:
	ldr	pc, __reset
	ldr	pc, __undefined_instruction
	ldr	pc, __software_interrupt
	ldr	pc, __prefetch_abort
	ldr	pc, __data_abort
	ldr	pc, __hypervisor_trap
	ldr	pc, __irq
	ldr	pc, __fiq
__reset:
	.word _reset
__undefined_instruction:
	.word _undef_inst
__software_interrupt:
	.word _soft_irq
__prefetch_abort:
	.word _prefetch_abort
__data_abort:
	.word _data_abort
__hypervisor_trap:
	.word _hyp_trap
__irq:
	.word _irq
__fiq:
	.word _fiq
	.global _end_vect
_end_vect:
	b	.

	/* 
	 * Exception stacks.
	 */
__hvc_stack_end:
	.word _hvc_stack_end

	/* 
	 * Initial Hypervisor settings.
	 */
__hcr_initial_set:
	.word (HCR_AMO_MASK | \
		HCR_IMO_MASK | \
		HCR_FMO_MASK | \
		HCR_VM_MASK)
__hcr_initial_clear:
	.word ~(0x0)

	/* 
	 * Reset exception handler.
	 * Reset hardware state before starting Xvisor.
	 */
	.globl _reset
_reset:
	/* Clear lr */
	mov 	lr, #0
	/* Clear a register for temporary usage */
	mov	r0, #0
	mov	r1, #0
	mov	r2, #0
	/* Disable IRQ & FIQ */
	mrs	r0, cpsr_all
	orr	r0, r0, #(CPSR_IRQ_DISABLED | CPSR_FIQ_DISABLED)
	msr	cpsr_cxsf, r0
	/* Set Hypervisor Mode Stack */
	ldr	sp, __hvc_stack_end
#ifdef CONFIG_SMP
	mrc     p15, 0, r0, c0, c0, 5
	ands	r0, r0, #0xFF
	mov	r1, #CONFIG_IRQ_STACK_SIZE
	mul	r1, r1, r0
	sub	sp, sp, r1
#endif
	/* Initial Hypervisor Configuration */
	ldr	r1, __hcr_initial_set
	ldr	r2, __hcr_initial_clear
	mrc     p15, 4, r0, c1, c1, 0
	and	r0, r0, r2
	orr	r0, r0, r1
	mcr     p15, 4, r0, c1, c1, 0
	/* Call CPU init function */
	b	cpu_init
	/* We should never reach here */
	b	.

	/* 
	 * Helper Macros for Exception Handlers
	 */
.macro EXCEPTION_HANDLER irqname
	.align 5
\irqname:
.endm

/* Push registers on stack */
.macro PUSH_REGS
	push	{sp, lr}
	push	{r0-r12}
	mrs	r0, spsr_hyp
	mrs	r1, elr_hyp
	push	{r0, r1}
.endm

/* Call C function to handle exception */
.macro CALL_EXCEPTION_CFUNC cfunc
	mov	r0, sp
	bl	\cfunc
.endm

/* Pull registers from stack */
.macro PULL_REGS
	pop	{r0, r1}
	msr	spsr_hyp, r0
	msr	elr_hyp, r1
	ldm	sp, {r0-r14}
	eret
.endm

	/* 
	 * Undefined instruction exception handler.
	 * Note: We will get this exception when we
	 * execute invalide instruction in hypervisor mode
	 */
EXCEPTION_HANDLER _undef_inst
	PUSH_REGS
	CALL_EXCEPTION_CFUNC do_undef_inst
	PULL_REGS

	/* 
	 * Software interrupt exception handler.
	 * Note: We will get this exception when we
	 * execute hvc or svc instruction in hypervisor mode
	 */
EXCEPTION_HANDLER _soft_irq
	PUSH_REGS
	CALL_EXCEPTION_CFUNC do_soft_irq
	PULL_REGS

	/* 
	 * Prefetch abort exception handler.
	 * Note: We will get this exception when we
	 * execute from invalid location in hypervisor mode
	 */
EXCEPTION_HANDLER _prefetch_abort
	PUSH_REGS
	CALL_EXCEPTION_CFUNC do_prefetch_abort
	PULL_REGS

	/* 
	 * Data abort exception handler.
	 * Note: We will get this exception when we
	 * access invalid location in hypervisor mode
	 */
EXCEPTION_HANDLER _data_abort
	PUSH_REGS
	CALL_EXCEPTION_CFUNC do_data_abort
	PULL_REGS

	/* 
	 * Hypervisor trap exception handler.
	 * Note: We will get this exception only when we
	 * have to provide service to Guest
	 */
EXCEPTION_HANDLER _hyp_trap
	PUSH_REGS
	CALL_EXCEPTION_CFUNC do_hyp_trap
	PULL_REGS

	/* 
	 * IRQ exception handler.
	 * Note: We will get this exception only when we
	 * have physical IRQ directed to hypervisor mode
	 */
EXCEPTION_HANDLER _irq
	PUSH_REGS
	CALL_EXCEPTION_CFUNC do_irq
	PULL_REGS

	/* 
	 * FIQ exception handler.
	 * Note: We will get this exception only when we
	 * have physical FIQ directed to hypervisor mode
	 */
EXCEPTION_HANDLER _fiq
	PUSH_REGS
	CALL_EXCEPTION_CFUNC do_fiq
	PULL_REGS

