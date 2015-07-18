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
	 * _start_secondary_nopen: Secondary CPU startup code without holding pen
	 *
	 * Note: Xvisor could be loaded any where in memory by boot loaders.
	 * The _start ensures that Xvisor exectues from intended
	 * base address provided at compile time.
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
	b	_start_secondary_nopen

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
#ifdef CONFIG_SMP
	/* Setup SMP ID for current processor */
	bl	proc_setup_smp_id
#endif
	/* Setup Hypervisor System Control Register */
	bl	proc_setup
	dsb
	isb
	mcr     p15, 4, r0, c1, c0, 0
	dsb
	isb
	/* Jump to reset code */
	ldr	pc, __reset

_start_hang:
	b	.

#define FDT_MAGIC	0xedfe0dd0	/* 0xd00dfeed in big-endian */

__fdt_magic:
	.word FDT_MAGIC
__builtin_dtb:
	.word dt_blob_start
__max_dtb_size:
	.word CONFIG_ARM_MAX_DTB_SIZE
__hmair0_set:
	.word (HMAIR0_INITVAL)
__hmair1_set:
	.word (HMAIR1_INITVAL)
__htcr_clear:
	.word ~(HTCR_T0SZ_MASK | \
		HTCR_ORGN0_MASK | \
		HTCR_IRGN0_MASK | \
		HTCR_SH0_MASK)
__htcr_set:
	.word ((0x1 << HTCR_ORGN0_SHIFT) | \
	       (0x1 << HTCR_IRGN0_SHIFT) | \
	       (0x3 << HTCR_SH0_SHIFT))
__httbr_set:
	.word def_ttbl
__vtcr_clear:
	.word ~(VTCR_T0SZ_MASK | \
		VTCR_S_MASK | \
		VTCR_SL0_MASK | \
		VTCR_ORGN0_MASK | \
		VTCR_IRGN0_MASK | \
		VTCR_SH0_MASK)
__vtcr_set:
	.word ((VTCR_T0SZ_VAL(39)) | \
	       (VTCR_S_VAL(39)) | \
	       (VTCR_SL0_L1) | \
	       (0x1 << VTCR_ORGN0_SHIFT) | \
	       (0x1 << VTCR_IRGN0_SHIFT) | \
	       (0x3 << VTCR_SH0_SHIFT))

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
	.word HCR_DEFAULT_BITS
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
	bl	arch_smp_id
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

