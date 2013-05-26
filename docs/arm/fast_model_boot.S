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
 * @file fast_model_boot.S
 * @author Anup Patel (anup@brainfault.org)
 * @brief light-weight boot-wrapper for ARM fast models
 */

/** 
 * The code in fast_model_boot.S is an adapted version of boot.S from:
 * git://github.com/virtualopensystems/boot-wrapper.git
 * 
 * boot.S - simple register setup code for stand-alone Linux booting
 *
 * Copyright (C) 2011 ARM Limited. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE.txt file.
 */

	/* Boot-wrapper entry point */
	.section .text, "ax", %progbits
	.globl	start_boot
start_boot:
	/* Setup generic timer cntfrq */
	b	__gentimer_freq_next
__gentimer_freq:
	.word	100000000
__gentimer_freq_next:
	ldr	r1, __gentimer_freq
	mcr     p15, 0, r1, c14, c0, 0

	/* GIC Distributor Interface Init */
	mrc	p15, 0, r4, c0, c0, 5		/* MPIDR (ARMv7 only) */
	and	r4, r4, #0xF			/* CPU number */
	ldr	r0, __dist_gic_base		/* Dist GIC base */
	b 	__dist_gic_base_next
__dist_gic_base:
#ifdef VEXPRESS_A15
	.word	0x2c001000
#else
	.word	0x00000000
#endif
__dist_gic_base_next:
	ldr	r1, [r0, #0x04]			/* Type Register */
	cmp	r4, #0
	andeq	r1, r1, #0x1f
	movne	r1, #0
	add	r2, r0, #0x080			/* Security Register 0 */
	mvn	r3, #0
2:	str	r3, [r2]
	sub	r1, r1, #1
	add	r2, r2, #4			/* Next security register */
	cmp	r1, #-1
	bne	2b

	/* Set NSACR to allow coprocessor access from non-secure */
	mrc	p15, 0, r0, c1, c1, 2
	ldr	r1, __nsacr_val
	b	__nsacr_val_next
__nsacr_val:
	.word	0x43fff
__nsacr_val_next:
	orr	r0, r0, r1
	mcr	p15, 0, r0, c1, c1, 2

	/* GIC Secured CPU Interface Init */
	ldr	r0, __cpu_gic_base		/* GIC CPU base */
	b 	__cpu_gic_base_next
__cpu_gic_base:
#ifdef VEXPRESS_A15
	.word	0x2c002000
#else
	.word	0x00000000
#endif
__cpu_gic_base_next:
	mov	r1, #0x80
	str	r1, [r0, #0x4]			/* GIC CPU Priority Mask */

	/* Enter hypervisor mode */
	bl	enter_hvc_mode

	/* GIC Non-Secured CPU Interface Init */
	ldr	r0, __cpu_gic_base
	mov	r1, #0x1
	str	r1, [r0]			/* GIC CPU Control */

	/* Skip secondary loop for Primary core */
	mrc	p15, 0, r0, c0, c0, 5		/* MPIDR (ARMv7 only) */
	and	r0, r0, #15			/* CPU number */
	cmp	r0, #0				/* primary CPU? */
	beq	secondary_loop_skip

	/* Secondary CPUs (following the SMP booting protocol) */
	ldr	r1, __spin_loop_addr
	b 	__spin_loop_addr_next
__spin_loop_addr:
	.word	SPIN_LOOP_ADDR
__spin_loop_addr_next:
	adr	r2, secondary_loop
	ldmia	r2, {r3 - r7}			/* Move the code to a location */
	stmia	r1, {r3 - r7}			/* less likely to be overridden */
	ldr	r0, __sys_flags_addr
	mov	pc, r1				/* Branch to the relocated code */
__sys_flags_addr:
#ifdef VEXPRESS_A15
	.word	0x1c010030			/* VExpress SYS_FLAGS */
#else
	.word	0x00000000
#endif
__sys_flags_addr_next:
secondary_loop:
	wfi
	ldr	r1, [r0]
	cmp	r1, #0
	beq	secondary_loop
	mov	pc, r1
secondary_loop_skip:

	/* Jump to input binary */
	mov	r0, #0
	mov	r1, #0
	/* deliberately put a non-4B aligned value to r2 skip dtb checking */
	mov	r2, #1
	b	input_bin

	/* 
	 * Function to enable hypervisor mode.
	 * Note: This function should be called before enabling MMU
	 */
	.globl enter_hvc_mode
enter_hvc_mode:
	/* Save lr and jump to hvc_start */
	mov	r12, lr
	b	enter_hvc_mode_start
	.balign	256
hvc_texec_table:
	b	.
	b	.
	b	hvc_tmon_call
	b	.
	b	.
	b	hvc_thyp_call
	b	.
	b	.
__hvc_texec_table:
	.word hvc_texec_table
hvc_tmon_call:
	/* Get secure configuration register */
	mrc	p15, 0, r0, c1, c1, 0	
	bic	r0, r0, #0x07f
	mov	r1, #0x10
	lsl	r1, r1, #4
	orr	r1, r1, #0x1
	orr	r0, r0, r1
	mcr	p15, 0, r0, c1, c1, 0
	/* Get temp hypervisor vector base address */
	ldr	r0, __hvc_texec_table
	mcr	p15, 4, r0, c12, c0, 0
	movs	pc, lr
hvc_thyp_call:
	/* Yeppie! in hypervisor mode */
	mrs	r0, cpsr_all
	msr	spsr_hyp, r0
	msr	elr_hyp, r12
	eret
enter_hvc_mode_start:
	/* Get temp monitor vector base address */
	ldr	r0, __hvc_texec_table
	/* Set temp monitor vector base address */
	mcr	p15, 0, r0, c12, c0, 1
	/* Call temp monitor mode code (returns in non-secure SVC mode) */
	smc	#0
	/* Call temp hypervisor mode code (never returns) */
	hvc	#0
	/* Hang! Not supposed to reach here */
	b	.

	/* Input binary containing OS images */
	.section .text, "ax", %progbits
	.globl	input_bin
	.balign 0x10000
input_bin:
	.incbin	"fast_model_boot.img"
	.globl	input_bin_end
input_bin_end:
