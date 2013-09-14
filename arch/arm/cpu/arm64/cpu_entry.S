/**
 * Copyright (c) 2013 Sukanto Ghosh.
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
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief entry points (booting, reset, exceptions) for ARMv8 family
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
	/*
	 * x4 -> load start
	 * x5 -> load end
	 * x6 -> execution start
	 * x7 -> execution end
	 */
	adr	x4, .		/* x4 <- pc */
	ldr	x6, __exec_start
	ldr	x7, __exec_end
	sub	x3, x7, x6
	add	x5, x4, x3

	/* Save boot reg0 (i.e. x0) */
	ldr	x3, __boot_reg0
	sub	x3, x3, x6
	add	x3, x3, x4
	str	x0, [x3]

	/* Save load start and load end addresses */
	ldr	x0, __load_start
	sub	x0, x0, x6
	add	x0, x0, x4
	str	x4, [x0]
	ldr	x0, __load_end
	sub	x0, x0, x6
	add	x0, x0, x4
	str	x5, [x0]

	/* Hang if execution start is not 4 KB aligned */
	movn	x3, #0xfff
	mov	x0, x6
	ands	x0, x0, x3
	cmp	x0, x6
	blt	_start_hang

	/* Hang if execution end is not 4 KB aligned */
	mov	x0, x7
	ands	x0, x0, x3
	cmp	x0, x7
	blt	_start_hang

	/* Ensure that we are in hypervisor mode */
	mrs	x0, currentEL
	cmp	w0, #PSR_MODE64_EL2t
	bne	_start_hang

	/*
	 * Setup Hypervisor System Control Register
	 * (we need to at least clear the A bit for 
	 *  _copy routine to work properly)
	 */
	ldr	x2, __hsctlr_clear
	mrs    	x3, sctlr_el2
	and	x3, x3, x2
	msr     sctlr_el2, x3
	dsb	sy
	isb

	/* Zero-out bss section */
	ldr	x1, __bss_start
	sub	x1, x1, x6
	add	x1, x1, x4
	ldr	x2, __bss_end
	sub	x2, x2, x6
	add	x2, x2, x4
