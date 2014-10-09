/**
 * Copyright (c) 2014 Anup Patel.
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
 * @file cpu_vcpu_switch.S
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of low-level VCPU context switching functions
 */

#include <cpu_defines.h>
#include <generic_timer.h>

	.globl cpu_vcpu_sysregs_regs_save
cpu_vcpu_sysregs_regs_save:
	str	x1, [sp, #-8]!
	/* Save 64bit EL1/EL0 registers */
	mrs	x1, sp_el0		/* sp_el0 */
	str	x1, [x0, #ARM_PRIV_SYSREGS_sp_el0]
	mrs	x1, sp_el1		/* sp_el1 */
	str	x1, [x0, #ARM_PRIV_SYSREGS_sp_el1]
	mrs	x1, elr_el1		/* elr_el1 */
	str	x1, [x0, #ARM_PRIV_SYSREGS_elr_el1]
	mrs	x1, spsr_el1		/* spsr_el1 */
	str	x1, [x0, #ARM_PRIV_SYSREGS_spsr_el1]
	mrs	x1, sctlr_el1		/* sctlr_el1 */
	str	x1, [x0, #ARM_PRIV_SYSREGS_sctlr_el1]
	mrs	x1, cpacr_el1		/* cpacr_el1 */
	str	x1, [x0, #ARM_PRIV_SYSREGS_cpacr_el1]
	mrs	x1, ttbr0_el1		/* ttbr0_el1 */
	str	x1, [x0, #ARM_PRIV_SYSREGS_ttbr0_el1]
	mrs	x1, ttbr1_el1		/* ttbr1_el1 */
	str	x1, [x0, #ARM_PRIV_SYSREGS_ttbr1_el1]
	mrs	x1, tcr_el1		/* tcr_el1 */
	str	x1, [x0, #ARM_PRIV_SYSREGS_tcr_el1]
	mrs	x1, esr_el1		/* esr_el1 */
	str	x1, [x0, #ARM_PRIV_SYSREGS_esr_el1]
	mrs	x1, far_el1		/* far_el1 */
	str	x1, [x0, #ARM_PRIV_SYSREGS_far_el1]
	mrs	x1, par_el1		/* par_el1 */
	str	x1, [x0, #ARM_PRIV_SYSREGS_par_el1]
	mrs	x1, mair_el1		/* mair_el1 */
	str	x1, [x0, #ARM_PRIV_SYSREGS_mair_el1]
	mrs	x1, vbar_el1		/* vbar_el1 */
	str	x1, [x0, #ARM_PRIV_SYSREGS_vbar_el1]
	mrs	x1, contextidr_el1	/* contextidr_el1 */
	str	x1, [x0, #ARM_PRIV_SYSREGS_contextidr_el1]
	mrs	x1, tpidr_el0		/* tpidr_el0 */
	str	x1, [x0, #ARM_PRIV_SYSREGS_tpidr_el0]
	mrs	x1, tpidr_el1		/* tpidr_el1 */
	str	x1, [x0, #ARM_PRIV_SYSREGS_tpidr_el1]
	mrs	x1, tpidrro_el0		/* tpidrro_el0 */
	str	x1, [x0, #ARM_PRIV_SYSREGS_tpidrro_el0]
	/* Save 32bit only registers */
	mrs	x1, spsr_abt		/* spsr_abt */
	str	w1, [x0, #ARM_PRIV_SYSREGS_spsr_abt]
	mrs	x1, spsr_und		/* spsr_und */
	str	w1, [x0, #ARM_PRIV_SYSREGS_spsr_und]
	mrs	x1, spsr_irq		/* spsr_irq */
	str	w1, [x0, #ARM_PRIV_SYSREGS_spsr_irq]
	mrs	x1, spsr_fiq		/* spsr_fiq */
	str	w1, [x0, #ARM_PRIV_SYSREGS_spsr_fiq]
	mrs	x1, dacr32_el2		/* dacr32_el2 */
	str	w1, [x0, #ARM_PRIV_SYSREGS_dacr32_el2]
	mrs	x1, ifsr32_el2		/* ifsr32_el2 */
	str	w1, [x0, #ARM_PRIV_SYSREGS_ifsr32_el2]
	/* Save 32bit only ThumbEE registers */
	mrs	x1, id_pfr0_el1
	and	x1, x1, #ID_PFR0_THUMBEE_MASK
	cmp	x1, #0
	beq	save_skip_thumbee
	mrs	x1, teecr32_el1		/* teecr32_el1 */
	str	w1, [x0, #ARM_PRIV_SYSREGS_teecr32_el1]
	mrs	x1, teehbr32_el1	/* teehbr32_el1 */
	str	w1, [x0, #ARM_PRIV_SYSREGS_teehbr32_el1]
save_skip_thumbee:
	ldr	x1, [sp], #8
	ret

	.globl cpu_vcpu_sysregs_regs_restore
cpu_vcpu_sysregs_regs_restore:
	str	x1, [sp, #-8]!
	/* Update VPIDR and VMPIDR */
	ldr	x1, [x0, #ARM_PRIV_SYSREGS_midr_el1]
	msr	vpidr_el2, x1		/* midr_el1 */
	ldr	x1, [x0, #ARM_PRIV_SYSREGS_mpidr_el1]
	msr	vmpidr_el2, x1		/* mpidr_el1 */
	/* Restore 64bit EL1/EL0 register */
	ldr	x1, [x0, #ARM_PRIV_SYSREGS_sp_el0]
	msr	sp_el0, x1		/* sp_el0 */
	ldr	x1, [x0, #ARM_PRIV_SYSREGS_sp_el1]
	msr	sp_el1, x1		/* sp_el1 */
	ldr	x1, [x0, #ARM_PRIV_SYSREGS_elr_el1]
	msr	elr_el1, x1		/* elr_el1 */
	ldr	x1, [x0, #ARM_PRIV_SYSREGS_spsr_el1]
	msr	spsr_el1, x1		/* spsr_el1 */
	ldr	x1, [x0, #ARM_PRIV_SYSREGS_sctlr_el1]
	msr	sctlr_el1, x1		/* sctlr_el1 */
	ldr	x1, [x0, #ARM_PRIV_SYSREGS_cpacr_el1]
	msr	cpacr_el1, x1		/* cpacr_el1 */
	ldr	x1, [x0, #ARM_PRIV_SYSREGS_ttbr0_el1]
	msr	ttbr0_el1, x1		/* ttbr0_el1 */
	ldr	x1, [x0, #ARM_PRIV_SYSREGS_ttbr1_el1]
	msr	ttbr1_el1, x1		/* ttbr1_el1 */
	ldr	x1, [x0, #ARM_PRIV_SYSREGS_tcr_el1]
	msr	tcr_el1, x1		/* tcr_el1 */
	ldr	x1, [x0, #ARM_PRIV_SYSREGS_esr_el1]
	msr	esr_el1, x1		/* esr_el1 */
	ldr	x1, [x0, #ARM_PRIV_SYSREGS_far_el1]
	msr	far_el1, x1		/* far_el1 */
	ldr	x1, [x0, #ARM_PRIV_SYSREGS_par_el1]
	msr	par_el1, x1		/* par_el1 */
	ldr	x1, [x0, #ARM_PRIV_SYSREGS_mair_el1]
	msr	mair_el1, x1		/* mair_el1 */
	ldr	x1, [x0, #ARM_PRIV_SYSREGS_vbar_el1]
	msr	vbar_el1, x1		/* vbar_el1 */
	ldr	x1, [x0, #ARM_PRIV_SYSREGS_contextidr_el1]
	msr	contextidr_el1, x1	/* contextidr_el1 */
	ldr	x1, [x0, #ARM_PRIV_SYSREGS_tpidr_el0]
	msr	tpidr_el0, x1		/* tpidr_el0 */
	ldr	x1, [x0, #ARM_PRIV_SYSREGS_tpidr_el1]
	msr	tpidr_el1, x1		/* tpidr_el1 */
	ldr	x1, [x0, #ARM_PRIV_SYSREGS_tpidrro_el0]
	msr	tpidrro_el0, x1		/* tpidrro_el0 */
	/* Restore 32bit only registers */
	ldr	w1, [x0, #ARM_PRIV_SYSREGS_spsr_abt]
	msr	spsr_abt, x1 		/* spsr_abt */
	ldr	w1, [x0, #ARM_PRIV_SYSREGS_spsr_und]
	msr	spsr_und, x1		/* spsr_und */
	ldr	w1, [x0, #ARM_PRIV_SYSREGS_spsr_irq]
	msr	spsr_irq, x1		/* spsr_irq */
	ldr	w1, [x0, #ARM_PRIV_SYSREGS_spsr_fiq]
	msr	spsr_fiq, x1		/* spsr_fiq */
	ldr	w1, [x0, #ARM_PRIV_SYSREGS_dacr32_el2]
	msr	dacr32_el2, x1		/* dacr32_el2 */
	ldr	w1, [x0, #ARM_PRIV_SYSREGS_ifsr32_el2]
	msr	ifsr32_el2, x1		/* ifsr32_el2 */
	/* Restore 32bit only ThumbEE registers */
	mrs	x1, id_pfr0_el1
	and	x1, x1, #ID_PFR0_THUMBEE_MASK
	cmp	x1, #0
	beq	restore_skip_thumbee
	ldr	w1, [x0, #ARM_PRIV_SYSREGS_teecr32_el1]
	msr	teecr32_el1, x1		/* teecr32_el1 */
	ldr	w1, [x0, #ARM_PRIV_SYSREGS_teehbr32_el1]
	msr	teehbr32_el1, x1	/* teehbr32_el1 */
restore_skip_thumbee:
	ldr	x1, [sp], #8
	ret

	.globl cpu_vcpu_vfp_regs_save
cpu_vcpu_vfp_regs_save:
	str	x1, [sp, #-8]!
	/* Save floating point control registers */
	mrs	x1, fpcr		/* fpcr */
	str	w1, [x0, #ARM_PRIV_VFP_fpcr]
	mrs	x1, fpsr		/* fpsr */
	str	w1, [x0, #ARM_PRIV_VFP_fpsr]
	mrs	x1, fpexc32_el2		/* fpexc32 */
	str	w1, [x0, #ARM_PRIV_VFP_fpexc32]
	/* Save floating point registers */
	add	x1, x0, #ARM_PRIV_VFP_fpregs
	stp	 q0,  q1, [x1, #0x000]
	stp	 q2,  q3, [x1, #0x020]
	stp	 q4,  q5, [x1, #0x040]
	stp	 q6,  q7, [x1, #0x060]
	stp	 q8,  q9, [x1, #0x080]
	stp	q10, q11, [x1, #0x0A0]
	stp	q12, q13, [x1, #0x0C0]
	stp	q14, q15, [x1, #0x0E0]
	stp	q16, q17, [x1, #0x100]
	stp	q18, q19, [x1, #0x120]
	stp	q20, q21, [x1, #0x140]
	stp	q22, q23, [x1, #0x160]
	stp	q24, q25, [x1, #0x180]
	stp	q26, q27, [x1, #0x1A0]
	stp	q28, q29, [x1, #0x1C0]
	stp	q30, q31, [x1, #0x1E0]
	ldr	x1, [sp], #8
	ret

	.globl cpu_vcpu_vfp_regs_restore
cpu_vcpu_vfp_regs_restore:
	str	x1, [sp, #-8]!
	/* Restore floating point registers */
	add	x1, x0, #ARM_PRIV_VFP_fpregs
	ldp	 q0,  q1, [x1, #0x000]
	ldp	 q2,  q3, [x1, #0x020]
	ldp	 q4,  q5, [x1, #0x040]
	ldp	 q6,  q7, [x1, #0x060]
	ldp	 q8,  q9, [x1, #0x080]
	ldp	q10, q11, [x1, #0x0A0]
	ldp	q12, q13, [x1, #0x0C0]
	ldp	q14, q15, [x1, #0x0E0]
	ldp	q16, q17, [x1, #0x100]
	ldp	q18, q19, [x1, #0x120]
	ldp	q20, q21, [x1, #0x140]
	ldp	q22, q23, [x1, #0x160]
	ldp	q24, q25, [x1, #0x180]
	ldp	q26, q27, [x1, #0x1A0]
	ldp	q28, q29, [x1, #0x1C0]
	ldp	q30, q31, [x1, #0x1E0]
	/* Restore floating point control registers */
	ldr	w1, [x0, #ARM_PRIV_VFP_fpcr]
	msr	fpcr, x1		/* fpcr */
	ldr	w1, [x0, #ARM_PRIV_VFP_fpsr]
	msr	fpsr, x1		/* fpsr */
	ldr	w1, [x0, #ARM_PRIV_VFP_fpexc32]
	msr	fpexc32_el2, x1		/* fpexc32 */
	ldr	x1, [sp], #8
	ret

	.globl generic_timer_regs_save
generic_timer_regs_save:
	str	x1, [sp, #-8]!
	mrs	x1, cntp_ctl_el0	/* cntpctl */
	str	w1, [x0, #GENERIC_TIMER_CONTEXT_cntpctl]
	mrs	x1, cntv_ctl_el0	/* cntvctl */
	str	w1, [x0, #GENERIC_TIMER_CONTEXT_cntvctl]
	mrs	x1, cntkctl_el1		/* cntkctl */
	str	w1, [x0, #GENERIC_TIMER_CONTEXT_cntkctl]
	mrs	x1, cntp_cval_el0	/* cntpcval */
	str	x1, [x0, #GENERIC_TIMER_CONTEXT_cntpcval]
	mrs	x1, cntv_cval_el0	/* cntvcval */
	str	x1, [x0, #GENERIC_TIMER_CONTEXT_cntvcval]
	mov	x1, #GENERIC_TIMER_CTRL_IT_MASK
	msr	cntp_ctl_el0, x1	/* disable physical timer */
	msr	cntv_ctl_el0, x1	/* disable virtual timer */
	ldr	x1, [sp], #8
	ret

	.globl generic_timer_regs_restore
generic_timer_regs_restore:
	str	x1, [sp, #-8]!
	ldr	x1, [x0, GENERIC_TIMER_CONTEXT_cntvoff]
	msr	cntvoff_el2, x1		/* cntvoff */
	ldr	x1, [x0, #GENERIC_TIMER_CONTEXT_cntpcval]
	msr	cntp_cval_el0, x1	/* cntpcval */
	ldr	x1, [x0, #GENERIC_TIMER_CONTEXT_cntvcval]
	msr	cntv_cval_el0, x1	/* cntvcval */
	ldr	w1, [x0, #GENERIC_TIMER_CONTEXT_cntkctl]
	msr	cntkctl_el1, x1		/* cntkctl */
	ldr	w1, [x0, #GENERIC_TIMER_CONTEXT_cntpctl]
	msr	cntp_ctl_el0, x1	/* cntpctl */
	ldr	w1, [x0, #GENERIC_TIMER_CONTEXT_cntvctl]
	msr	cntv_ctl_el0, x1	/* cntvctl */
	ldr	x1, [sp], #8
	ret

