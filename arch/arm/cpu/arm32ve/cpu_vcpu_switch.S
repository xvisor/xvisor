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

	.globl cpu_vcpu_banked_regs_save
cpu_vcpu_banked_regs_save:
	push	{r1}
	mrs	r1, SP_usr	/* bnk->sp_usr */
	str	r1, [r0, #0x0]
	mrs	r1, SP_svc	/* bnk->sp_svc */
	str	r1, [r0, #0x4]
	mrs	r1, LR_svc	/* bnk->lr_svc */
	str	r1, [r0, #0x8]
	mrs	r1, SPSR_svc	/* bnk->spsr_svc */
	str	r1, [r0, #0xC]
	mrs	r1, SP_abt	/* bnk->sp_abt */
	str	r1, [r0, #0x10]
	mrs	r1, LR_abt	/* bnk->lr_abt */
	str	r1, [r0, #0x14]
	mrs	r1, SPSR_abt	/* bnk->spsr_abt */
	str	r1, [r0, #0x18]
	mrs	r1, SP_und	/* bnk->sp_und */
	str	r1, [r0, #0x1C]
	mrs	r1, LR_und	/* bnk->lr_und */
	str	r1, [r0, #0x20]
	mrs	r1, SPSR_und	/* bnk->spsr_und */
	str	r1, [r0, #0x24]
	mrs	r1, SP_irq	/* bnk->sp_irq */
	str	r1, [r0, #0x28]
	mrs	r1, LR_irq	/* bnk->lr_irq */
	str	r1, [r0, #0x2C]
	mrs	r1, SPSR_irq	/* bnk->spsr_irq */
	str	r1, [r0, #0x30]
	mrs	r1, r8_fiq	/* bnk->gpr_fiq[0] */
	str	r1, [r0, #0x34]
	mrs	r1, r9_fiq	/* bnk->gpr_fiq[1] */
	str	r1, [r0, #0x38]
	mrs	r1, r10_fiq	/* bnk->gpr_fiq[2] */
	str	r1, [r0, #0x3C]
	mrs	r1, r11_fiq	/* bnk->gpr_fiq[3] */
	str	r1, [r0, #0x40]
	mrs	r1, r12_fiq	/* bnk->gpr_fiq[4] */
	str	r1, [r0, #0x44]
	mrs	r1, SP_fiq 	/* bnk->sp_fiq */
	str	r1, [r0, #0x48]
	mrs	r1, LR_fiq	/* bnk->lr_fiq */
	str	r1, [r0, #0x4C]
	mrs	r1, SPSR_fiq	/* bnk->spsr_fiq */
	str	r1, [r0, #0x50]
	pop	{r1}
	bx 	lr

	.globl cpu_vcpu_banked_regs_restore
cpu_vcpu_banked_regs_restore:
	push	{r1}
	ldr	r1, [r0, #0x0]
	msr	SP_usr, r1	/* bnk->sp_usr */
	ldr	r1, [r0, #0x4]
	msr	SP_svc, r1	/* bnk->sp_svc */
	ldr	r1, [r0, #0x8]
	msr	LR_svc, r1	/* bnk->lr_svc */
	ldr	r1, [r0, #0xC]
	msr	SPSR_svc, r1	/* bnk->spsr_svc */
	ldr	r1, [r0, #0x10]
	msr	SP_abt, r1	/* bnk->sp_abt */
	ldr	r1, [r0, #0x14]
	msr	LR_abt, r1	/* bnk->lr_abt */
	ldr	r1, [r0, #0x18]
	msr	SPSR_abt, r1	/* bnk->spsr_abt */
	ldr	r1, [r0, #0x1C]
	msr	SP_und, r1	/* bnk->sp_und */
	ldr	r1, [r0, #0x20]
	msr	LR_und, r1	/* bnk->lr_und */
	ldr	r1, [r0, #0x24]
	msr	SPSR_und, r1	/* bnk->spsr_und */
	ldr	r1, [r0, #0x28]
	msr	SP_irq, r1	/* bnk->sp_irq */
	ldr	r1, [r0, #0x2C]
	msr	LR_irq, r1	/* bnk->lr_irq */
	ldr	r1, [r0, #0x30]
	msr	SPSR_irq, r1	/* bnk->spsr_irq */
	ldr	r1, [r0, #0x34]
	msr	r8_fiq, r1	/* bnk->gpr_fiq[0] */
	ldr	r1, [r0, #0x38]
	msr	r9_fiq, r1	/* bnk->gpr_fiq[1] */
	ldr	r1, [r0, #0x3C]
	msr	r10_fiq, r1	/* bnk->gpr_fiq[2] */
	ldr	r1, [r0, #0x40]
	msr	r11_fiq, r1	/* bnk->gpr_fiq[3] */
	ldr	r1, [r0, #0x44]
	msr	r12_fiq, r1	/* bnk->gpr_fiq[4] */
	ldr	r1, [r0, #0x48]
	msr	SP_fiq, r1 	/* bnk->sp_fiq */
	ldr	r1, [r0, #0x4C]
	msr	LR_fiq, r1	/* bnk->lr_fiq */
	ldr	r1, [r0, #0x50]
	msr	SPSR_fiq, r1	/* bnk->spsr_fiq */
	pop	{r1}
	bx 	lr

