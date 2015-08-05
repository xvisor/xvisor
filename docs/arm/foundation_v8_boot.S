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
	/* UART initialisation */
	mrs	x0, mpidr_el1
	ldr	x1, __mpidr_mask
	tst	x0, x1
	b.ne	_uart_init_done			/* Secondary CPU */
#if defined(UART_PL011)
_uart_init:
	#define UART_PL011_DR			0x00	 /* Data read or written from the interface. */
	#define UART_PL011_FR			0x18	 /* Flag register (Read only). */
	#define UART_PL011_IBRD			0x24
	#define UART_PL011_CR			0x30
	#define UART_PL011_FR_TXFF		0x20
	#define UART_PL011_CR_CTSEN		(1 << 15)
	#define UART_PL011_CR_RTSEN		(1 << 14)
	#define UART_PL011_CR_RXE		(1 << 9)
	#define UART_PL011_CR_TXE		(1 << 8)
	#define UART_PL011_CR_UARTEN		(1 << 0)
	ldr	x4, __uart_pl011_base		/* UART base */
	ldr	x5, __uart_pl011_ibrd
	str	w5, [x4, #UART_PL011_IBRD]	/* IBRD */
	mov	w5, #(UART_PL011_CR_CTSEN|UART_PL011_CR_RTSEN|UART_PL011_CR_RXE|UART_PL011_CR_TXE)
	orr	w5, w5, #UART_PL011_CR_UARTEN	/* CR */
	str	w5, [x4, #UART_PL011_CR]
	b	_uart_init_done
_uart_puts:
	ldrb	w5, [x0], #1
	cmp	w5, #0
	beq	_uart_puts_done
	ldr	x4, __uart_pl011_base		/* UART base */
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
__uart_pl011_base:
	.dword UART_PL011_BASE
__uart_pl011_ibrd:
	#ifdef UART_PL011_IBRD
	.dword UART_PL011_IBRD
	#else
	.dword 0x10
	#endif
#elif defined(UART_8250_32BIT)
_uart_init:
	#define UART_RBR_OFFSET		(0*4)	/* In:	Recieve Buffer Register */
	#define UART_THR_OFFSET		(0*4)	/* Out: Transmitter Holding Register */
	#define UART_DLL_OFFSET		(0*4)	/* Out: Divisor Latch Low */
	#define UART_IER_OFFSET		(1*4)	/* I/O: Interrupt Enable Register */
	#define UART_DLM_OFFSET		(1*4)	/* Out: Divisor Latch High */
	#define UART_FCR_OFFSET		(2*4)	/* Out: FIFO Control Register */
	#define UART_LCR_OFFSET		(3*4)	/* Out: Line Control Register */
	#define UART_MCR_OFFSET		(4*4)	/* Out: Modem Control Register */
	#define UART_LSR_OFFSET		(5*4)	/* In:	Line Status Register */
	#define UART_SCR_OFFSET		(7*4)	/* I/O: Scratch Register */
	#define UART_LSR_THRE		0x20	/* Transmit-hold-register empty */
	ldr	x4, __uart_8250_base		/* UART base */
	mov	w5, #0x80
	str	w5, [x4, #UART_LCR_OFFSET]	/* set DLAB bit */
	ldr	x5, __uart_8250_dll
	str	w5, [x4, #UART_DLL_OFFSET]	/* set baudrate divisor */
	ldr	x5, __uart_8250_dlm
	str	w5, [x4, #UART_DLM_OFFSET]	/* set baudrate divisor */
	mov	w5, #0x03
	str	w5, [x4, #UART_LCR_OFFSET]	/* clear DLAB; set 8 bits, no parity */
	mov	w5, #0x01
	str	w5, [x4, #UART_FCR_OFFSET]	/* enable FIFO */
	mov	w5, #0x00
	str	w5, [x4, #UART_MCR_OFFSET]	/* no modem control DTR RTS */
	ldr	w5, [x4, #UART_LSR_OFFSET]	/* clear line status */
	ldr	w5, [x4, #UART_RBR_OFFSET]	/* read receive buffer */
	mov	w5, #0x00
	str	w5, [x4, #UART_SCR_OFFSET]	/* set scratchpad */
	mov	w5, #0x00
	str	w5, [x4, #UART_IER_OFFSET]	/* set scratchpad */
	b	_uart_init_done
	.align 3
__uart_8250_base:
	.dword UART_8250_BASE
__uart_8250_dll:
	#ifdef UART_8250_DLL
	.dword UART_8250_DLL
	#else
	.dword 0x10
	#endif
__uart_8250_dlm:
	#ifdef UART_8250_DLM
	.dword UART_8250_DLM
	#else
	.dword 0x1
	#endif
_uart_puts:
	ldrb	w5, [x0], #1
	cmp	w5, #0
	beq	_uart_puts_done
	ldr	x4, __uart_8250_base		/* UART base */
_uart_puts_check:
	ldr	w6, [x4, #UART_LSR_OFFSET]	/* LSR */
	and	x6, x6, #UART_LSR_THRE		/* LSR.THRE */
	cmp	x6, #UART_LSR_THRE
	bne	_uart_puts_check
	str	w5, [x4, #UART_THR_OFFSET]	/* THR */
	b	_uart_puts
_uart_puts_done:
	ret
#else
_uart_init:
	b	_uart_init_done
_uart_puts:
	ret
#endif
_uart_init_done:

	/* Print first banner */
	mrs	x0, mpidr_el1
	ldr	x1, __mpidr_mask
	tst	x0, x1
	b.ne	_first_banner_done		/* Secondary CPU */
_first_banner:
	adr	x0, __banner_first
	bl	_uart_puts
_first_banner_done:

	/* Relocate to execution address */
	adr	x10, _start
	ldr	x11, __reloc_start
	ldr	x12, __reloc_end
	cmp	x10, x11
	beq	_reloc_done
	mrs	x14, mpidr_el1
	ldr	x15, __mpidr_mask
	tst	x14, x15
	b.ne	_reloc_jump			/* Secondary CPU */
	adr	x0, __banner_reloc_start
	bl	_uart_puts
1:
	ldp	x13, x14, [x10], #16
	stp	x13, x14, [x11], #16
	and	x13, x10, #0xFFF
	cmp	x13, #0
	b.ne	2f
	adr	x0, __banner_reloc_busy
	bl	_uart_puts
2:
	cmp	x11, x12
	b.lt	1b
	adr	x0, __banner_reloc_end
	bl	_uart_puts
_reloc_jump:
	ldr	x10, __reloc_done
	br	x10
	.align 3
__reloc_start:
	.dword _start
__reloc_end:
	.dword _the_end
__reloc_done:
	.dword _reloc_done
_reloc_done:

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

	/* Print timer banner */
	mrs	x0, mpidr_el1
	ldr	x1, __mpidr_mask
	tst	x0, x1
	b.ne	_timer_banner_done	/* Secondary CPU */
_timer_banner:
	adr	x0, __banner_timer
	bl	_uart_puts
_timer_banner_done:

#ifdef GICv3
	/* GICv3 secured distributor interface init
	 * Note: Only the primary CPU setups the distributor interface.
	 */
	mrs	x0, mpidr_el1
	ldr	x1, __mpidr_mask
	tst	x0, x1
	b.ne	_gic_rdist_init			/* Secondary CPU */
_gic_dist_init:
	ldr	x1, __gic_dist_base
	mov	w0, #7				/* EnableGrp0 | EnableGrp1ns | EnableGrp1s */
	orr	w0, w0, #(3 << 4)		/* ARE_S | ARE_NS */
	str	w0, [x1]			/* GICD_CTLR */
	ldr	w2, [x1, #4]			/* GICD_TYPER */
	and	w2, w2, #0x1f			/* ITLinesNumber */
	cbz	w2, _gic_rdist_init
	add	x3, x1, #0x84			/* GICD_IGROUP1 */
	add	x4, x1, #0xD04			/* GICD_IGRPMOD1 */
	mvn	w5, wzr
1:	str	w5, [x3], #4
	str	wzr, [x4], #4
	sub	w2, w2, #1
	cbnz	w2, 1b
	/* GICv3 secured Redistributor interface init */
_gic_rdist_init:
	ldr	x2, __gic_rdist_base
1:	mrs	x4, mpidr_el1
	lsr	x3, x4, #32
	bfi	x4, x3, #24, #8			/* w4 is aff3:aff2:aff1:aff0 */
	ldr	x5, [x2, #8]			/* GICR_TYPER */
	lsr	x5, x5, #32
	cmp	w4, w5
	b.eq	3f
	ldr	w4, [x2, #8]			/* GICR_TYPER */
	add	x3, x2, #(2 << 16)		/* Next redist */
	tbz	w4, #1, 2f			/* if VLPIS is set, */
	add	x3, x2, #(4 << 16)		/* it is two page further away */
2:	mov	x2, x3
	b	1b
3:	movn	w5, #(1 << 1)			/* ProcessorSleep */
	ldr	w4, [x2, #0x014]		/* GICR_WAKER */
	and	w4, w4, w5			/* Clear ProcessorSleep */
	str	w4, [x2, #0x014]		/* GICR_WAKER */
	dsb	st
	isb
4:	ldr	w4, [x2, #0x014]		/* GICR_WAKER */
	ands	wzr, w4, #(1 << 2)		/* Test ChildrenAsleep */
	b.ne	4b
	add	x3, x2, #(1 << 16)		/* SGI_base */
	mvn	w5, wzr
	str	w5, [x3, #0x80]			/* GICR_IGROUP0 */
	str	wzr, [x3, #0xD00]		/* GICR_IGRPMOD0 */
	/* GICv3 secured CPU interface init */
_gic_cpu_init:
#define ICC_SRE_EL2	S3_4_C12_C9_5
#define ICC_SRE_EL3	S3_6_C12_C12_5
#define ICC_CTLR_EL1	S3_0_C12_C12_4
#define ICC_CTLR_EL3	S3_6_C12_C12_4
#define ICC_PMR_EL1	S3_0_C4_C6_0
	/* Enable SRE at EL3 and ICC_SRE_EL2 access */
	mov	x0, #((1 << 3) | (1 << 0))	/* Enable | SRE */
	mrs	x1, ICC_SRE_EL3
	orr	x1, x1, x0
	msr	ICC_SRE_EL3, x1
	isb
	/* Configure CPU interface */
	msr	ICC_CTLR_EL3, xzr
	isb
	b	_gic_init_done
	.align 3
__gic_dist_base:
	.dword GIC_DIST_BASE
__gic_rdist_base:
	.dword GIC_RDIST_BASE
_gic_init_done:
#elif defined(GICv2)
	/* GICv2 secured distributor interface init */
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
	/* GICv2 secured CPU interface init */
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
#endif

	/* Print GIC banner */
	mrs	x0, mpidr_el1
	ldr	x1, __mpidr_mask
	tst	x0, x1
	b.ne	_gic_banner_done	/* Secondary CPU */
_gic_banner:
	adr	x0, __banner_gic
	bl	_uart_puts
_gic_banner_done:

	/* SOC specific TrustZone initialization */
	mrs	x0, mpidr_el1
	ldr	x1, __mpidr_mask
	tst	x0, x1
	b.ne	_tz_soc_init_done		/* Secondary CPU */
_tz_soc_init:
	nop
_tz_soc_init_done:

	/* Print SOC banner */
	mrs	x0, mpidr_el1
	ldr	x1, __mpidr_mask
	tst	x0, x1
	b.ne	_soc_banner_done	/* Secondary CPU */
_soc_banner:
	adr	x0, __banner_soc
	bl	_uart_puts
_soc_banner_done:

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
	/* Print EL2 banner */
	mrs	x0, mpidr_el1
	ldr	x1, __mpidr_mask
	tst	x0, x1
	b.ne	_el2_banner_done	/* Secondary CPU */
_el2_banner:
	adr	x0, __banner_toel2
	bl	_uart_puts
_el2_banner_done:

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

	/* Print banner */
	adr	x0, __banner_last
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
__banner_first:
	.ascii "\r\n\r\nboot-wrapper: starting.\r\n"
	.dword 0x0
	.align 3
__banner_reloc_start:
	.ascii "boot-wrapper: relocation start.\r\n"
	.dword 0x0
	.align 3
__banner_reloc_busy:
	.ascii "#"
	.dword 0x0
	.align 3
__banner_reloc_end:
	.ascii "\r\nboot-wrapper: relocation done.\r\n"
	.dword 0x0
	.align 3
__banner_timer:
	.ascii "boot-wrapper: timer initialization done.\r\n"
	.dword 0x0
	.align 3
__banner_gic:
	.ascii "boot-wrapper: gic initialization done.\r\n"
	.dword 0x0
	.align 3
__banner_soc:
	.ascii "boot-wrapper: soc initialization done.\r\n"
	.dword 0x0
	.align 3
__banner_toel2:
	.ascii "boot-wrapper: switch to el2-mode.\r\n"
	.dword 0x0
	.align 3
__banner_last:
	.ascii "boot-wrapper: finished.\r\nboot-wrapper: jumping to input kernel...\r\n"
	.dword 0x0
#ifdef DTB
	.align 3
__dtb_addr:
	.dword	dtb
#endif

#define	str(s)		#s
#define	stringify(s)	str(s)

	.section .text, "ax", %progbits
#ifdef DTB
	/* DTB binary */
	.globl	dtb
	.balign 0x20000
dtb:
	.incbin	stringify(DTB)
#endif
#ifdef IMAGE
	/* Input binary containing OS images */
	.globl	input_bin
	.balign 0x80000
input_bin:
	.incbin	stringify(IMAGE)
#endif
#ifdef INITRD
	/* INITRD binary */
	.globl	initrd
	.balign 0x800000
initrd:
	.incbin	stringify(INITRD)
	.dword 0x0
	.dword 0x0
	.dword 0x0
	.dword 0x0
#endif
	.balign 0x1000
_the_end:
	.dword 0x0
