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
	 * _start: Primary CPU startup code
	 * _start_secondary: Secondary CPU startup code
	 * _start_secondary_nopen: Secondary CPU startup code without holding pen
	 *
	 * Note: Xvisor could be loaded any where in memory by boot loaders.
	 * The _start ensures that Xvisor executes from intended base address 
	 * provided at compile time.
	 */
	.section .entry, "ax", %progbits
	.globl _start
	.globl _start_secondary
	.globl _start_secondary_nopen
_start:
	/* r4 -> load start
	 * r5 -> load end
	 * r6 -> execution start
	 * r7 -> execution end
	 * r10 -> core# in case of SMP
	 */
	add	r4, pc, #-0x8
	ldr	r6, __exec_start
	ldr	r7, __exec_end
	sub	r3, r7, r6
	add	r5, r4, r3

	/* Save boot reg0 (i.e. r0) */
	ldr	r3, __boot_reg0
	sub	r3, r3, r6
	add	r3, r3, r4
	str	r0, [r3]

	/* Save boot reg1 (i.e. r1) */
	ldr	r3, __boot_reg1
	sub	r3, r3, r6
	add	r3, r3, r4
	str	r1, [r3]

	/* Save boot reg2 (i.e. r2) */
	ldr	r3, __boot_reg2
	sub	r3, r3, r6
	add	r3, r3, r4
	str	r2, [r3]

	/* Save load start and load end */
	ldr	r0, __load_start
	sub	r0, r0, r6
	add	r0, r0, r4
	str	r4, [r0]
	ldr	r0, __load_end
	sub	r0, r0, r6
	add	r0, r0, r4
	str	r5, [r0]

	/* Hang if execution start is not 4KB aligned */
	mov	r0, r6
	mov	r0, r0, lsr #12
	mov	r0, r0, lsl #12
	cmp	r0, r6
	blt	.

	/* Hang if execution end is not 4KB aligned */
	mov	r0, r7
	mov	r0, r0, lsr #12
	mov	r0, r0, lsl #12
	cmp	r0, r7
	blt	.

	/* Zero-out bss section */
	mov	r0, #0
	ldr	r1, __bss_start
	sub	r1, r1, r6
	add	r1, r1, r4
	ldr	r2, __bss_end
	sub	r2, r2, r6
	add	r2, r2, r4
