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

	.globl cpu_vcpu_banked_regs_save
cpu_vcpu_banked_regs_save:
	push	{r1}
	mrs	r1, SP_usr	/* sp_usr */
	str	r1, [r0, #ARM_PRIV_BANKED_sp_usr]
	mrs	r1, SP_svc	/* sp_svc */
	str	r1, [r0, #ARM_PRIV_BANKED_sp_svc]
	mrs	r1, LR_svc	/* lr_svc */
	str	r1, [r0, #ARM_PRIV_BANKED_lr_svc]
	mrs	r1, SPSR_svc	/* spsr_svc */
	str	r1, [r0, #ARM_PRIV_BANKED_spsr_svc]
	mrs	r1, SP_abt	/* sp_abt */
	str	r1, [r0, #ARM_PRIV_BANKED_sp_abt]
	mrs	r1, LR_abt	/* lr_abt */
	str	r1, [r0, #ARM_PRIV_BANKED_lr_abt]
	mrs	r1, SPSR_abt	/* spsr_abt */
	str	r1, [r0, #ARM_PRIV_BANKED_spsr_abt]
	mrs	r1, SP_und	/* sp_und */
	str	r1, [r0, #ARM_PRIV_BANKED_sp_und]
	mrs	r1, LR_und	/* lr_und */
	str	r1, [r0, #ARM_PRIV_BANKED_lr_und]
	mrs	r1, SPSR_und	/* spsr_und */
	str	r1, [r0, #ARM_PRIV_BANKED_spsr_und]
	mrs	r1, SP_irq	/* sp_irq */
	str	r1, [r0, #ARM_PRIV_BANKED_sp_irq]
	mrs	r1, LR_irq	/* lr_irq */
	str	r1, [r0, #ARM_PRIV_BANKED_lr_irq]
	mrs	r1, SPSR_irq	/* spsr_irq */
	str	r1, [r0, #ARM_PRIV_BANKED_spsr_irq]
	mrs	r1, r8_fiq	/* gpr_fiq[0] */
	str	r1, [r0, #ARM_PRIV_BANKED_gpr_fiq0]
	mrs	r1, r9_fiq	/* gpr_fiq[1] */
	str	r1, [r0, #(ARM_PRIV_BANKED_gpr_fiq0 + 0x4*1)]
	mrs	r1, r10_fiq	/* gpr_fiq[2] */
	str	r1, [r0, #(ARM_PRIV_BANKED_gpr_fiq0 + 0x4*2)]
	mrs	r1, r11_fiq	/* gpr_fiq[3] */
	str	r1, [r0, #(ARM_PRIV_BANKED_gpr_fiq0 + 0x4*3)]
	mrs	r1, r12_fiq	/* gpr_fiq[4] */
	str	r1, [r0, #(ARM_PRIV_BANKED_gpr_fiq0 + 0x4*4)]
	mrs	r1, SP_fiq 	/* sp_fiq */
	str	r1, [r0, #ARM_PRIV_BANKED_sp_fiq]
	mrs	r1, LR_fiq	/* lr_fiq */
	str	r1, [r0, #ARM_PRIV_BANKED_lr_fiq]
	mrs	r1, SPSR_fiq	/* spsr_fiq */
	str	r1, [r0, #ARM_PRIV_BANKED_spsr_fiq]
	pop	{r1}
	bx 	lr

	.globl cpu_vcpu_banked_regs_restore
cpu_vcpu_banked_regs_restore:
	push	{r1}
	ldr	r1, [r0, #ARM_PRIV_BANKED_sp_usr]
	msr	SP_usr, r1	/* sp_usr */
	ldr	r1, [r0, #ARM_PRIV_BANKED_sp_svc]
	msr	SP_svc, r1	/* sp_svc */
	ldr	r1, [r0, #ARM_PRIV_BANKED_lr_svc]
	msr	LR_svc, r1	/* lr_svc */
	ldr	r1, [r0, #ARM_PRIV_BANKED_spsr_svc]
	msr	SPSR_svc, r1	/* spsr_svc */
	ldr	r1, [r0, #ARM_PRIV_BANKED_sp_abt]
	msr	SP_abt, r1	/* sp_abt */
	ldr	r1, [r0, #ARM_PRIV_BANKED_lr_abt]
	msr	LR_abt, r1	/* lr_abt */
	ldr	r1, [r0, #ARM_PRIV_BANKED_spsr_abt]
	msr	SPSR_abt, r1	/* spsr_abt */
	ldr	r1, [r0, #ARM_PRIV_BANKED_sp_und]
	msr	SP_und, r1	/* sp_und */
	ldr	r1, [r0, #ARM_PRIV_BANKED_lr_und]
	msr	LR_und, r1	/* lr_und */
	ldr	r1, [r0, #ARM_PRIV_BANKED_spsr_und]
	msr	SPSR_und, r1	/* spsr_und */
	ldr	r1, [r0, #ARM_PRIV_BANKED_sp_irq]
	msr	SP_irq, r1	/* sp_irq */
	ldr	r1, [r0, #ARM_PRIV_BANKED_lr_irq]
	msr	LR_irq, r1	/* lr_irq */
	ldr	r1, [r0, #ARM_PRIV_BANKED_spsr_irq]
	msr	SPSR_irq, r1	/* spsr_irq */
	ldr	r1, [r0, #ARM_PRIV_BANKED_gpr_fiq0]
	msr	r8_fiq, r1	/* gpr_fiq[0] */
	ldr	r1, [r0, #(ARM_PRIV_BANKED_gpr_fiq0 + 0x4*1)]
	msr	r9_fiq, r1	/* gpr_fiq[1] */
	ldr	r1, [r0, #(ARM_PRIV_BANKED_gpr_fiq0 + 0x4*2)]
	msr	r10_fiq, r1	/* gpr_fiq[2] */
	ldr	r1, [r0, #(ARM_PRIV_BANKED_gpr_fiq0 + 0x4*3)]
	msr	r11_fiq, r1	/* gpr_fiq[3] */
	ldr	r1, [r0, #(ARM_PRIV_BANKED_gpr_fiq0 + 0x4*4)]
	msr	r12_fiq, r1	/* gpr_fiq[4] */
	ldr	r1, [r0, #ARM_PRIV_BANKED_sp_fiq]
	msr	SP_fiq, r1 	/* sp_fiq */
	ldr	r1, [r0, #ARM_PRIV_BANKED_lr_fiq]
	msr	LR_fiq, r1	/* lr_fiq */
	ldr	r1, [r0, #ARM_PRIV_BANKED_spsr_fiq]
	msr	SPSR_fiq, r1	/* spsr_fiq */
	pop	{r1}
	bx 	lr

	.globl cpu_vcpu_cp15_regs_save
cpu_vcpu_cp15_regs_save:
	push	{r1, r2}
	mrc	p15, 2, r1, c0, c0, 0	/* c0_cssel */
	str	r1, [r0, #ARM_PRIV_CP15_c0_cssel]
	mrc	p15, 0, r1, c1, c0, 0	/* c1_sctlr */
	str	r1, [r0, #ARM_PRIV_CP15_c1_sctlr]
	mrc	p15, 0, r1, c1, c0, 2	/* c1_cpacr */
	str	r1, [r0, #ARM_PRIV_CP15_c1_cpacr]
	mrrc	p15, 0, r1, r2, c2	/* c2_ttbr0 */
	str	r1, [r0, #ARM_PRIV_CP15_c2_ttbr0]
	str	r2, [r0, #(ARM_PRIV_CP15_c2_ttbr0 + 0x4)]
	mrrc	p15, 1, r1, r2, c2	/* c2_ttbr1 */
	str	r1, [r0, #ARM_PRIV_CP15_c2_ttbr1]
	str	r2, [r0, #(ARM_PRIV_CP15_c2_ttbr1 + 0x4)]
	mrc	p15, 0, r1, c2, c0, 2	/* c2_ttbcr */
	str	r1, [r0, #ARM_PRIV_CP15_c2_ttbcr]
	mrc	p15, 0, r1, c3, c0, 0	/* c3_dacr */
	str	r1, [r0, #ARM_PRIV_CP15_c3_dacr]
	mrc	p15, 0, r1, c5, c0, 1	/* c5_ifsr */
	str	r1, [r0, #ARM_PRIV_CP15_c5_ifsr]
	mrc	p15, 0, r1, c5, c0, 0	/* c5_dfsr */
	str	r1, [r0, #ARM_PRIV_CP15_c5_dfsr]
	mrc	p15, 0, r1, c5, c1, 1	/* c5_aifsr */
	str	r1, [r0, #ARM_PRIV_CP15_c5_aifsr]
	mrc	p15, 0, r1, c5, c1, 0	/* c5_adfsr */
	str	r1, [r0, #ARM_PRIV_CP15_c5_adfsr]
	mrc	p15, 0, r1, c6, c0, 2	/* c6_ifar */
	str	r1, [r0, #ARM_PRIV_CP15_c6_ifar]
	mrc	p15, 0, r1, c6, c0, 0	/* c6_dfar */
	str	r1, [r0, #ARM_PRIV_CP15_c6_dfar]
	mrc	p15, 0, r1, c7, c4, 0	/* c7_par */
	str	r1, [r0, #ARM_PRIV_CP15_c7_par]
	mrrc	p15, 0, r1, r2, c7	/* c7_par64 */
	str	r1, [r0, #ARM_PRIV_CP15_c7_par64]
	str	r2, [r0, #(ARM_PRIV_CP15_c7_par64 + 0x4)]
	mrc	p15, 0, r1, c10, c2, 0	/* c10_prrr */
	str	r1, [r0, #ARM_PRIV_CP15_c10_prrr]
	mrc	p15, 0, r1, c10, c2, 1	/* c10_nmrr */
	str	r1, [r0, #ARM_PRIV_CP15_c10_nmrr]
	mrc	p15, 0, r1, c12, c0, 0	/* c12_vbar */
	str	r1, [r0, #ARM_PRIV_CP15_c12_vbar]
	mrc	p15, 0, r1, c13, c0, 0	/* c13_fcseidr */
	str	r1, [r0, #ARM_PRIV_CP15_c13_fcseidr]
	mrc	p15, 0, r1, c13, c0, 1	/* c13_contextidr */
	str	r1, [r0, #ARM_PRIV_CP15_c13_contextidr]
	mrc	p15, 0, r1, c13, c0, 2	/* c13_tls1 */
	str	r1, [r0, #ARM_PRIV_CP15_c13_tls1]
	mrc	p15, 0, r1, c13, c0, 3	/* c13_tls2 */
	str	r1, [r0, #ARM_PRIV_CP15_c13_tls2]
	mrc	p15, 0, r1, c13, c0, 4	/* c13_tls3 */
	str	r1, [r0, #ARM_PRIV_CP15_c13_tls3]
	pop	{r1, r2}
	bx 	lr

	.globl cpu_vcpu_cp15_regs_restore
cpu_vcpu_cp15_regs_restore:
	push	{r1, r2}
	ldr	r1, [r0, #ARM_PRIV_CP15_c0_cssel]
	mcr	p15, 2, r1, c0, c0, 0	/* c0_cssel */
	ldr	r1, [r0, #ARM_PRIV_CP15_c1_sctlr]
	mcr	p15, 0, r1, c1, c0, 0	/* c1_sctlr */
	ldr	r1, [r0, #ARM_PRIV_CP15_c1_cpacr]
	mcr	p15, 0, r1, c1, c0, 2	/* c1_cpacr */
	ldr	r1, [r0, #ARM_PRIV_CP15_c2_ttbr0]
	ldr	r2, [r0, #(ARM_PRIV_CP15_c2_ttbr0 + 0x4)]
	mcrr	p15, 0, r1, r2, c2	/* c2_ttbr0 */
	ldr	r1, [r0, #ARM_PRIV_CP15_c2_ttbr1]
	ldr	r2, [r0, #(ARM_PRIV_CP15_c2_ttbr1 + 0x4)]
	mcrr	p15, 1, r1, r2, c2	/* c2_ttbr1 */
	ldr	r1, [r0, #ARM_PRIV_CP15_c2_ttbcr]
	mcr	p15, 0, r1, c2, c0, 2	/* c2_ttbcr */
	ldr	r1, [r0, #ARM_PRIV_CP15_c3_dacr]
	mcr	p15, 0, r1, c3, c0, 0	/* c3_dacr */
	ldr	r1, [r0, #ARM_PRIV_CP15_c5_ifsr]
	mcr	p15, 0, r1, c5, c0, 1	/* c5_ifsr */
	ldr	r1, [r0, #ARM_PRIV_CP15_c5_dfsr]
	mcr	p15, 0, r1, c5, c0, 0	/* c5_dfsr */
	ldr	r1, [r0, #ARM_PRIV_CP15_c5_aifsr]
	mcr	p15, 0, r1, c5, c1, 1	/* c5_aifsr */
	ldr	r1, [r0, #ARM_PRIV_CP15_c5_adfsr]
	mcr	p15, 0, r1, c5, c1, 0	/* c5_adfsr */
	ldr	r1, [r0, #ARM_PRIV_CP15_c6_ifar]
	mcr	p15, 0, r1, c6, c0, 2	/* c6_ifar */
	ldr	r1, [r0, #ARM_PRIV_CP15_c6_dfar]
	mcr	p15, 0, r1, c6, c0, 0	/* c6_dfar */
	ldr	r1, [r0, #ARM_PRIV_CP15_c7_par]
	mcr	p15, 0, r1, c7, c4, 0	/* c7_par */
	ldr	r1, [r0, #ARM_PRIV_CP15_c7_par64]
	ldr	r2, [r0, #(ARM_PRIV_CP15_c7_par64 + 0x4)]
	mcrr	p15, 0, r1, r2, c7	/* c7_par64 */
	ldr	r1, [r0, #ARM_PRIV_CP15_c10_prrr]
	mcr	p15, 0, r1, c10, c2, 0	/* c10_prrr */
	ldr	r1, [r0, #ARM_PRIV_CP15_c10_nmrr]
	mcr	p15, 0, r1, c10, c2, 1	/* c10_nmrr */
	ldr	r1, [r0, #ARM_PRIV_CP15_c12_vbar]
	mcr	p15, 0, r1, c12, c0, 0	/* c12_vbar */
	ldr	r1, [r0, #ARM_PRIV_CP15_c13_fcseidr]
	mcr	p15, 0, r1, c13, c0, 0	/* c13_fcseidr */
	ldr	r1, [r0, #ARM_PRIV_CP15_c13_contextidr]
	mcr	p15, 0, r1, c13, c0, 1	/* c13_contextidr */
	ldr	r1, [r0, #ARM_PRIV_CP15_c13_tls1]
	mcr	p15, 0, r1, c13, c0, 2	/* c13_tls1 */
	ldr	r1, [r0, #ARM_PRIV_CP15_c13_tls2]
	mcr	p15, 0, r1, c13, c0, 3	/* c13_tls2 */
	ldr	r1, [r0, #ARM_PRIV_CP15_c13_tls3]
	mcr	p15, 0, r1, c13, c0, 4	/* c13_tls3 */
	pop	{r1, r2}
	bx 	lr

	.globl generic_timer_regs_save
generic_timer_regs_save:
	push	{r1, r2}
	mrc	p15, 0, r1, c14, c2, 1	/* cntpctl */
	str	r1, [r0, #GENERIC_TIMER_CONTEXT_cntpctl]
	mrc	p15, 0, r1, c14, c3, 1	/* cntvctl */
	str	r1, [r0, #GENERIC_TIMER_CONTEXT_cntvctl]
	mrc	p15, 0, r1, c14, c1, 0	/* cntkctl */
	str	r1, [r0, #GENERIC_TIMER_CONTEXT_cntkctl]
	mrrc	p15, 2, r1, r2, c14	/* cntpcval */
	str	r1, [r0, #GENERIC_TIMER_CONTEXT_cntpcval]
	str	r2, [r0, #(GENERIC_TIMER_CONTEXT_cntpcval + 0x4)]
	mrrc	p15, 3, r1, r2, c14	/* cntvcval */
	str	r1, [r0, #GENERIC_TIMER_CONTEXT_cntvcval]
	str	r2, [r0, #(GENERIC_TIMER_CONTEXT_cntvcval + 0x4)]
	mov	r1, #GENERIC_TIMER_CTRL_IT_MASK
	mcr	p15, 0, r1, c14, c2, 1	/* cntpctl.it = 0 */
	mcr	p15, 0, r1, c14, c3, 1	/* cntvctl.it = 0 */
	pop	{r1, r2}
	bx 	lr

	.globl generic_timer_regs_restore
generic_timer_regs_restore:
	push	{r1, r2}
	ldr	r1, [r0, #GENERIC_TIMER_CONTEXT_cntvoff]
	ldr	r2, [r0, #(GENERIC_TIMER_CONTEXT_cntvoff + 0x4)]
	mcrr	p15, 4, r1, r2, c14	/* cntvoff */
	ldr	r1, [r0, #GENERIC_TIMER_CONTEXT_cntpcval]
	ldr	r2, [r0, #(GENERIC_TIMER_CONTEXT_cntpcval + 0x4)]
	mcrr	p15, 2, r1, r2, c14	/* cntpcval */
	ldr	r1, [r0, #GENERIC_TIMER_CONTEXT_cntvcval]
	ldr	r2, [r0, #(GENERIC_TIMER_CONTEXT_cntvcval + 0x4)]
	mcrr	p15, 3, r1, r2, c14	/* cntvcval */
	ldr	r1, [r0, #GENERIC_TIMER_CONTEXT_cntkctl]
	mcr	p15, 0, r1, c14, c1, 0	/* cntkctl */
	ldr	r1, [r0, #GENERIC_TIMER_CONTEXT_cntpctl]
	mcr	p15, 0, r1, c14, c2, 1	/* cntpctl */
	ldr	r1, [r0, #GENERIC_TIMER_CONTEXT_cntvctl]
	mcr	p15, 0, r1, c14, c3, 1	/* cntvctl */
	pop	{r1, r2}
	bx 	lr

