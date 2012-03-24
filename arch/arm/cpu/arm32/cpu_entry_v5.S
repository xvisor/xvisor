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
 * @file cpu_entry_v5.S
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @brief various entry points (booting, reset, exceptions) to xvisor
 */

#include <cpu_defines.h>

	/* 
	 * Xvisor could be loaded any where in memory by boot loaders.
	 * The _start ensures that Xvisor exectues from intended base
	 * address provided at compile time.
	 */
	.section .entry, "ax", %progbits
	.globl _start
_start:
	/*
	 * r4 -> load start
	 * r5 -> load end
	 * r6 -> execution start
	 * r7 -> execution end
	 * r2 -> code size
	 */
	add	r4, pc, #-0x8	 /* r4 loaded with load start */
	ldr	r6, __exec_start /* r6 loaded with __exec_start */
	ldr	r7, __exec_end	 /* r7 loaded with __exec_end */
	sub	r2, r7, r6	 /* r2 loaded with __exec size */
	add	r5, r4, r2	 /* r5 loaded with load end */
	ldr	r0, __load_start /* compute actual __load_start */
	sub	r0, r0, r6
	add	r0, r0, r4	 /* r0 loaded with __load_start */
	str	r4, [r0]	 /* load load start in __load_start */
	ldr	r0, __load_end	 /* compute actual __load_end */
	sub	r0, r0, r6
	add	r0, r0, r4	 /* r0 loaded with __load_end */
	str	r5, [r0]	 /* load load end in __load_end */
	/*
	 * Hang if execution start is not 1 MB aligned
	 */
	mov	r0, r6
	mov	r0, r0, lsr #20
	mov	r0, r0, lsl #20
	cmp	r0, r6
	blt	_start_hang
	/*
	 * Hang if execution end is not 1 MB aligned
	 */
	mov	r0, r7
	mov	r0, r0, lsr #20
	mov	r0, r0, lsl #20
	cmp	r0, r7
	blt	_start_hang
	/*
	 * Relocate code if load start is not 1 MB aligned
	 */
	mov	r0, r4
	mov	r0, r0, lsr #20
	mov	r0, r0, lsl #20
	cmp	r0, r4
	/* skip relocate if already aligned */
	beq	_start_mmu_init

	/*
	 * Relocate copy function at end after load end address
	 */
	ldr	r0, __copy_start /* r0 loaded with __copy_start */
	ldr	r1, __copy_end   /* r1 loaded with __copy_end */
	sub	r2, r1, r0	 /* r2 loaded with __copy size */
	sub	r0, r0, r6	 /* r0 loaded with __copy_start offset */
	add	r0, r0, r4	 /* r0 loaded with actual __copy_start address */
	mov	r1, r5		 /* r1 loaded with load end */
	bl	_copy		 /* copy the _copy function after the code */

	/*
	 * Use newly relocated copy function to relocate entire code
	 */
	mov	r0, r4		 /* r0 loaded with load start */
	mov	r1, r5		 /* r1 loaded with load end */
	sub	r2, r5, r4	 /* r2 loaded with size */
	mov	r1, r4		 /* r1 loaded with load start */
	mov	r1, r1, lsr #20	 /* align r1 on 1 MB bondary */
	mov	r1, r1, lsl #20
	bl	_start_nextpc1
_start_nextpc1:
	add	lr, lr, #16	 /* adjust return value lr */
	sub	lr, lr, r4	 /* The value will change because of the */
	add	lr, lr, r1
	bx	r5		 /* call _copy */
	/*
	 * Update load start and load end
	 * r4 -> new load start
	 * r5 -> new load end
	 */
	mov	r0, r4		 /* r0 loaded with load start */
	mov	r0, r0, lsr #20  /* align r0 on 1 MB bondary */
	mov	r0, r0, lsl #20
	subs	r1, r4, r0	 /* r1 loaded with diff between load start and aligned address */
	subs	r4, r4, r1	 /* remove offset from load start */
	subs	r5, r5, r1	 /* remove offset from load end */
	ldr	r0, __load_start /* compute actual load start */
	sub	r0, r0, r6
	add	r0, r0, r4
	str	r4, [r0]	 /* update __load_start */
	ldr	r0, __load_end	 /* compute actual load end */
	sub	r0, r0, r6
	add	r0, r0, r4
	str	r5, [r0]	 /* update __load_end */
_start_mmu_init:
	/* r0 -> default Level1 Table base address */
	ldr	r0, __defl1_mem	 /* compute actual __defl1_mem */
	sub	r0, r0, r6
	add	r0, r0, r4	 /* r0 loaded with __defl1_mem */
	mov	r1, #1
	lsl	r1, #14		 /* r1 loaded wih 16 K */
	mov	r2, #0		 /* r2 loaded with 0 */

	/* Clear default Level1 Table (32 K) */
_start_mmu_defl1_clear:
	str	r2, [r0]
	add	r0, r0, #4
	sub	r1, r1, #4
	cmp	r1, #0
	/* do it until we have zeroized the 32 KB */
	bgt	_start_mmu_defl1_clear

	/* OK level1 table is filled with 0 now */

	/*
	 * Create entries in default Level1 Table
	 * for execution & load addresses
	 */
	/* r0 -> default Level1 Table base address */
	ldr	r0, __defl1_mem	 /* compute actual __defl1_mem */
	sub	r0, r0, r6
	add	r0, r0, r4	 /* r0 loaded with __defl1_mem */
	mov	r1, r4
	lsr	r1, #20 	 /* divide load end by 1MB */
	lsl	r1, #2		 /* multiply by 4 (to get an offset in bytes) */
	add	r1, r0, r1	 /* add __defl1_mem to r1 */

	ldr	r3, __mmu_section_attr /* r3 loaded with section attributes */

	orr	r2, r3, r4	 /* r2 loaded with load start and attributes */
	str	r2, [r1]	 /* load Level1 table entry for load start */

	mov	r1, r6		 /* r1 loaded with execute start address */
	lsr	r1, #20 	 /* divide by 1MB */
	lsl	r1, #2		 /* multiply by 4 (to get an offset in bytes) */
	add	r1, r0, r1	 /* add __defl1_mem to r1 */

	sub	r2, r7, r6	 /* r2 loaded with execute size */
	lsr	r2, #20 	 /* divide by 1MB */
	lsl	r2, #2		 /* multiply by 4 (to get an offset in bytes) */
	add	r2, r1, r2	 /* add __defl1_mem to r2 */

	mov	r8, #1		 /* r2 loaded with 1 */
	lsl	r8, #20		 /* multiply by 1MB */
				 /* r8 loaded with 1MB */

_start_mmu_defl1_set:
	orr	r5, r3, r4
	str	r5, [r1]
	add	r4, r4, r8	 /* Add 1MB  to r4 */
	add	r1, r1, #4	 /* Add 4 bytes to r1 */
	cmp	r1, r2		 /* compare r1 with code end */
	/* until we have covered the entire code */
	blt	_start_mmu_defl1_set

	/*
	 * Setup Translation Table Base Register 0
	 * vlue is stored in r0 
	 */
	mcr     p15, 0, r0, c2, c0, 0

	/*
	 * Setup Domain Control Register
	 */
	ldr	r1, __dacr_mmu_val
	mcr	p15, 0, r1, c3, c0, 0

	/*
	 * Setup System Control Register
	 */
	ldr	r1, __sctrl_mmu_val
	mrc     p15, 0, r2, c1, c0, 0
	orr	r2, r2, r1
	mcr     p15, 0, r2, c1, c0, 0

	/*
	 * Jump to reset code
	 */
	ldr	pc, __reset
_start_hang:
	b	.

#define SECTION_ATTR 	((TTBL_AP_SRW_U << TTBL_L1TBL_TTE_AP_SHIFT) | \
			(TTBL_L1TBL_TTE_DOM_RESERVED << TTBL_L1TBL_TTE_DOM_SHIFT) | \
			TTBL_L1TBL_TTE_REQ_MASK | TTBL_L1TBL_TTE_C_MASK | \
			TTBL_L1TBL_TTE_TYPE_SECTION)