_bss_zero:
	str	xzr, [x1], #8
	cmp	x1, x2
	blt	_bss_zero

	/* Determine if a valid external dtb is provided */
	/* Get DTB address */
	ldr     x3, __boot_reg0
	sub     x3, x3, x6
	add     x3, x3, x4
	ldr     x2, [x3]

	/* First check for 8B alignment */
	tst	x2, #0x7
	bne	align_4k_boundary

	/* Now check for FDT_MAGIC */
	ldr	w0, [x2]
	ldr	x1, __fdt_magic
	cmp	w0, w1
	bne	align_4k_boundary

	/* Now get the dtb total-size */
	ldr	w0, [x2, #4]
	rev	w0, w0		/* Byte-swap as FDt is in big-endian */

	/* Compare with max supported dtb size */
	ldr	x1, __max_dtb_size
	cmp	x0, x1
	bgt	align_4k_boundary

	/* Overwrite the built-in fdt with the one passed */
	ldr	x1, __builtin_dtb
	sub	x1, x1, x6
	add	x1, x1, x4

	/* x0 = dtb total size */
	/* x1 = builtin_dtb load-address */
	/* x2 = passed dtb address */

dtb_save_loop:
	ldr	x3, [x2], #8
	str	x3, [x1], #8
	subs	x0, x0, #8
	bgt	dtb_save_loop

align_4k_boundary:
	/* Relocate code if load start is not 4 KB aligned */
	mov	x0, x4
	movn	x3, #0xfff
	ands	x0, x0, x3
	cmp	x0, x4
	b.eq	_start_mmu_init

_start_relocate:
	/* Relocate copy function at end after load end address */
	ldr	x0, __copy_start
	ldr	x1, __copy_end
	sub	x2, x1, x0
	sub	x0, x0, x6
	add	x0, x0, x4
	mov	x1, x0
	mov	x0, x5
	bl	_copy
	/*
	 * Use newly relocated copy function to relocate entire code
	 */
	mov	x1, x4
	mov	x0, x5
	sub	x2, x5, x4
	mov	x0, x4
	movn	x3, #0xfff
	ands	x0, x0, x3
	bl	_start_nextpc1
_start_nextpc1:
	add	x30, x30, #16
	sub	x30, x30, x4
	add	x30, x30, x0
	br	x5
	/*
	 * Update load start and load end
	 * x4 -> new load start
	 * x5 -> new load end
	 */
	mov	x0, x4
	movn	x3, #0xfff
	ands	x0, x0, x3
	sub	x1, x4, x0
	sub	x4, x4, x1
	sub	x5, x5, x1
	ldr	x0, __load_start
	sub	x0, x0, x6
	add	x0, x0, x4
	str	x4, [x0]
	ldr	x0, __load_end
	sub	x0, x0, x6
	add	x0, x0, x4
	str	x5, [x0]

_start_mmu_init:
	/* Setup SP as-per load address */
	ldr	x0, __hvc_stack_end
	mov	sp, x0
	sub	sp, sp, x6
	add	sp, sp, x4

	/* Saving x4 .. x7 in callee-saved regs */
	mov	x21, x4
	mov	x22, x5
	mov	x23, x6
	mov	x24, x7

	/* Setup arguments and call setup_initial_ttbl */
	mov	x0, x4
	mov	x1, x5
	mov	x2, x6
	mov	x3, x7

	bl	_setup_initial_ttbl

	/* Restoring x4 .. x7 from callee-saved regs */
	mov	x4, x21
	mov	x5, x22
	mov	x6, x23
	mov	x7, x24

	/* Update the httbr value */
	ldr	x1, __httbr_set_addr
	sub	x1, x1, x6
	add	x1, x1, x4
	ldr	x0, [x1]
	sub	x0, x0, x6
	add	x0, x0, x4
	str	x0, [x1]
	b	_common_init

#ifdef CONFIG_SMP
	.align	3
1:	.dword	.
	.dword	secondary_holding_pen_release
__leds_base:
	.dword	0x1c010008

#define MPIDR_HWID_BITMASK	0xff00ffffff

	/*
	 * Secondary CPU startup code 
	 */
_start_secondary:
	/*
	 * This provides a "holding pen" for platforms to hold all secondary
	 * cores are held until we're ready for them to initialise.
	 */
	ldr	x15, __leds_base
	mov	w14, #0
	str	w14, [x15]

	mrs	x0, mpidr_el1
	ldr     x1, =MPIDR_HWID_BITMASK
	and	x0, x0, x1

	/* Calculate load address of secondary_holding_pen_release */
	adr	x1, 1b
	ldp	x2, x3, [x1]
	sub	x1, x1, x2
	add	x3, x3, x1
	sevl
pen:	wfe
	ldr	x4, [x3]
	cmp	x4, x0
	bne	pen
#endif

	/*
	 * Note: From this point primary CPU startup is same as secondary CPU
	 */
_common_init:
	/* Disable IRQ & FIQ */
	msr	daifset, #3

	/* Ensure that we are in hypervisor mode */
	mrs	x0, currentEL
	cmp	w0, #PSR_MODE64_EL2t
	bne	_start_hang

	/* Setup Hypervisor MAIR0 & MAIR1 */
	ldr	x0, __hmair_set
	msr	mair_el2, x0

	/* Setup Hypervisor Translation Control Register */
	ldr	x0, __htcr_set
	msr     tcr_el2, x0

	/* Setup Hypervisor Translation Base Register */
	ldr	x0, __httbr_set
	msr	ttbr0_el2, x0

	/* Setup Hypervisor Stage2 Translation Control Register */
	ldr	x0, __vtcr_set
	msr     vtcr_el2, x0

	/* Setup HCR */
	ldr	x0, __hcr_set
	msr	hcr_el2, x0

	/* Setup temporary vector containing branch to self for debug */
	ldr	x0, __vbar_val
	msr    	vbar_el2, x0

	/* Set Hypervisor Stack */
	ldr	x0,	__hvc_stack_end
#ifdef CONFIG_SMP
	mov	x1, #CONFIG_IRQ_STACK_SIZE
	mrs	x10, mpidr_el1
	ldr     x2, =MPIDR_HWID_BITMASK
	and	x10, x2, x10
	mul	x1, x1, x10
	sub	x0, x0, x1
#endif
	mov	sp, x0

	/* Setup Hypervisor System Control Register */
	ldr	x1, __hsctlr_set
	mrs	x3, sctlr_el2
	orr	x3, x3, x1
	msr     sctlr_el2, x3
	dsb	sy
	isb

	ldr	x0, __cpu_init
	blr	x0

_start_hang:
	b	.

#define FDT_MAGIC	0xedfe0dd0	/* 0xd00dfeed in big-endian */

	.align 3
__fdt_magic:
	.dword FDT_MAGIC
__builtin_dtb:
	.dword dt_blob_start
__max_dtb_size:
	.dword CONFIG_ARM_MAX_DTB_SIZE
__hmair_set:
	.dword (MAIR_INITVAL)
__htcr_set:
	.dword ((0x19 << TCR_T0SZ_SHIFT) | (0x2 << TCR_PS_SHIFT) | (0x3 << TCR_SH0_SHIFT) | (0x1 << TCR_ORGN0_SHIFT) | (0x1 << TCR_IRGN0_SHIFT))
__vtcr_set:
	.dword ((0x1 << VTCR_SL0_SHIFT) | (0x19 << VTCR_T0SZ_SHIFT) | (0x2 << VTCR_PS_SHIFT) | (0x3 << VTCR_SH0_SHIFT) | (0x1 << VTCR_ORGN0_SHIFT) | (0x1 << VTCR_IRGN0_SHIFT))
__hsctlr_clear:
	.dword ~(SCTLR_A_MASK)
__hsctlr_set:
	.dword (SCTLR_M_MASK | SCTLR_C_MASK | SCTLR_I_MASK)
__hcr_set:
	.dword (HCR_AMO_MASK | HCR_IMO_MASK | HCR_FMO_MASK | HCR_VM_MASK)
__exec_start:
	.dword _code_start
__exec_end:
	.dword _code_end
__load_start:
	.dword _load_start
__load_end:
	.dword _load_end
__bss_start:
	.dword _bss_start
__bss_end:
	.dword _bss_end
__copy_start:
	.dword _copy
__copy_end:
	.dword _copy_end
__httbr_set_addr:
	.dword __httbr_set
__httbr_set:
	.dword def_ttbl
__cpu_init:
	.dword cpu_init
__vbar_val:
	.dword vectors
__boot_reg0:
	.dword _boot_reg0

	/*
	 * Boot register 0 passed by bootloader
	 */
	.globl _boot_reg0
_boot_reg0:
	.dword 0x0

	/* 
	 * Load start address storage
	 */
	.globl _load_start
_load_start:
	.dword 0x0

	/* 
	 * Load end address storage
	 */
	.globl _load_end
_load_end:
	.dword 0x0

	/* 
	 * Exception stacks.
	 */
__hvc_stack_end:
	.dword _hvc_stack_end

	/* 
	 * Copy data from source to destination
	 * (alignment handled by hardware)
	 * Arguments:
	 *  x0 -> destination address
	 *  x1 -> source address
	 *  x2 -> byte count
	 * Return:
	 *  x0 -> bytes copied
	 */
	.section .entry, "ax", %progbits
	.globl _copy
_copy:
	mov	x4, x2
	subs	x2, x2, #8
	b.mi	2f
1:	ldr	x3, [x1], #8
	subs	x2, x2, #8
	str	x3, [x0], #8
	b.pl	1b
2:	adds	x2, x2, #4
	b.mi	3f
	ldr	w3, [x1], #4
	sub	x2, x2, #4
	str	w3, [x0], #4
3:	adds	x2, x2, #2
	b.mi	4f
	ldrh	w3, [x1], #2
	sub	x2, x2, #2
	strh	w3, [x0], #2
4:	adds	x2, x2, #1
	b.mi	5f
	ldrb	w3, [x1]
	strb	w3, [x0]
5:	mov	x0, x4
	ret
_copy_end:




/* Stack pushing/popping (register pairs only). 
   Equivalent to store decrement before, load increment after */

.macro  push, xreg1, xreg2
	stp     \xreg1, \xreg2, [sp, #-16]!
.endm

.macro  pop, xreg1, xreg2
	ldp     \xreg1, \xreg2, [sp], #16
.endm


.macro EXCEPTION_HANDLER irqname
	.align 6
\irqname:
.endm


/* Push registers on stack */
.macro	PUSH_REGS
	sub	sp, sp, #0x20		/* room for LR, SP, SPSR, ELR */
	push	x28, x29
	push	x26, x27
	push	x24, x25
	push	x22, x23
	push	x20, x21
	push	x18, x19
	push	x16, x17
	push	x14, x15
	push	x12, x13
	push	x10, x11
	push	x8, x9
	push	x6, x7
	push	x4, x5
	push	x2, x3
	push	x0, x1
	add	x21, sp, #0x110
	mrs	x22, elr_el2
	mrs	x23, spsr_el2
	stp	x30, x21, [sp, #0xF0]
	stp	x22, x23, [sp, #0x100]
	/*
	 * Registers that may be useful after this macro is invoked:
	 *
	 * x21 - aborted SP
	 * x22 - aborted PC
	 * x23 - aborted PSTATE
	 */
.endm

/* Call C function to handle exception */
.macro CALL_EXCEPTION_CFUNC cfunc
	mov	x0, sp
	bl	\cfunc
.endm

/* Pull registers from stack */
.macro	PULL_REGS
	ldp	x21, x22, [sp, #0x100]		/* load ELR, SPSR */
	msr	elr_el2, x21
	msr	spsr_el2, x22
	pop	x0, x1
	pop	x2, x3				
	pop	x4, x5
	pop	x6, x7
	pop	x8, x9
	pop	x10, x11
	pop	x12, x13
	pop	x14, x15
	pop	x16, x17
	pop	x18, x19
	pop	x20, x21
	pop	x22, x23
	pop	x24, x25
	pop	x26, x27
	pop	x28, x29
	msr	tpidr_el2, x24
	ldp	x30, x24, [sp]
	mov	sp, x24
	mrs	x24, tpidr_el2
	eret
.endm


/*
 * Exception vectors.
 */
.macro	ventry	label
	.align	7
	b	\label
.endm

	.align	11
	.globl vectors;
vectors:
	ventry	hyp_sync_invalid	/* Synchronous EL1t */
	ventry	hyp_irq_invalid		/* IRQ EL1t */
	ventry	hyp_fiq_invalid		/* FIQ EL1t */
	ventry	hyp_error_invalid	/* Error EL1t */

	ventry	hyp_sync		/* Synchronous EL1h */
	ventry	hyp_irq			/* IRQ EL1h */
	ventry	hyp_fiq_invalid		/* FIQ EL1h */
	ventry	hyp_error_invalid	/* Error EL1h */

	ventry	guest_sync_a64		/* Synchronous 64-bit EL0 */
	ventry	guest_irq_a64		/* IRQ 64-bit EL0 */
	ventry	guest_fiq_a64		/* FIQ 64-bit EL0 */
	ventry	guest_error_a64		/* Error 64-bit EL0 */

	ventry	guest_sync_a32		/* Synchronous 32-bit EL0 */
	ventry	guest_irq_a32		/* IRQ 32-bit EL0 */
	ventry	guest_fiq_a32		/* FIQ 32-bit EL0 */
	ventry	guest_error_a32		/* Error 32-bit EL0 */


EXCEPTION_HANDLER hyp_sync_invalid
	PUSH_REGS
	mov	x1, EXC_HYP_SYNC_SP0
	CALL_EXCEPTION_CFUNC do_bad_mode
	PULL_REGS

EXCEPTION_HANDLER hyp_irq_invalid
	PUSH_REGS
	mov	x1, EXC_HYP_IRQ_SP0
	CALL_EXCEPTION_CFUNC do_bad_mode
	PULL_REGS

EXCEPTION_HANDLER hyp_fiq_invalid
	PUSH_REGS
	mov	x1, EXC_HYP_FIQ_SP0
	CALL_EXCEPTION_CFUNC do_bad_mode
	PULL_REGS

EXCEPTION_HANDLER hyp_error_invalid
	PUSH_REGS
	mov	x1, EXC_HYP_SERROR_SP0
	CALL_EXCEPTION_CFUNC do_bad_mode
	PULL_REGS

EXCEPTION_HANDLER hyp_sync
	PUSH_REGS
	mov	x1, EXC_HYP_SYNC_SPx
	CALL_EXCEPTION_CFUNC do_sync
	PULL_REGS

EXCEPTION_HANDLER hyp_irq
	PUSH_REGS
	mov	x1, EXC_HYP_IRQ_SPx
	CALL_EXCEPTION_CFUNC do_irq
	PULL_REGS

EXCEPTION_HANDLER hyp_fiq
	PUSH_REGS
	mov	x1, EXC_HYP_FIQ_SPx
	CALL_EXCEPTION_CFUNC do_bad_mode
	PULL_REGS

EXCEPTION_HANDLER hyp_error
	PUSH_REGS
	mov	x1, EXC_HYP_SERROR_SPx
	CALL_EXCEPTION_CFUNC do_bad_mode
	PULL_REGS

EXCEPTION_HANDLER guest_sync_a64
	PUSH_REGS
	mov	x1, EXC_GUEST_SYNC_A64
	CALL_EXCEPTION_CFUNC do_sync
	PULL_REGS

EXCEPTION_HANDLER guest_irq_a64
	PUSH_REGS
	mov	x1, EXC_GUEST_IRQ_A64
	CALL_EXCEPTION_CFUNC do_irq
	PULL_REGS

EXCEPTION_HANDLER guest_fiq_a64
	PUSH_REGS
	mov	x1, EXC_GUEST_FIQ_A64
	CALL_EXCEPTION_CFUNC do_bad_mode
	PULL_REGS

EXCEPTION_HANDLER guest_error_a64
	PUSH_REGS
	mov	x1, EXC_GUEST_SERROR_A64
	CALL_EXCEPTION_CFUNC do_bad_mode
	PULL_REGS

EXCEPTION_HANDLER guest_sync_a32
	PUSH_REGS
	mov	x1, EXC_GUEST_SYNC_A32
	CALL_EXCEPTION_CFUNC do_sync
	PULL_REGS

EXCEPTION_HANDLER guest_irq_a32
	PUSH_REGS
	mov	x1, EXC_GUEST_IRQ_A32
	CALL_EXCEPTION_CFUNC do_irq
	PULL_REGS

EXCEPTION_HANDLER guest_fiq_a32
	PUSH_REGS
	mov	x1, EXC_GUEST_FIQ_A32
	CALL_EXCEPTION_CFUNC do_bad_mode
	PULL_REGS

EXCEPTION_HANDLER guest_error_a32
	PUSH_REGS
	mov	x1, EXC_GUEST_SERROR_A32
	CALL_EXCEPTION_CFUNC do_bad_mode
	PULL_REGS

