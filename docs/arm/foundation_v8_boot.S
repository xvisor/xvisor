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
 * @file foundation_v8_boot.S
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @author Anup Patel (anup@brainfault.org)
 * @brief light-weight boot-wrapper for ARMv8 foundation models
 */

#define PSR_MODE64_EL3			0x0000000c
#define PSR_MODE64_EL2h			0x00000009
#define PSR_MODE64_MASK			0x0000001f
#define PSR_FIQ_DISABLED		(1 << 6)
#define PSR_IRQ_DISABLED		(1 << 7)
#define PSR_ASYNC_ABORT_DISABLED	(1 << 8)
#define PSR_MODE64_DEBUG_DISABLED	(1 << 9)

	/* Boot-wrapper entry point */
	.section .text, "ax", %progbits
	.globl	_start
_start:
	/* Assume EL2 mode if not in EL3 mode */
	mrs	x0, CurrentEL
	cmp	x0, #PSR_MODE64_EL3
	b.ne	_start_el2

	/* Setup EL3 security control register */
	mov	x0, #0x30			/* RES1 */
	orr	x0, x0, #(1 << 0)		/* Non-secure EL1 */
	orr	x0, x0, #(1 << 8)		/* HVC enable */
	orr	x0, x0, #(1 << 10)		/* 64-bit EL2 */
	msr	scr_el3, x0

	/* Disable copro. traps to EL3 */
	msr	cptr_el3, xzr

	/* Setup generic timer cntfrq */
	ldr	x0, __gentimer_freq
	msr	cntfrq_el0, x0
	b	_gentimer_done
	.align 3
__gentimer_freq:
	.dword GENTIMER_FREQ
_gentimer_done:

	/* GIC Secured Distributor Interface Init */
	mrs	x4, mpidr_el1
	ldr	x5, __mpidr_mask
	and	x4, x4, x5			/* CPU affinity */