__mmu_section_attr:
	.word SECTION_ATTR
__dacr_mmu_val:
	.word (TTBL_DOM_CLIENT << (2 * TTBL_L1TBL_TTE_DOM_RESERVED))
__sctrl_mmu_val:
	.word (SCTLR_M_MASK | SCTLR_C_MASK | SCTLR_I_MASK)
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
__defl1_mem:
	.word defl1_mem

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
	.globl _start_vect
_start_vect:
	ldr	pc, __reset
	ldr	pc, __undefined_instruction
	ldr	pc, __software_interrupt
	ldr	pc, __prefetch_abort
	ldr	pc, __data_abort
	ldr	pc, __not_used
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
__not_used:
	.word _not_used
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
__svc_stack_end:
	.word _svc_stack_end
__und_stack_end:
	.word _und_stack_end
__abt_stack_end:
	.word _abt_stack_end
__irq_stack_end:
	.word _irq_stack_end
__fiq_stack_end:
	.word _fiq_stack_end

	/* 
	 * Reset exception handler.
	 * Reset hardware state before starting Xvisor.
	 */
	.globl _reset
_reset:
	/* Clear a register for temporary usage */
	mov	r8, #0
	/* Disable IRQ & FIQ */
	mrs	r8, cpsr_all
	orr	r8, r8, #(CPSR_IRQ_DISABLED | CPSR_FIQ_DISABLED)
	msr	cpsr_cxsf, r8
	/* Set Supervisor Mode Stack */
	mov	r8, r8, lsr #5
	mov	r8, r8, lsl #5
	orr	r8, r8, #CPSR_MODE_SUPERVISOR
	msr	cpsr_c, r8
	ldr	sp, __svc_stack_end
	/* Set Undefined Mode Stack */
	mov	r8, r8, lsr #5
	mov	r8, r8, lsl #5
	orr	r8, r8, #CPSR_MODE_UNDEFINED
	msr	cpsr_c, r8
	ldr	sp, __und_stack_end
	/* Set Abort Mode Stack */
	mov	r8, r8, lsr #5
	mov	r8, r8, lsl #5
	orr	r8, r8, #CPSR_MODE_ABORT
	msr	cpsr_c, r8
	ldr	sp, __abt_stack_end
	/* Set IRQ Mode Stack */
	mov	r8, r8, lsr #5
	mov	r8, r8, lsl #5
	orr	r8, r8, #CPSR_MODE_IRQ
	msr	cpsr_c, r8
	ldr	sp, __irq_stack_end
	/* Set FIQ Mode Stack */
	mov	r8, r8, lsr #5
	mov	r8, r8, lsl #5
	orr	r8, r8, #CPSR_MODE_FIQ
	msr	cpsr_c, r8
	ldr	sp, __fiq_stack_end
	/* Set to Supervisor Mode */
	mov	r8, r8, lsr #5
	mov	r8, r8, lsl #5
	orr	r8, r8, #CPSR_MODE_SUPERVISOR
	msr	cpsr_c, r8
	/* Call CPU init function */
	b	cpu_init
	/* We should never reach here */
	b	.

	/* 
	 * Helper Macros for Exception Handlers
	 */
.macro EXCEPTION_HANDLER irqname, lroffset
	.align 5
\irqname:
	sub	lr, lr, #\lroffset
.endm