_bss_zero:
	str	r0, [r1], #4
	cmp	r1, r2
	blt	_bss_zero

	/* Determine if a valid external dtb is provided */
	/* Get DTB address */
	ldr     r3, __boot_reg2
	sub     r3, r3, r6
	add     r3, r3, r4
	ldr     r2, [r3]

	/* First check for 4B alignment */
	tst	r2, #0x3
	bne	align_4k_boundary

	/* Now check for FDT_MAGIC */
	ldr	r0, [r2]
	ldr	r1, __fdt_magic
	cmp	r0, r1
	bne	align_4k_boundary

	/* Now get the dtb total-size */
	ldrb	r0, [r2, #4]
	ldrb	r1, [r2, #5]
	add	r0, r1, r0, lsl #8
	ldrb	r1, [r2, #6]
	add	r0, r1, r0, lsl #8
	ldrb	r1, [r2, #7]
	add	r0, r1, r0, lsl #8

	/* Compare with max supported dtb size */
	ldr	r1, __max_dtb_size
	cmp	r0, r1
	bgt	align_4k_boundary

	/* Overwrite the built-in fdt with the one passed */
	ldr	r1, __builtin_dtb
	sub	r1, r1, r6
	add	r1, r1, r4

	/* r0 = dtb total size */
	/* r1 = builtin_dtb load-address */
	/* r2 = passed dtb address */

dtb_save_loop:
	ldr	r3, [r2], #4
	str	r3, [r1], #4
	subs	r0, r0, #4
	bgt	dtb_save_loop

align_4k_boundary:
	/* Relocate code if load start is not 4KB aligned */
	mov	r0, r4
	mov	r0, r0, lsr #12
	mov	r0, r0, lsl #12
	cmp	r0, r4
	/* Skip relocation if already aligned */
	beq	_start_mmu_init

	/* Relocate copy function at end after load end address */
	ldr	r0, __copy_start
	ldr	r1, __copy_end
	sub	r2, r1, r0	 /* r2 -> __copy size */
	sub	r0, r0, r6
	add	r0, r0, r4	 /* r0 -> load_address of copy_start */
	mov	r1, r5		 /* r1 -> load end */
	bl	_copy		 /* copy the _copy function after the code */

	/* Use newly relocated copy function to relocate entire code
	 * to 1MB boundary 
	 */
	mov	r0, r4		 /* r0 -> load start */
	mov	r1, r4
	mov	r1, r1, lsr #12
	mov	r1, r1, lsl #12	 /* r1 -> load start aligned to 4KB boundary */
	sub	r2, r5, r4	 /* r2 -> code size */
	bl	_start_nextpc1
_start_nextpc1:
	add	lr, lr, #16	 /* Adjust return address (lr) for jump to */
	sub	lr, lr, r4	 /* relocated address on return from _copy */
	add	lr, lr, r1
	bx	r5		 /* call _copy */
	/* Update load start and load end */
	mov	r0, r4
	mov	r0, r0, lsr #12
	mov	r0, r0, lsl #12  /* r0 -> load_start aligned to 4KB boundary */
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
	ldr	r0, __defl1_ttbl
	sub	r0, r0, r6
	add	r0, r0, r4
	ldr	r1, __defl1_ttbl_addr
	sub	r1, r1, r6
	add	r1, r1, r4
	str	r0, [r1]

	ldr	sp, __svc_stack_end
	sub	sp, sp, r6
	add	sp, sp, r4

	mov	r0, r4
	mov	r1, r5
	mov	r2, r6
	mov	r3, r7
	bl	_setup_initial_ttbl
	b	_start_secondary_nopen

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

#ifdef CONFIG_SMP
	.align	3
__start_secondary_pen_release:
	.word	start_secondary_pen_release

	/*
	 * Secondary CPU startup code 
	 */
_start_secondary:
	/*
	 * This provides a "holding pen" for platforms to hold all secondary
	 * cores are held until we're ready for them to initialise.
	 */
	mrc     p15, 0, r0, c0, c0, 5
	ldr     r1, =MPIDR_HWID_BITMASK
	and	r0, r0, r1

	/* Calculate load address of secondary_holding_pen_release */
	ldr	r1, __start_secondary_pen_release
	ldr	r2, __exec_start
	ldr	r3, _load_start
	sub	r1, r1, r2
	add	r1, r1, r3
	sev
pen:	wfe
	ldr	r4, [r1]
	cmp	r4, r0
	bne	pen
#endif

	/*
	 * Note: From this point primary CPU startup is same as secondary CPU
	 */
_start_secondary_nopen:
	/*
	 * Latest ARMv7 bootloaders will enter Xvisor in HYP-mode
	 * if Virtualization extenstion is available as-per Linux
	 * ARM32 booting protocol.
	 *
	 * For sanity we force-fully switch to SVC mode.
	 */
	CMODE	r0, CPSR_MODE_SUPERVISOR

	/* Setup Translation Table Base Register 0 */
	ldr	r0, __defl1_ttbl
	mcr	p15, 0, r0, c2, c0, 0

	/* Setup Domain Control Register */
	ldr	r1, __dacr_mmu_val
	mcr	p15, 0, r1, c3, c0, 0

#ifdef CONFIG_SMP
	/* Setup SMP ID for current processor */
	bl	proc_setup_smp_id
#endif

	/* Setup System Control Register */
	bl	proc_setup
	mcr	p15, 0, r0, c1, c0, 0

	/* Jump to reset code */
	ldr	pc, __reset

	/* We should never reach here. */
	b	.

#if (__ARM_ARCH_VERSION__ < 6)
#define TTBL_L1TBL_TTE_ARCH_ATTR	TTBL_L1TBL_TTE_REQ_MASK
#else
#define TTBL_L1TBL_TTE_ARCH_ATTR	(TTBL_L1TBL_TTE_C_MASK | \
					TTBL_L1TBL_TTE_B_MASK)
#endif

#define SECTION_ATTR 	((TTBL_AP_SRW_U << TTBL_L1TBL_TTE_AP_SHIFT) | \
			(TTBL_L1TBL_TTE_DOM_RESERVED << TTBL_L1TBL_TTE_DOM_SHIFT) | \
			TTBL_L1TBL_TTE_ARCH_ATTR | \
			TTBL_L1TBL_TTE_TYPE_SECTION)

#define FDT_MAGIC	0xedfe0dd0	/* 0xd00dfeed in big-endian */

__fdt_magic:
	.word FDT_MAGIC
__builtin_dtb:
	.word dt_blob_start
__max_dtb_size:
	.word CONFIG_ARM_MAX_DTB_SIZE
__mmu_section_attr:
	.word SECTION_ATTR
__dacr_mmu_val:
	.word (TTBL_DOM_CLIENT << (2 * TTBL_L1TBL_TTE_DOM_RESERVED))
__boot_reg0:
	.word _boot_reg0
__boot_reg1:
	.word _boot_reg1
__boot_reg2:
	.word _boot_reg2
__exec_start:
	.word _code_start
__exec_end:
	.word _code_end
__load_start:
	.word _load_start
__load_end:
	.word _load_end
__bss_start:
	.word _bss_start
__bss_end:
	.word _bss_end
__copy_start:
	.word _copy
__copy_end:
	.word _copy_end
__defl1_ttbl_addr:
	.word __defl1_ttbl
__defl1_ttbl:
	.word defl1_ttbl
#if (__ARM_ARCH_VERSION__ < 6)
	.globl _ifar
_ifar:
	.word 0x0
	.globl _abort_inst
_abort_inst:
	.word 0x0
#endif

	/*
	 * Boot register 0 passed by bootloader
	 */
	.globl _boot_reg0
_boot_reg0:
	.word 0x0

	/*
	 * Boot register 1 passed by bootloader
	 */
	.globl _boot_reg1
_boot_reg1:
	.word 0x0

	/*
	 * Boot register 2 passed by bootloader
	 */
	.globl _boot_reg2
_boot_reg2:
	.word 0x0

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
	bl	arch_smp_id
	mov	r10, r0
	mov	r9, #CONFIG_IRQ_STACK_SIZE
	mul	r9, r9, r10
	sub	sp, sp, r9
#endif
	/* Set Undefined Mode Stack */
	CMODE	r8, CPSR_MODE_UNDEFINED
	ldr	sp, __und_stack_end
#ifdef CONFIG_SMP
	bl	arch_smp_id
	mov	r10, r0
	mov	r9, #0x100
	mul	r9, r9, r10
	sub	sp, sp, r9
#endif
	/* Set Abort Mode Stack */
	CMODE	r8, CPSR_MODE_ABORT
	ldr	sp, __abt_stack_end
#ifdef CONFIG_SMP
	bl	arch_smp_id
	mov	r10, r0
	mov	r9, #0x100
	mul	r9, r9, r10
	sub	sp, sp, r9
#endif
	/* Set IRQ Mode Stack */
	CMODE	r8, CPSR_MODE_IRQ
	ldr	sp, __irq_stack_end
#ifdef CONFIG_SMP
	bl	arch_smp_id
	mov	r10, r0
	mov	r9, #0x100
	mul	r9, r9, r10
	sub	sp, sp, r9
#endif
	/* Set FIQ Mode Stack */
	CMODE	r8, CPSR_MODE_FIQ
	ldr	sp, __fiq_stack_end
#ifdef CONFIG_SMP
	bl	arch_smp_id
	mov	r10, r0
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
.macro PUSH_REGS mode=CPSR_MODE_SUPERVISOR
	/* Save return address and return machine state */
	.if \mode != CPSR_MODE_SUPERVISOR
		/* Temporary save return address, machine state, R0 and R1 */
		str	lr, [sp, #-4];
		mrs	lr, spsr_all;
		str	lr, [sp, #-8];
		str	r1, [sp, #-12];
		str	r0, [sp, #-16];
		/* Point R0 to temporary location */
		mov	r0, sp
#if (__ARM_ARCH_VERSION__ < 6)
		mrs	lr, cpsr_all;
		and	lr, lr, #~(CPSR_MODE_MASK);
		orr	lr, lr, #CPSR_MODE_SUPERVISOR;
		msr	cpsr_c, lr;
#else
		cps	#CPSR_MODE_SUPERVISOR;
#endif
		/* Save return address (i.e. return PC) */
		ldr	r1, [r0, #-4];
		str	r1, [sp, #-4]!;
		/* Save return machine state and decrement stack pointer */
		ldr	r1, [r0, #-8];
		str	r1, [sp, #-(4*16)];
		/* Restore R0, R1 register */
		ldr	r1, [r0, #-12];
		ldr	r0, [r0, #-16];
	.else
		/* Save return address (i.e. return PC) */
		str	lr, [sp, #-4]!;
		/* Save return machine state and decrement stack pointer */
		mrs	lr, spsr_all;
		str	lr, [sp, #-(4*16)];
	.endif

	/* Save r0, r1, ..., r14 and decrement stack pointer */
	stmdb	sp, {r0-r14}^;
	sub	sp, sp, #(4*16);

	/* If we came from privildeged (or non-usr) mode then
	 * overwrite r13, r14 with mode specific banked values
	 * else skip this step
	 */
	ldr	r4, [sp];
	and	r0, r4, #CPSR_MODE_MASK;
	cmp	r0, #CPSR_MODE_USER;
	beq	100f;
	cmp	r0, #CPSR_MODE_SUPERVISOR;
	beq	101f;

	/* We handle all exceptions, irqs, and fiqs in SVC mode so,
	 * should never reach here. Just sit in infinite loop !!!!! 
	 */
	b	.;

	/* Came from svc mode */
101:	add	r1, sp, #(4*17);
	str	r1, [sp, #(4*14)];
	str	lr, [sp, #(4*15)];

	/* Came from usr mode */
100:	add	r1, sp, #(4*17);
	str	r1, [sp, #-4]!;
	mov	r0, sp;
.endm

/* Call C function to handle exception */
.macro CALL_EXCEPTION_CFUNC cfunc
	bl	\cfunc;
.endm

/* Restore registers from arch registers */
.macro PULL_REGS mode=CPSR_MODE_SUPERVISOR
	/* Get arch register pointer.
	 * r12 -> pointer to arch regs
	 */
	mov	r12, sp;

	/* Restore exception stack */ 
	ldr	sp, [r12], #4;

	/* Restore target CPSR */
	ldr	r1, [r12], #4;
	msr	spsr_cxsf, r1;

	/* If-else ladder for different target modes  */
	and	r0, r1, #CPSR_MODE_MASK;
	cmp	r0, #CPSR_MODE_USER;
	beq	200f;
	cmp	r0, #CPSR_MODE_SUPERVISOR;
	beq	201f;

	/* We handle all exceptions, irqs, and fiqs in SVC mode so,
	 * should never reach here. Just sit in infinite loop !!!!! 
	 */
	b	.;

	/* Going back to usr mode */
200:	ldr	lr, [r12, #(4*15)];
	ldmia	r12, {r0-r14}^;
	movs	pc, lr

	/* Going back to svc mode */
201:	ldm	r12, {r0-r15}^;
	mov	r0, r0;
.endm

	/*
	 * Undefined instruction exception handler.
	 */
EXCEPTION_HANDLER _undef_inst, 4
	PUSH_REGS CPSR_MODE_UNDEFINED
	CALL_EXCEPTION_CFUNC do_undef_inst
	PULL_REGS CPSR_MODE_UNDEFINED

	/*
	 * Software interrupt exception handler.
	 */
EXCEPTION_HANDLER _soft_irq, 4
	PUSH_REGS 
	CALL_EXCEPTION_CFUNC do_soft_irq
	PULL_REGS 

	/*
	 * Prefetch abort exception handler.
	 */
EXCEPTION_HANDLER _prefetch_abort, 4
	PUSH_REGS CPSR_MODE_ABORT
#if (__ARM_ARCH_VERSION__ < 6)
	ldr r1, _ifar;
	ldr lr, [r0, #(4 * 17)]
	str lr, [r1];
#endif
	CALL_EXCEPTION_CFUNC do_prefetch_abort
	PULL_REGS CPSR_MODE_ABORT

	/*
	 * Data abort exception handler.
	 */
EXCEPTION_HANDLER _data_abort, 8
	PUSH_REGS CPSR_MODE_ABORT
#if (__ARM_ARCH_VERSION__ < 6)
	ldr r1, _abort_inst;
	ldr lr, [r0, #(4 * 17)]
	str lr, [r1];
#endif
	CALL_EXCEPTION_CFUNC do_data_abort
	PULL_REGS CPSR_MODE_ABORT

	/*
	 * Not used exception handler.
	 */
EXCEPTION_HANDLER _not_used, 4
	PUSH_REGS 
	CALL_EXCEPTION_CFUNC do_not_used
	PULL_REGS 

	/*
	 * IRQ exception handler.
	 */
EXCEPTION_HANDLER _irq, 4
	PUSH_REGS CPSR_MODE_IRQ
	CALL_EXCEPTION_CFUNC do_irq
	PULL_REGS CPSR_MODE_IRQ

	/*
	 * FIQ exception handler.
	 */
EXCEPTION_HANDLER _fiq, 4
	PUSH_REGS CPSR_MODE_FIQ
	CALL_EXCEPTION_CFUNC do_fiq
	PULL_REGS CPSR_MODE_FIQ