_gic_dist_init:
	ldr	x0, __gic_dist_base		/* Dist GIC base */
	mov	x1, #0				/* non-0 cpus should at least */						
	cmp	x4, xzr				/* program IGROUP0 */
	bne	1f
	mov	x1, #3				/* Enable group0 & group1 */
	str	w1, [x0, #0x00]			/* Ctrl Register */
	ldr	w1, [x0, #0x04]			/* Type Register */
1:	and	x1, x1, #0x1f			/* No. of IGROUPn registers */
	add	x2, x0, #0x080			/* IGROUP0 Register */
	movn	x3, #0				/* All interrupts to group-1 */
2:	str	w3, [x2], #4
	subs	x1, x1, #1
	bge	2b
	/* GIC Secured CPU Interface Init */
_gic_cpu_init:
	ldr	x0, __gic_cpu_base		/* GIC CPU base */
	mov	x1, #0x80
	str	w1, [x0, #0x4]			/* GIC CPU Priority Mask */
	mov	x1, #0x3			/* Enable group0 & group1 */
	str	w1, [x0]			/* GIC CPU Control */
	b	_gic_init_done
	.align 3
__gic_dist_base:
	.dword GIC_DIST_BASE
__gic_cpu_base:
	.dword GIC_CPU_BASE
_gic_init_done:

_switch_el3_to_el2:
	/* Clear EL2 control register */
	msr	sctlr_el2, xzr
	/* Prepare the switch to EL2 mode from EL3 mode */
	ldr	x0, __start_el2			/* Return after mode switch */
	mov	x1, #0x3c9			/* EL2_SP1 | D | A | I | F */
	msr	elr_el3, x0
	msr	spsr_el3, x1
	eret
	.align 3
__start_el2:
	.dword _start_el2

_start_el2:
	/* Skip secondary spin loop for primary core */
	mrs	x4, mpidr_el1
	ldr	x5, __mpidr_mask
	ands	x4, x4, x5
	beq	__secondary_spin_code_end
	/* Copy the secondary_spin(start, end) to SPIN_LOOP_ADDR */
	adr	x0, __secondary_spin_code_start
	adr	x1, __secondary_spin_code_end
	ldr	x2, __secondary_spin_loc
1:	ldr	x4, [x0], #8
	str	x4, [x2], #8
	cmp	x1, x0
	bge	1b
	/* Jump to copied over spin loop */
	adr	x3, __secondary_spin
	sub	x3, x3, x0
	add	x3, x3, x2
	br	x3
	/* Secondary spin loop */
	.align 3
__secondary_spin_code_start:
	.dword	0x0
__secondary_spin_loc:
	.dword	SPIN_LOOP_ADDR
__mpidr_mask:
	.dword	0xff00ffffff
__secondary_spin:
	ldr	x0, __secondary_spin_loc
	sevl
1:	wfe
	ldr	x1, [x0]
	cmp	x1, xzr
	beq	1b
	br	x1
__secondary_spin_code_end:

	/* UART initialisation */
_uart_init:
#define UART_PL011_DR                   0x00	 /*  Data read or written from the interface. */
#define UART_PL011_FR                   0x18	 /*  Flag register (Read only). */
#define UART_PL011_IBRD                 0x24
#define UART_PL011_CR                   0x30
#define UART_PL011_FR_TXFF              0x20
#define UART_PL011_CR_CTSEN             (1 << 15)
#define UART_PL011_CR_RTSEN             (1 << 14)
#define UART_PL011_CR_RXE               (1 << 9)
#define UART_PL011_CR_TXE               (1 << 8)
#define UART_PL011_CR_UARTEN            (1 << 0)
	ldr	x4, __uart_base			/* UART base */
	ldr	x5, __uart_baud_magic
	str	w5, [x4, #UART_PL011_IBRD]	/* IBRD */
	mov	w5, #(UART_PL011_CR_CTSEN|UART_PL011_CR_RTSEN|UART_PL011_CR_RXE|UART_PL011_CR_TXE)
	orr	w5, w5, #UART_PL011_CR_UARTEN	/* CR */
	str	w5, [x4, #UART_PL011_CR]
	b	_uart_init_done
_uart_puts:
	ldrb	w5, [x0], #1
	cmp	w5, #0
	beq	_uart_puts_done
	ldr	x4, __uart_base			/* UART base */
_uart_puts_check:
	ldr	w6, [x4, #UART_PL011_FR]	/* FR */
	and	x6, x6, #UART_PL011_FR_TXFF	/* FR.TXFF */
	cmp	x6, #UART_PL011_FR_TXFF
	beq	_uart_puts_check
	str	w5, [x4, #UART_PL011_DR]	/* DR */
	b	_uart_puts
_uart_puts_done:
	ret
	.align 3
__uart_base:
	.dword UART_BASE
__uart_baud_magic:
#ifdef UART_BAUD_MAGIC
	.dword UART_BAUD_MAGIC
#else
	.dword 0x10
#endif
_uart_init_done:

	/* Print banner */
	adr	x0, __banner
	bl	_uart_puts
	/* Jump to input kernel if available as-per Linux booting protocol */
#ifdef DTB
	/* If dtb provided load the address where we placed it */
	ldr	x0, __dtb_addr
#else
	/* Deliberately put a non-8B aligned value to x0 skip dtb checking */
	mov	x0, #1
#endif
	mov	x1, #0
	mov	x2, #0
	mov	x3, #0
#ifdef IMAGE
	/* Jump to input binary */
	b	input_bin
#else
	/* Just hang */
	b	.
#endif
	.align 3
__banner:
	.ascii "Boot-wrapper initialization done.\r\nJumping to input kernel...\r\n"
	.align 3
#ifdef DTB
__dtb_addr:
	.dword	dtb
#endif

#define	str(s)		#s
#define	stringify(s)	str(s)

	.section .text, "ax", %progbits
#ifdef DTB
	/* DTB binary */
	.globl	dtb
	.balign 0x1000
dtb:
	.incbin	stringify(DTB)
#endif
#ifdef IMAGE
	/* Input binary containing OS images */
	.globl	input_bin
	.balign 0x80000
input_bin:
	.incbin	stringify(IMAGE)
	.globl	input_bin_end
input_bin_end:
#endif
#ifdef INITRD
	/* INITRD binary */
	.globl	initrd
	.balign 0x1000000
initrd:
	.incbin	stringify(INITRD)
	.globl	initrd_end
initrd_end:
#endif
