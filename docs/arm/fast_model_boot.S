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
#ifdef SMP
	/* CPU initialisation */
	mrc	p15, 0, r4, c0, c0, 5		/* MPIDR (ARMv7 only) */
	and	r4, r4, #15			/* CPU number */
	cmp	r4, #0
	bne	skip_gic_dist_init

	/* TrustZone initialization */
	ldr	r0, __dist_gic_base		/* Dist GIC base */
	b 	__dist_gic_base_next
__dist_gic_base:
	.word	0x2c001000
__dist_gic_base_next:
	ldr	r1, [r0, #0x04]			/* Type Register */
	andeq	r1, r1, #0x1f
	movne	r1, #0
	add	r2, r0, #0x080			/* Security Register 0 */
#ifdef GIC_DIST_NONSECURED
	/* Set all interrupts to be non-secure */
	mvn	r3, #0
#else
	/* Set all interrupts to be secure */
	mov	r3, #0
#endif
2:	str	r3, [r2]
	sub	r1, r1, #1
	add	r2, r2, #4			/* Next security register */
	cmp	r1, #-1
	bne	2b
skip_gic_dist_init:

	/* Set GIC priority mask bit [7] = 1 */
	ldr	r0, __cpu_gic_base		/* CPU GIC base */
	b 	__cpu_gic_base_next
__cpu_gic_base:
	.word	0x2c002000
__cpu_gic_base_next:
	mov	r1, #0x80
	str	r1, [r0, #0x4]			/* GIC ICCPMR */

	/* Set NSACR to allow coprocessor access from non-secure */
	mrc	p15, 0, r0, c1, c1, 2
	ldr	r1, __nsacr_val
	b	__nsacr_val_next
__nsacr_val:
	.word	0x43fff
__nsacr_val_next:
	orr	r0, r0, r1
	mcr	p15, 0, r0, c1, c1, 2

	/* Check CPU nr again */
	mrc	p15, 0, r0, c0, c0, 5		/* MPIDR (ARMv7 only) */
	and	r0, r0, #15			/* CPU number */
	cmp	r0, #0				/* primary CPU? */
	beq	2f

	/* Secondary CPUs (following the RealView SMP booting protocol) */
	ldr	r0, __sys_flags_addr
	b	__sys_flags_addr_next
__sys_flags_addr:
#ifdef VEXPRESS_A15
	.word	0x1c010030			/* VExpress SYS_FLAGS */
#else
	.word	0x1c010030			/* RealView SYS_FLAGS */
#endif
__sys_flags_addr_next:
1:
#ifdef VEXPRESS_A15
	wfe
#endif
	ldr	r1, [r0]
	cmp	r1, #0
	beq	1b
	mov	pc, r1				/* branch to given address */
#endif

2:
	/* UART initialisation (38400 8N1) */
	ldr	r0, __uart_base
	b	__uart_base_next
__uart_base:
#ifdef MACH_MPS
	.word	0x1f005000			/* UART3 base (MPS) */
#elif defined (VEXPRESS_A15)
	.word	0x1c090000			/* UART base (VExpress) */
#else
	.word	0x10009000			/* UART base (RealView/EB) */
#endif
__uart_base_next:
	mov	r1, #0x10			/* ibrd */
	str	r1, [r0, #0x24]
	mov	r1, #0xc300
	orr	r1, #0x0001			/* cr */
	str	r1, [r0, #0x30]

	/* Setup generic timer cntfrq */
	ldr	r1, _gentimer_freq
	mcr     p15, 0, r1, c14, c0, 0

	/* Jump to input binary */
	b	input_bin

_gentimer_freq:
	.word	100000000


	/* Input binary containing OS images */
	.section .text, "ax", %progbits
	.globl	input_bin
	.balign 0x10000
input_bin:
	.incbin	"fast_model_boot.img"
	.globl	input_bin_end
input_bin_end:
