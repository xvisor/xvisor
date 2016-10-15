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

#if defined(PRESERVE_BOOT_REGS) && defined(DTB)
#error "Cannot specify DTB when boot registers are preserved"
#endif

#if defined(PRESERVE_BOOT_REGS) && defined(INITRD)
#error "Cannot specify INITRD when boot registers are preserved"
#endif

#define CPSR_MODE_MASK					0x0000001f
#define CPSR_MODE_USER					0x00000010
#define CPSR_MODE_FIQ					0x00000011
#define CPSR_MODE_IRQ					0x00000012
#define CPSR_MODE_SUPERVISOR				0x00000013
#define CPSR_MODE_MONITOR				0x00000016
#define CPSR_MODE_ABORT					0x00000017
#define CPSR_MODE_HYPERVISOR				0x0000001a
#define CPSR_MODE_UNDEFINED				0x0000001b
#define CPSR_MODE_SYSTEM				0x0000001f

	/* Boot-wrapper entry point */
	.section .text, "ax", %progbits
	.globl	start_boot
start_boot:
	/* Preserve boot registers r0, r1, and r2 */
#if defined(PRESERVE_BOOT_REGS)
preserve_boot_regs:
	mrc	p15, 0, r4, c0, c0, 5		/* MPIDR (ARMv7 only) */
	and	r4, r4, #0xFFFFFF		/* CPU affinity */
	cmp	r4, #0				/* primary CPU? */
	bne	preserve_boot_regs_done
	adr	r4, __boot_reg0
	str	r0, [r4]
	adr	r4, __boot_reg1
	str	r1, [r4]
	adr	r4, __boot_reg2
	str	r2, [r4]
	b	preserve_boot_regs_done
__boot_reg0:
	.word 0x0
__boot_reg1:
	.word 0x0
__boot_reg2:
	.word 0x0
preserve_boot_regs_done:
#endif

	/* Setup UART */
uart_init:
	mrc	p15, 0, r0, c0, c0, 5		/* MPIDR (ARMv7 only) */
	and	r0, r0, #0xFFFFFF		/* CPU affinity */
	cmp	r0, #0				/* primary CPU? */
	bne	uart_init_done
#if defined(UART_PL011)
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
	ldr	r1, __uart_addr
	b	__uart_addr_next
__uart_addr:
	.word	UART_PL011_BASE
