/**
 * Copyright (c) 2011 Anup Patel.
 * Copyright (c) 2012 Jean-Christophe Dubois.
 * Copyright (c) 2012 Sukanto Ghosh
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
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief various entry points (booting, reset, exceptions) to xvisor
 */

#include <cpu_defines.h>

	/*
	 * Xvisor could be loaded any where in memory by boot loaders.
	 * The _start ensures that Xvisor executes from intended base
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
	 * r10 -> core# in case of SMP
	 */
	add	r4, pc, #-0x8
	ldr	r6, __exec_start
	ldr	r7, __exec_end
	sub	r2, r7, r6
	add	r5, r4, r2

#ifdef CONFIG_SMP
	/* Secondary CPUs directly start mmu setup from here */
	mrc     p15, 0, r10, c0, c0, 5
	ands	r10, r10, #0xFF
	cmp	r10, #0
	bne	_start_mmu_setup
#endif

	ldr	r0, __load_start
	sub	r0, r0, r6
	add	r0, r0, r4
	str	r4, [r0]
	ldr	r0, __load_end
	sub	r0, r0, r6
	add	r0, r0, r4
	str	r5, [r0]
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
	 * Relocate code if load start is not 1MB aligned
	 */
	mov	r0, r4
	mov	r0, r0, lsr #20
	mov	r0, r0, lsl #20
	cmp	r0, r4
	/*
	 * Skip relocation if already aligned
	 */
	beq	_start_mmu_init

	/*
	 * Relocate copy function at end after load end address
	 */
	ldr	r0, __copy_start
	ldr	r1, __copy_end
	sub	r2, r1, r0	 /* r2 -> __copy size */
	sub	r0, r0, r6
	add	r0, r0, r4	 /* r0 -> load_address of copy_start */
	mov	r1, r5		 /* r1 -> load end */
	bl	_copy		 /* copy the _copy function after the code */

	/*
	 * Use newly relocated copy function to relocate entire code to 1MB boundary
	 */
	mov	r0, r4		 /* r0 -> load start */
	mov	r1, r4
	mov	r1, r1, lsr #20
	mov	r1, r1, lsl #20	 /* r1 -> load start aligned to 1MB boundary */
	sub	r2, r5, r4	 /* r2 -> code size */
	bl	_start_nextpc1
_start_nextpc1:
	add	lr, lr, #16	 /* Adjust return address (lr) for jump to */
	sub	lr, lr, r4	 /* relocated address on return from _copy */
	add	lr, lr, r1
	bx	r5		 /* call _copy */
	/*
	 * Update load start and load end
	 */
	mov	r0, r4
	mov	r0, r0, lsr #20
	mov	r0, r0, lsl #20  /* r0 -> load_start aligned to 1MB boundary */
	subs	r1, r4, r0	 /* r1 -> offset between load start and aligned address */
	subs	r4, r4, r1	 /* r4 -> new load start */
	subs	r5, r5, r1	 /* r5 -> new load end */
	ldr	r0, __load_start
	sub	r0, r0, r6
	add	r0, r0, r4
	str	r4, [r0]
	ldr	r0, __load_end
	sub	r0, r0, r6
	add	r0, r0, r4
	str	r5, [r0]
_start_mmu_init:
	/*
	 * Clear default Level1 Table
	 */
	ldr	r0, __tmpl1_mem
	sub	r0, r0, r6
	add	r0, r0, r4
	ldr	r1, __tmpl1_mem_addr
	sub	r1, r1, r6
	add	r1, r1, r4
	str	r0, [r1]
	/* r0 -> default Level1 Table base address */
	mov	r1, #1
	lsl	r1, #14		 /* r1 -> 16K */
	mov	r2, #0
	mov	r3, r0

_start_mmu_tmpl1_clear:
	str	r2, [r3]
	add	r3, r3, #4
	sub	r1, r1, #4
	cmp	r1, #0
	bgt	_start_mmu_tmpl1_clear

	/*
	 * Create entries in default Level1 Table
	 * for execution & load addresses
	 */
	ldr	r3, __mmu_section_attr
	/* r1 -> load entry start address */
	mov	r1, r4		/* r4 -> load start address */
	lsr	r1, #18 	/* r1 >> 20, r1 << 2 */
	add	r1, r0, r1
	/* Setup load entry */
	orr	r2, r3, r4
	str	r2, [r1]
	/* r1 -> execute entry start address */
	mov	r1, r6
	lsr	r1, #18		/* r1 >> 20, r1 << 2 */
	add	r1, r0, r1
	/* r2 -> execute entry end address */
	sub	r2, r7, r6
	lsr	r2, #18		/* r2 >> 20, r2 << 2 */
	add	r2, r1, r2
	/* r5 -> temporary value */
	mov	r5, #0
_start_mmu_tmpl1_set:
	orr	r5, r3, r4
	str	r5, [r1]
	lsr	r4, #20
	add	r4, r4, #1
	lsl	r4, #20
	add	r1, r1, #4
	cmp	r1, r2
	/* until we have covered the entire code */
	blt	_start_mmu_tmpl1_set

_start_mmu_setup:
	ldr	r0, __tmpl1_mem

	/*
	 * Setup Translation Table Base Register 0
	 */
	mcr	p15, 0, r0, c2, c0, 0

	/*
	 * Setup Domain Control Register
	 */
	ldr	r1, __dacr_mmu_val
	mcr	p15, 0, r1, c3, c0, 0

	/*
	 * Setup System Control Register
	 */
	ldr	r1, __sctrl_mmu_val
	ldr	r2, __sctrl_mmu_clear
	mrc	p15, 0, r3, c1, c0, 0
	and	r3, r3, r2
	orr	r3, r3, r1
	mcr	p15, 0, r3, c1, c0, 0

	/*
	 * Jump to reset code
	 */
	ldr	pc, __reset
_start_hang:
	b	.

#if (__ARM_ARCH_VERSION__ < 6)
#define TTBL_L1TBL_TTE_ARCH_ATTR	TTBL_L1TBL_TTE_REQ_MASK
#else
#define TTBL_L1TBL_TTE_ARCH_ATTR	TTBL_L1TBL_TTE_B_MASK
#endif

#define SECTION_ATTR 	((TTBL_AP_SRW_U << TTBL_L1TBL_TTE_AP_SHIFT) | \
			(TTBL_L1TBL_TTE_DOM_RESERVED << TTBL_L1TBL_TTE_DOM_SHIFT) | \
			TTBL_L1TBL_TTE_ARCH_ATTR | \
			TTBL_L1TBL_TTE_TYPE_SECTION)

__mmu_section_attr:
	.word SECTION_ATTR
__dacr_mmu_val:
	.word (TTBL_DOM_CLIENT << (2 * TTBL_L1TBL_TTE_DOM_RESERVED))
__sctrl_mmu_clear:
	.word ~(SCTLR_A_MASK | SCTLR_TRE_MASK)
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
__tmpl1_mem_addr:
	.word __tmpl1_mem
__tmpl1_mem:
	.word tmpl1_mem
#if (__ARM_ARCH_VERSION__ < 6)
	.globl _ifar
_ifar:
	.word 0x0
	.globl _abort_inst
_abort_inst:
	.word 0x0
#endif

/*
 * CMODE - Change processor mode
 * \rs : the GPR having cpsr value
 * \mode : target processor mode
 *
 * Please note we do *not* read cpsr here
 * \rs is supposed to have cpsr before calling this
 * This macro is called from performance critical paths
 * (read exception entry/exit) and to allow optimizations
 * we do *not* read cpsr in this macro and instead
 * expect \rs to have cpsr before calling this.
 */
#if (__ARM_ARCH_VERSION__ < 6)
.macro	CMODE rs mode
	and	\rs, \rs, #~(CPSR_MODE_MASK)
	orr	\rs, \rs, #\mode
	msr	cpsr_c, \rs
.endm
#else
.macro	CMODE rs mode
	cps	#\mode
.endm
#endif

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
	CMODE	r8, CPSR_MODE_SUPERVISOR
	ldr	sp, __svc_stack_end
#ifdef CONFIG_SMP
	mov	r9, #CONFIG_IRQ_STACK_SIZE
	mul	r9, r9, r10
	sub	sp, sp, r9
#endif
	/* Set Undefined Mode Stack */
	CMODE	r8, CPSR_MODE_UNDEFINED
	ldr	sp, __und_stack_end
#ifdef CONFIG_SMP
	mov	r9, #0x100
	mul	r9, r9, r10
	sub	sp, sp, r9
#endif
	/* Set Abort Mode Stack */
	CMODE	r8, CPSR_MODE_ABORT
	ldr	sp, __abt_stack_end
#ifdef CONFIG_SMP
	mov	r9, #0x100
	mul	r9, r9, r10
	sub	sp, sp, r9
#endif
	/* Set IRQ Mode Stack */
	CMODE	r8, CPSR_MODE_IRQ
	ldr	sp, __irq_stack_end
#ifdef CONFIG_SMP
	mov	r9, #0x100
	mul	r9, r9, r10
	sub	sp, sp, r9
#endif
	/* Set FIQ Mode Stack */
	CMODE	r8, CPSR_MODE_FIQ
	ldr	sp, __fiq_stack_end
#ifdef CONFIG_SMP
	mov	r9, #0x100
	mul	r9, r9, r10
	sub	sp, sp, r9
#endif
	/* Set to Supervisor Mode */
	CMODE	r8, CPSR_MODE_SUPERVISOR
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
	sub	lr, lr, #\lroffset;
.endm

/* Save arch registers into small mode-specific stack */
.macro PUSH_REGS skip_label mode=CPSR_MODE_SUPERVISOR
	/* Save return address (i.e. return PC) */
	str	lr, [sp, #-4]!;
	/* Adjust stack pointer to make room for r13, r14 */		
	sub	sp, sp, #(4*2);
	/* Save usr mode r13, r14 */
	stm	sp, {r13-r14}^;
	/* Save r0, r1, ..., r12 and decrement stack pointer */
	stmdb	sp!, {r0-r12};
	/* Save return machine state and decrement stack pointer */
	mrs	r4, spsr_all;
	str	r4, [sp, #-4]!;
	/* If we came from privildeged (or non-usr) mode then
	 * overwrite r13, r14 with mode specific banked values
	 * else skip this step
	 */
	mrs	r2, cpsr_all;
	and	r0, r4, #CPSR_MODE_MASK;
	cmp	r0, #CPSR_MODE_USER;
	beq	\skip_label;
	add	r1, sp, #(4*14);
	orr	r4, r4, #(CPSR_IRQ_DISABLED | CPSR_FIQ_DISABLED);
	msr	cpsr_cxsf, r4;
	stm	r1, {r13-r14};
	msr	cpsr_cxsf, r2;
	/* Save restore value for SVC stack (or Exception stack) */
\skip_label:
	sub	sp, sp, #4;
	mov	r0, sp;
	.if \mode == CPSR_MODE_SUPERVISOR
	add	r1, sp, #(4*18);
	str	r1, [r0];
	.else
	CMODE	r2, CPSR_MODE_SUPERVISOR;
	str	sp, [r0];
	.endif
.endm

/* Call C function to handle exception */
.macro CALL_EXCEPTION_CFUNC cfunc
	bl	\cfunc;
.endm

/* Restore registers from arch registers */
.macro PULL_REGS skip_label skip_label1 mode=CPSR_MODE_SUPERVISOR
	/* Get arch regs and current cpsr.
	 * Also adjust stack pointer of \mode
	 * r0 -> pointer to arch regs
	 * r1 -> current cpsr
	 */
	mrs	r1, cpsr_all;
	CMODE	r1, \mode;
	mov	r0, sp;
	add	sp, sp, #(4*18);
	CMODE	r1, CPSR_MODE_SUPERVISOR;
	/* Restore exception stack */ 
	ldr	sp, [r0, #0];
	/* Get target cpsr 
	 * r2 -> target cpsr
	 */
	ldr	r2, [r0, #4];
	/* Adjust pointer to arch regs */
	add	r0, r0, #(2*4);
	/* If-else ladder for different target modes  */
	and	r3, r2, #CPSR_MODE_MASK;
	cmp	r3, #CPSR_MODE_USER;
	beq	\skip_label;
	cmp	r3, #CPSR_MODE_SUPERVISOR;
	beq	\skip_label1;
	/* Going back to irq/und/abt mode */
	orr	r4, r2, #(CPSR_IRQ_DISABLED | CPSR_FIQ_DISABLED);
	msr	cpsr_cxsf, r4;
	ldr	sp, [r0, #(4*13)];
	ldr	lr, [r0, #(4*14)];
	msr	cpsr_cxsf, r1;
	msr	spsr_cxsf, r2;
	ldr	lr, [r0, #(4*15)];
	ldmia   r0, {r0-r12};
	movs	pc, lr;
	/* Going back to usr mode */
\skip_label:
	msr	spsr_cxsf, r2;
	ldr	lr, [r0, #(4*15)];
	ldmia   r0, {r0-r14}^;
	movs	pc, lr
	/* Going back to svc mode */
\skip_label1:
	msr	spsr_cxsf, r2;
	ldm	r0, {r0-r15}^;
	mov	r0, r0;
.endm

	/*
	 * Undefined instruction exception handler.
	 */
EXCEPTION_HANDLER _undef_inst, 4
	PUSH_REGS _undef_inst_push CPSR_MODE_UNDEFINED
	CALL_EXCEPTION_CFUNC do_undef_inst
	PULL_REGS _undef_inst_pull _undef_inst_pull1 CPSR_MODE_UNDEFINED

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
	PUSH_REGS _prefetch_abort_push CPSR_MODE_ABORT
#if (__ARM_ARCH_VERSION__ < 6)
	ldr r1, _ifar;
	ldr lr, [r0, #(4 * 17)]
	str lr, [r1];
#endif
	CALL_EXCEPTION_CFUNC do_prefetch_abort
	PULL_REGS _prefetch_abort_pull _prefetch_abort_pull1 CPSR_MODE_ABORT

	/*
	 * Data abort exception handler.
	 */
EXCEPTION_HANDLER _data_abort, 8
	PUSH_REGS _data_abort_push CPSR_MODE_ABORT
#if (__ARM_ARCH_VERSION__ < 6)
	ldr r1, _abort_inst;
	ldr lr, [r0, #(4 * 17)]
	str lr, [r1];
#endif
	CALL_EXCEPTION_CFUNC do_data_abort
	PULL_REGS _data_abort_pull _data_abort_pull1 CPSR_MODE_ABORT

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
	PUSH_REGS _irq_push CPSR_MODE_IRQ
	CALL_EXCEPTION_CFUNC do_irq
	PULL_REGS _irq_pull _irq_pull1 CPSR_MODE_IRQ

	/*
	 * FIQ exception handler.
	 */
EXCEPTION_HANDLER _fiq, 4
	PUSH_REGS _fiq_push CPSR_MODE_FIQ
	CALL_EXCEPTION_CFUNC do_fiq
	PULL_REGS _fiq_pull _fiq_pull1 CPSR_MODE_FIQ