/* If came from priviledged mode then push banked registers */
.macro PUSH_REGS skip_label
	str     lr, [sp, #-4]!;         /* Push the return address */
	sub     sp, sp, #(4*15);        /* Adjust the stack pointer */
	stmia   sp, {r0-r12};           /* Push user mode registers */
	add     r0, sp, #(4*13);        /* Adjust the stack pointer */
	stmia   r0, {r13-r14}^;         /* Push user mode registers */
	mov     r0, r0;                 /* NOP for previous inst */
	mrs     r0, spsr_all;           /* Put the SPSR on the stack */
	str     r0, [sp, #-4]!
	mov	r4, r0
	and	r0, r0, #CPSR_MODE_MASK
	cmp	r0, #CPSR_MODE_USER
	beq	\skip_label
	add	r1, sp, #(4*14)
	mrs	r5, cpsr_all
	orr	r4, r4, #(CPSR_IRQ_DISABLED | CPSR_FIQ_DISABLED)
	msr	cpsr_cxsf, r4
	str	sp, [r1, #0]
	str	lr, [r1, #4]
	msr	cpsr_cxsf, r5
\skip_label:
.endm

/* Call C function to handle exception */
.macro CALL_EXCEPTION_CFUNC cfunc
	mov	r0, sp
	bl	\cfunc
.endm

/* If going back to priviledged mode then pull banked registers */
.macro PULL_REGS skip_label skip_label1
	ldr     r0, [sp, #0]
	mov	r4, r0
	and	r0, r0, #CPSR_MODE_MASK
	cmp	r0, #CPSR_MODE_USER
	beq	\skip_label
	mrs	r1, cpsr_all
	and 	r1, r1, #CPSR_MODE_MASK
	cmp	r0, r1
	beq	\skip_label1
	add	r1, sp, #(4*14)
	mrs	r5, cpsr_all
	orr	r4, r4, #(CPSR_IRQ_DISABLED | CPSR_FIQ_DISABLED)
	msr	cpsr_cxsf, r4
	ldr	sp, [r1, #0]
	ldr	lr, [r1, #4]
	msr	cpsr_cxsf, r5
\skip_label:
	ldr     r0, [sp], #0x0004;      /* Get SPSR from stack */
	msr     spsr_cxsf, r0;
	ldmia   sp, {r0-r14}^;          /* Restore registers (user) */
	mov     r0, r0;                 /* NOP for previous isnt */
	add     sp, sp, #(4*15);        /* Adjust the stack pointer */
	ldr     lr, [sp], #0x0004       /* Pull return address */
	movs	pc, lr
\skip_label1:
	ldr     r0, [sp], #0x0004;      /* Get SPSR from stack */
	msr     spsr_cxsf, r0;
	ldm	sp, {r0-r15}^;          /* Restore all registers & CPSR */
	mov     r0, r0;                 /* NOP for previous inst */
.endm

	/* 
	 * Undefined instruction exception handler.
	 */
EXCEPTION_HANDLER _undef_inst, 4
	PUSH_REGS _undef_inst_push
	CALL_EXCEPTION_CFUNC do_undef_inst
	PULL_REGS _undef_inst_pull _undef_inst_pull1

	/* 
	 * Software interrupt exception handler.
	 */
EXCEPTION_HANDLER _soft_irq, 4
	PUSH_REGS _soft_irq_push
	CALL_EXCEPTION_CFUNC do_soft_irq
	PULL_REGS _soft_irq_pull _soft_irq_pull1

	/* 
	 * Prefetch abort exception handler.
	 */
EXCEPTION_HANDLER _prefetch_abort, 4
	PUSH_REGS _prefetch_abort_push
	CALL_EXCEPTION_CFUNC do_prefetch_abort
	PULL_REGS _prefetch_abort_pull _prefetch_abort_pull1

	/* 
	 * Data abort exception handler.
	 */
EXCEPTION_HANDLER _data_abort, 8
	PUSH_REGS _data_abort_push
	CALL_EXCEPTION_CFUNC do_data_abort
	PULL_REGS _data_abort_pull _data_abort_pull1

	/* 
	 * Not used exception handler.
	 */
EXCEPTION_HANDLER _not_used, 4
	PUSH_REGS _not_used_push
	CALL_EXCEPTION_CFUNC do_not_used
	PULL_REGS _not_used_pull _not_used_pull1

	/* 
	 * IRQ exception handler.
	 */
EXCEPTION_HANDLER _irq, 4
	PUSH_REGS _irq_push
	CALL_EXCEPTION_CFUNC do_irq
	PULL_REGS _irq_pull _irq_pull1

	/* 
	 * FIQ exception handler.
	 */
EXCEPTION_HANDLER _fiq, 4
	PUSH_REGS _fiq_push
	CALL_EXCEPTION_CFUNC do_fiq
	PULL_REGS _fiq_pull _fiq_pull1