__uart_addr_next:
	mov	r0, #0x10			/* IBRD */
	str	r0, [r1, #UART_PL011_IBRD]
	mov	r0, #0xc300
	orr	r0, #0x0001			/* CR */
	str	r0, [r1, #UART_PL011_CR]
	b	uart_init_done
uart_puts:
	ldrb	r5, [r0], #1
	cmp	r5, #0
	beq	uart_puts_done
	ldr	r4, __uart_addr
uart_puts_check:
	ldr	r6, [r4, #UART_PL011_FR]	/* FR */
	and	r6, r6, #UART_PL011_FR_TXFF	/* FR.TXFF */
	cmp	r6, #UART_PL011_FR_TXFF
	beq	uart_puts_check
	str	r5, [r4, #UART_PL011_DR]	/* DR */
	b	uart_puts
uart_puts_done:
	mov	pc, lr
#else
	b	uart_init_done
uart_puts:
	mov	pc, lr
#endif
uart_init_done:

	/* Print first banner */
first_banner:
	mrc	p15, 0, r0, c0, c0, 5		/* MPIDR (ARMv7 only) */
	and	r0, r0, #0xFFFFFF		/* CPU affinity */
	cmp	r0, #0				/* primary CPU? */
	bne	first_banner_done
	adr	r0, __banner_first
	bl	uart_puts
first_banner_done:

	/* If already in Hyp-mode then skip timer, coproc, and GIC init */
hyp_check:
	mrs	r0, cpsr_all
	and	r0, r0, #(CPSR_MODE_MASK)
	cmp	r0, #(CPSR_MODE_HYPERVISOR)
	beq	tohyp_skip

	/* Setup generic timer cntfrq */
gentimer_init:
	b	__gentimer_freq_next
__gentimer_freq:
	.word	GENTIMER_FREQ
__gentimer_freq_next:
	ldr	r1, __gentimer_freq
	mcr     p15, 0, r1, c14, c0, 0

	/* Print timer banner */
timer_banner:
	mrc	p15, 0, r0, c0, c0, 5		/* MPIDR (ARMv7 only) */
	and	r0, r0, #0xFFFFFF		/* CPU affinity */
	cmp	r0, #0				/* primary CPU? */
	bne	timer_banner_done
	adr	r0, __banner_timer
	bl	uart_puts
timer_banner_done:

	/* Set NSACR to allow coprocessor access from non-secure */
coproc_init:
	mrc	p15, 0, r0, c1, c1, 2
	ldr	r1, __nsacr_val
	b	__nsacr_val_next
__nsacr_val:
	.word	0x43fff
__nsacr_val_next:
	orr	r0, r0, r1
	mcr	p15, 0, r0, c1, c1, 2

	/* Print coproc banner */
coproc_banner:
	mrc	p15, 0, r0, c0, c0, 5		/* MPIDR (ARMv7 only) */
	and	r0, r0, #0xFFFFFF		/* CPU affinity */
	cmp	r0, #0				/* primary CPU? */
	bne	coproc_banner_done
	adr	r0, __banner_coproc
	bl	uart_puts
coproc_banner_done:

	/* GIC init */
gic_init:
#ifdef GICv2
	/* GICv2 secured distributor interface init */
	mrc	p15, 0, r4, c0, c0, 5		/* MPIDR (ARMv7 only) */
	and	r4, r4, #0xFFFFFF		/* CPU affinity */
	ldr	r0, __dist_gic_base		/* Dist GIC base */
	b 	__dist_gic_base_next
__dist_gic_base:
	.word	GIC_DIST_BASE
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

	/* GICv2 secured CPU interface init */
	ldr	r0, __cpu_gic_base		/* GIC CPU base */
	b 	__cpu_gic_base_next
__cpu_gic_base:
	.word	GIC_CPU_BASE
__cpu_gic_base_next:
	mov	r1, #0x80
	str	r1, [r0, #0x4]			/* GIC CPU Priority Mask */
#endif

	/* Print gic banner */
gic_banner:
	mrc	p15, 0, r0, c0, c0, 5		/* MPIDR (ARMv7 only) */
	and	r0, r0, #0xFFFFFF		/* CPU affinity */
	cmp	r0, #0				/* primary CPU? */
	bne	gic_banner_done
	adr	r0, __banner_gic
	bl	uart_puts
gic_banner_done:

	/* Enter hypervisor mode */
switch_to_hvc_mode:
	bl	enter_hvc_mode

	/* Print tohyp banner */
tohyp_banner:
	mrc	p15, 0, r0, c0, c0, 5		/* MPIDR (ARMv7 only) */
	and	r0, r0, #0xFFFFFF		/* CPU affinity */
	cmp	r0, #0				/* primary CPU? */
	bne	tohyp_banner_done
	adr	r0, __banner_tohyp
	bl	uart_puts
tohyp_banner_done:

tohyp_skip:

#ifdef GICv2
	/* GIC non-secured CPU interface init */
gic_nonsec_cpu_init:
	ldr	r0, __cpu_gic_base
	mov	r1, #0x1
	str	r1, [r0]			/* GIC CPU Control */
#endif

	/* Non-secured spin-loop */
spin_loop_init:
	/* Skip secondary loop for Primary core */
	mrc	p15, 0, r0, c0, c0, 5		/* MPIDR (ARMv7 only) */
	and	r0, r0, #0xFFFFFF		/* CPU affinity */
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
	ldr	r0, __spin_location_addr
	mov	pc, r1				/* Branch to the relocated code */
__spin_location_addr:
	.word	SPIN_LOCATION
__spin_location_addr_next:
secondary_loop:
	wfi
	ldr	r1, [r0]
	cmp	r1, #0
	beq	secondary_loop
	mov	pc, r1
secondary_loop_skip:

	/* Print last banner */
last_banner:
	mrc	p15, 0, r0, c0, c0, 5		/* MPIDR (ARMv7 only) */
	and	r0, r0, #0xFFFFFF		/* CPU affinity */
	cmp	r0, #0				/* primary CPU? */
	bne	last_banner_done
	adr	r0, __banner_last
	bl	uart_puts
last_banner_done:

	/* Jump to input binary */
boot_next:
#if defined(PRESERVE_BOOT_REGS)
	adr	r4, __boot_reg0
	ldr	r0, [r4]
	adr	r4, __boot_reg1
	ldr	r1, [r4]
	adr	r4, __boot_reg2
	ldr	r2, [r4]
#else
	mov	r0, #0
	mov	r1, #0
#ifdef DTB
	/* If dtb is provided, load the address where we placed it */
	ldr	r2, __dtb_addr
#else
	/* Deliberately put a non-4B aligned value to r2 skip dtb checking */
	mov	r2, #1
#endif
#endif
#ifdef IMAGE
	/* Jump to input binary */
	b	input_bin
#else
	/* Just hang */
	b	.
#endif

#ifdef DTB
__dtb_addr:
	.word	dtb
#endif

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

	.align 2
__banner_first:
	.ascii "\r\n\r\nboot-wrapper: starting.\r\n"
	.word 0x0
	.align 2
__banner_timer:
	.ascii "boot-wrapper: timer initialization done.\r\n"
	.word 0x0
	.align 2
__banner_coproc:
	.ascii "boot-wrapper: coproc initialization done.\r\n"
	.word 0x0
	.align 2
__banner_gic:
	.ascii "boot-wrapper: gic initialization done.\r\n"
	.word 0x0
	.align 2
__banner_tohyp:
	.ascii "boot-wrapper: switch to hyp-mode.\r\n"
	.word 0x0
	.align 2
__banner_last:
	.ascii "boot-wrapper: finished.\r\nboot-wrapper: jumping to input kernel...\r\n"
	.word 0x0

#define	str(s)		#s
#define	stringify(s)	str(s)

	/* Input binary containing OS images */
	.section .text, "ax", %progbits
#ifdef DTB
	.globl	dtb
	.balign 0x1000
dtb:
	.incbin	stringify(DTB)
#endif
#ifdef IMAGE
	.globl	input_bin
	.balign 0x8000
input_bin:
	.incbin	stringify(IMAGE)
	.globl	input_bin_end
input_bin_end:
#endif
#ifdef INITRD
	.globl	initrd
	.balign 0x1000000
initrd:
	.incbin	stringify(INITRD)
	.globl	initrd_end
initrd_end:
#endif
