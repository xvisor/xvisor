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
 * @file cpu_vcpu_emulate_arm.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of non-hardware assisted ARM instruction emulation
 */

#include <vmm_types.h>
#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_vcpu_irq.h>
#include <vmm_host_aspace.h>
#include <cpu_inline_asm.h>
#include <cpu_vcpu_helper.h>
#include <cpu_vcpu_cp15.h>
#include <cpu_vcpu_emulate_arm.h>

#define arm_zero_extend(imm, bits)		((u32)(imm))
#define arm_align(addr, nbytes)			((addr) - ((addr) % (nbytes)))

static void arm_unpredictable(arch_regs_t * regs, 
			      struct vmm_vcpu * vcpu, 
			      u32 inst,
			      const char *reason)
{
	vmm_printf("Unprecidable Instruction 0x%08x\n", inst);
	vmm_printf("Reason: %s\n", reason);
	cpu_vcpu_halt(vcpu, regs);
}

static inline u32 arm_sign_extend(u32 imm, u32 len, u32 bits)
{
	if (imm & (1 << (len - 1))) {
		imm = imm | (~((1 << len) - 1));
	}
	return imm & ((1 << bits) - 1);
}

static bool arm_condition_check(u32 cond, arch_regs_t * regs)
{
	bool ret = FALSE;
	if (cond == 0xE) {
		return TRUE;
	}
	switch (cond >> 1) {
	case 0:
		ret = (regs->cpsr & CPSR_ZERO_MASK) ? TRUE : FALSE;
		break;
	case 1:
		ret = (regs->cpsr & CPSR_CARRY_MASK) ? TRUE : FALSE;
		break;
	case 2:
		ret = (regs->cpsr & CPSR_NEGATIVE_MASK) ? TRUE : FALSE;
		break;
	case 3:
		ret = (regs->cpsr & CPSR_OVERFLOW_MASK) ? TRUE : FALSE;
		break;
	case 4:
		ret = (regs->cpsr & CPSR_CARRY_MASK) ? TRUE : FALSE;
		ret = (ret && !(regs->cpsr & CPSR_ZERO_MASK)) ? 
								TRUE : FALSE;
		break;
	case 5:
		if (regs->cpsr & CPSR_NEGATIVE_MASK) {
			ret = (regs->cpsr & CPSR_OVERFLOW_MASK) ? 
								TRUE : FALSE;
		} else {
			ret = (regs->cpsr & CPSR_OVERFLOW_MASK) ? 
								FALSE : TRUE;
		}
		break;
	case 6:
		if (regs->cpsr & CPSR_NEGATIVE_MASK) {
			ret = (regs->cpsr & CPSR_OVERFLOW_MASK) ? 
								TRUE : FALSE;
		} else {
			ret = (regs->cpsr & CPSR_OVERFLOW_MASK) ? 
								FALSE : TRUE;
		}
		ret = (ret && !(regs->cpsr & CPSR_ZERO_MASK)) ? 
								TRUE : FALSE;
		break;
	case 7:
		ret = TRUE;
		break;
	default:
		break;
	};
	if ((cond & 0x1) && (cond != 0xF)) {
		ret = !ret;
	}
	return ret;
}

#define arm_condition_passed(cond, regs) (((cond) == 0xE) ? \
					 TRUE : \
					 arm_condition_check((cond), (regs)))

static u32 arm_decode_imm_shift(u32 type, u32 imm5, u32 *shift_t)
{
	switch (type) {
	case 0:
		*shift_t = arm_shift_lsl;
		return imm5;
		break;
	case 1:
		*shift_t = arm_shift_lsr;
		return (imm5) ? imm5 : 32;
		break;
	case 2:
		*shift_t = arm_shift_asr;
		return (imm5) ? imm5 : 32;
		break;
	case 3:
		if (imm5) {
			*shift_t = arm_shift_ror;
			return imm5;
		} else {
			*shift_t = arm_shift_rrx;
			return 1;
		}
		break;
	default:
		break;
	};
	return 0;
}

static u32 arm_shift_c(u32 val, u32 shift_t, u32 shift_n, u32 cin, u32 *cout)
{
	u64 rval;
	u32 carry = cin;
	if (shift_n) {
		switch (shift_t) {
		case arm_shift_lsl:
			rval = val;
			rval = rval << shift_n;
			carry = (rval >> 32) & 0x1;
			val = rval;
			break;
		case arm_shift_lsr:
			rval = val;
			rval = rval >> (shift_n - 1);
			carry = rval & 0x1;
			val = (rval >> 1);
			break;
		case arm_shift_asr:
			rval = val;
			if (val & 0x80000000) {
				rval |= 0xFFFFFFFF00000000ULL;
			}
			rval = rval >> (shift_n - 1);
			carry = rval & 0x1;
			val = (rval >> 1);
			break;
		case arm_shift_ror:
			val = (val >> (shift_n % 32)) | 
				(val << (32 - (shift_n % 32)));
			carry = (val >> 31);
			break;
		case arm_shift_rrx:
			carry = val & 0x1;
			val = (cin << 31) | (val >> 1);
			break;
		};
	}
	if (cout) {
		*cout = carry;
	}
	return val;
}

static inline u32 arm_shift(u32 val, u32 shift_t, u32 shift_n, u32 cin)
{
	return arm_shift_c(val, shift_t, shift_n, cin, NULL);
}

static inline u32 arm_expand_imm_c(u32 imm12, u32 cin, u32 *cout)
{
	return arm_shift_c((imm12 & 0xFF), 
			   arm_shift_ror, 
			   2 * (imm12 >> 8), 
			   cin, cout);
}

static inline u32 arm_expand_imm(arch_regs_t * regs, u32 imm12)
{
	return arm_expand_imm_c(imm12, 
				(regs->cpsr >> CPSR_CARRY_SHIFT) & 0x1,
				NULL);
}

/** Emulate hypercall instruction */
static int arm_instgrp_hypercall(u32 inst, 
				arch_regs_t * regs, struct vmm_vcpu * vcpu)
{
	/* Don't need to emulate this instruction group */
	return VMM_EFAIL;
}

/** Emulate data processing instructions */
static int arm_instgrp_dataproc(u32 inst, 
				arch_regs_t *regs, struct vmm_vcpu *vcpu)
{
	/* Don't need to emulate this instruction group */
	return VMM_EFAIL;
}

/** Emulate 'str (immediate)' instruction */
static int arm_inst_str_i(u32 inst, 
				arch_regs_t * regs, struct vmm_vcpu * vcpu)
{
	u32 data;
	register int rc;
	register u32 cond, P, U, W, Rn, Rt, imm32;
	register u32 address, offset_addr;
	arm_funcstat_start(vcpu, ARM_FUNCSTAT_STR_I);
	cond = ARM_INST_DECODE(inst, ARM_INST_COND_MASK, ARM_INST_COND_SHIFT);
	P = ARM_INST_BIT(inst, ARM_INST_LDRSTR_P_START);
	U = ARM_INST_BIT(inst, ARM_INST_LDRSTR_U_START);
	W = ARM_INST_BIT(inst, ARM_INST_LDRSTR_W_START);
	Rn = ARM_INST_BITS(inst,
			   ARM_INST_LDRSTR_RN_END,
			   ARM_INST_LDRSTR_RN_START);
	Rt = ARM_INST_BITS(inst,
			   ARM_INST_LDRSTR_RT_END,
			   ARM_INST_LDRSTR_RT_START);
	imm32 = ARM_INST_BITS(inst,
			      ARM_INST_LDRSTR_IMM12_END,
			      ARM_INST_LDRSTR_IMM12_START);
	imm32 = arm_zero_extend(imm32, 32);
	if ((P == 0) && (W == 1)) {
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	}
	if (((P == 0) || (W == 1)) && ((Rn == 15) || (Rn == Rt))) {
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	}
	if (arm_condition_passed(cond, regs)) {
		address = cpu_vcpu_reg_read(vcpu, regs, Rn);
		offset_addr = (U == 1) ? (address + imm32) : (address - imm32);
		address = (P == 1) ? offset_addr : address;
		data = cpu_vcpu_reg_read(vcpu, regs, Rt);
		if ((rc = cpu_vcpu_cp15_mem_write(vcpu, regs, address, 
						  &data, 4, FALSE))) {
			return rc;
		}
		if ((P == 0) || (W == 1)) {
			cpu_vcpu_reg_write(vcpu, regs, Rn, offset_addr);
		}
	}
	regs->pc += 4;
	arm_funcstat_end(vcpu, ARM_FUNCSTAT_STR_I);
	return VMM_OK;
}

/** Emulate 'str (register)' instruction */
static int arm_inst_str_r(u32 inst, 
				arch_regs_t * regs, struct vmm_vcpu * vcpu)
{
	int rc;
	u32 cond, P, U, W, Rn, Rt, imm5, type, Rm;
	u32 shift_t, shift_n, offset, offset_addr, address, data;
	bool index, add, wback;
	arm_funcstat_start(vcpu, ARM_FUNCSTAT_STR_R);
	cond = ARM_INST_DECODE(inst, ARM_INST_COND_MASK, ARM_INST_COND_SHIFT);
	P = ARM_INST_BITS(inst,
			  ARM_INST_LDRSTR_P_END,
			  ARM_INST_LDRSTR_P_START);
	U = ARM_INST_BITS(inst,
			  ARM_INST_LDRSTR_U_END,
			  ARM_INST_LDRSTR_U_START);
	W = ARM_INST_BITS(inst,
			  ARM_INST_LDRSTR_W_END,
			  ARM_INST_LDRSTR_W_START);
	Rn = ARM_INST_BITS(inst,
			   ARM_INST_LDRSTR_RN_END,
			   ARM_INST_LDRSTR_RN_START);
	Rt = ARM_INST_BITS(inst,
			   ARM_INST_LDRSTR_RT_END,
			   ARM_INST_LDRSTR_RT_START);
	imm5 = ARM_INST_BITS(inst,
			     ARM_INST_LDRSTR_IMM5_END,
			     ARM_INST_LDRSTR_IMM5_START);
	type = ARM_INST_BITS(inst,
			     ARM_INST_LDRSTR_TYPE_END,
			     ARM_INST_LDRSTR_TYPE_START);
	Rm = ARM_INST_BITS(inst,
			   ARM_INST_LDRSTR_RM_END,
			   ARM_INST_LDRSTR_RM_START);
	if ((P == 0) && (W == 1)) {
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	}
	index = (P == 1) ? TRUE : FALSE;
	add = (U == 1) ? TRUE : FALSE;
	wback = ((P == 0) || (W == 1)) ? TRUE : FALSE;
	shift_n = arm_decode_imm_shift(type, imm5, &shift_t);
	if (Rm == 15) {
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	}
	if (wback && (Rn == 15 || Rn == Rt)) {
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	}
	if (arm_condition_passed(cond, regs)) {
		offset = arm_shift(cpu_vcpu_reg_read(vcpu, regs, Rm), 
				   shift_t, 
				   shift_n, 
				  (regs->cpsr >> CPSR_CARRY_SHIFT) & 0x1);
		address = cpu_vcpu_reg_read(vcpu, regs, Rn);
		offset_addr = (add) ? (address + offset) : (address - offset);
		address = (index) ? offset_addr : address;
		data = cpu_vcpu_reg_read(vcpu, regs, Rt);
		if ((rc = cpu_vcpu_cp15_mem_write(vcpu, regs, address, 
						  &data, 4, FALSE))) {
			return rc;
		}
		if (wback) {
			cpu_vcpu_reg_write(vcpu, regs, Rn, offset_addr);
		}
	}
	regs->pc += 4;
	arm_funcstat_end(vcpu, ARM_FUNCSTAT_STR_R);
	return VMM_OK;
}

/** Emulate 'strb (immediate)' instruction */
static int arm_inst_strb_i(u32 inst, 
				arch_regs_t * regs, struct vmm_vcpu * vcpu)
{
	int rc;
	u32 cond, P, U, W, Rn, Rt, imm12;
	u32 imm32, offset_addr, address; u8 data;
	bool index, add, wback;
	arm_funcstat_start(vcpu, ARM_FUNCSTAT_STRB_I);
	cond = ARM_INST_DECODE(inst, ARM_INST_COND_MASK, ARM_INST_COND_SHIFT);
	P = ARM_INST_BITS(inst,
			  ARM_INST_LDRSTR_P_END,
			  ARM_INST_LDRSTR_P_START);
	U = ARM_INST_BITS(inst,
			  ARM_INST_LDRSTR_U_END,
			  ARM_INST_LDRSTR_U_START);
	W = ARM_INST_BITS(inst,
			  ARM_INST_LDRSTR_W_END,
			  ARM_INST_LDRSTR_W_START);
	Rn = ARM_INST_BITS(inst,
			   ARM_INST_LDRSTR_RN_END,
			   ARM_INST_LDRSTR_RN_START);
	Rt = ARM_INST_BITS(inst,
			   ARM_INST_LDRSTR_RT_END,
			   ARM_INST_LDRSTR_RT_START);
	imm12 = ARM_INST_BITS(inst,
			      ARM_INST_LDRSTR_IMM12_END,
			      ARM_INST_LDRSTR_IMM12_START);
	if ((P == 0) && (W == 1)) {
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	}
	imm32 = arm_zero_extend(imm12, 32);
	index = (P == 1) ? TRUE : FALSE;
	add = (U == 1) ? TRUE : FALSE;
	wback = ((P == 0) || (W == 1)) ? TRUE : FALSE;
	if (Rt == 15) {
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	}
	if (wback && ((Rn == 15) || (Rn == Rt))) {
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	}
	if (arm_condition_passed(cond, regs)) {
		address = cpu_vcpu_reg_read(vcpu, regs, Rn);
		offset_addr = (add) ? (address + imm32) : (address - imm32);
		address = (index) ? offset_addr : address;
		data = cpu_vcpu_reg_read(vcpu, regs, Rt) & 0xFF;
		if ((rc = cpu_vcpu_cp15_mem_write(vcpu, regs, address, 
						  &data, 1, FALSE))) {
			return rc;
		}
		if (wback) {
			cpu_vcpu_reg_write(vcpu, regs, Rn, offset_addr);
		}
	}
	regs->pc += 4;
	arm_funcstat_end(vcpu, ARM_FUNCSTAT_STRB_I);
	return VMM_OK;
}

/** Emulate 'strb (register)' instruction */
static int arm_inst_strb_r(u32 inst, 
				arch_regs_t * regs, struct vmm_vcpu * vcpu)
{
	int rc;
	u32 cond, P, U, W, Rn, Rt, imm5, type, Rm;
	u32 shift_t, shift_n, offset, offset_addr, address; u8 data;
	bool index, add, wback;
	arm_funcstat_start(vcpu, ARM_FUNCSTAT_STRB_R);
	cond = ARM_INST_DECODE(inst, ARM_INST_COND_MASK, ARM_INST_COND_SHIFT);
	P = ARM_INST_BITS(inst,
			  ARM_INST_LDRSTR_P_END,
			  ARM_INST_LDRSTR_P_START);
	U = ARM_INST_BITS(inst,
			  ARM_INST_LDRSTR_U_END,
			  ARM_INST_LDRSTR_U_START);
	W = ARM_INST_BITS(inst,
			  ARM_INST_LDRSTR_W_END,
			  ARM_INST_LDRSTR_W_START);
	Rn = ARM_INST_BITS(inst,
			   ARM_INST_LDRSTR_RN_END,
			   ARM_INST_LDRSTR_RN_START);
	Rt = ARM_INST_BITS(inst,
			   ARM_INST_LDRSTR_RT_END,
			   ARM_INST_LDRSTR_RT_START);
	imm5 = ARM_INST_BITS(inst,
			     ARM_INST_LDRSTR_IMM5_END,
			     ARM_INST_LDRSTR_IMM5_START);
	type = ARM_INST_BITS(inst,
			     ARM_INST_LDRSTR_TYPE_END,
			     ARM_INST_LDRSTR_TYPE_START);
	Rm = ARM_INST_BITS(inst,
			   ARM_INST_LDRSTR_RM_END,
			   ARM_INST_LDRSTR_RM_START);
	if ((P == 0) && (W == 1)) {
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	}
	index = (P == 1) ? TRUE : FALSE;
	add = (U == 1) ? TRUE : FALSE;
	wback = ((P == 0) || (W == 1)) ? TRUE : FALSE;
	shift_n = arm_decode_imm_shift(type, imm5, &shift_t);
	if ((Rt == 15) || (Rm == 15)) {
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	}
	if (wback && (Rn == 15 || Rn == Rt)) {
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	}
	if (arm_condition_passed(cond, regs)) {
		offset = arm_shift(cpu_vcpu_reg_read(vcpu, regs, Rm), 
				   shift_t, 
				   shift_n, 
				  (regs->cpsr & CPSR_CARRY_SHIFT) & 0x1);
		address = cpu_vcpu_reg_read(vcpu, regs, Rn);
		offset_addr = (add) ? (address + offset) : 
					(address - offset);
		address = (index) ? offset_addr : address;
		data = cpu_vcpu_reg_read(vcpu, regs, Rt) & 0xFF;
		if ((rc = cpu_vcpu_cp15_mem_write(vcpu, regs, address, 
						  &data, 1, FALSE))) {
			return rc;
		}
		if (wback) {
			cpu_vcpu_reg_write(vcpu, regs, Rn, offset_addr);
		}
	}
	regs->pc += 4;
	arm_funcstat_end(vcpu, ARM_FUNCSTAT_STRB_R);
	return VMM_OK;
}

/** Emulate 'ldr (immediate)' instruction */
static int arm_inst_ldr_i(u32 inst, 
				arch_regs_t * regs, struct vmm_vcpu * vcpu)
{
	u32 data;
	register int rc;
	register u32 cond, P, U, W, Rn, Rt, imm32;
	register u32 offset_addr, address;
	arm_funcstat_start(vcpu, ARM_FUNCSTAT_LDR_I);
	cond = ARM_INST_DECODE(inst, ARM_INST_COND_MASK, ARM_INST_COND_SHIFT);
	P = ARM_INST_BIT(inst, ARM_INST_LDRSTR_P_START);
	W = ARM_INST_BIT(inst, ARM_INST_LDRSTR_W_START);
	Rn = ARM_INST_BITS(inst,
			   ARM_INST_LDRSTR_RN_END,
			   ARM_INST_LDRSTR_RN_START);
	Rt = ARM_INST_BITS(inst,
			   ARM_INST_LDRSTR_RT_END,
			   ARM_INST_LDRSTR_RT_START);
	if ((P == 0) && (W == 1)) {
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	}
	if (((P == 0) || (W == 1)) && (Rn == Rt)) {
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	}
	if (arm_condition_passed(cond, regs)) {
		U = ARM_INST_BIT(inst, ARM_INST_LDRSTR_U_START);
		imm32 = ARM_INST_BITS(inst,
			      ARM_INST_LDRSTR_IMM12_END,
			      ARM_INST_LDRSTR_IMM12_START);
		imm32 = arm_zero_extend(imm32, 32);
		address = cpu_vcpu_reg_read(vcpu, regs, Rn);
		offset_addr = (U == 1) ? (address + imm32) : 
					(address - imm32);
		address = (P == 1) ? offset_addr : address;
		data = 0x0;
		if ((rc = cpu_vcpu_cp15_mem_read(vcpu, regs, address, 
						 &data, 4, FALSE))) {
			return rc;
		}
		cpu_vcpu_reg_write(vcpu, regs, Rt, data);
		if ((P == 0) || (W == 1)) {
			cpu_vcpu_reg_write(vcpu, regs, Rn, offset_addr);
		}
	}
	regs->pc += 4;
	arm_funcstat_end(vcpu, ARM_FUNCSTAT_LDR_I);
	return VMM_OK;
}

/** Emulate 'ldr (literal)' instruction */
static int arm_inst_ldr_l(u32 inst, 
				arch_regs_t * regs, struct vmm_vcpu * vcpu)
{
	int rc;
	u32 cond, U, Rt, imm12;
	u32 imm32, address, data;
	bool add;
	arm_funcstat_start(vcpu, ARM_FUNCSTAT_LDR_L);
	cond = ARM_INST_DECODE(inst, ARM_INST_COND_MASK, ARM_INST_COND_SHIFT);
	U = ARM_INST_BITS(inst,
			  ARM_INST_LDRSTR_U_END,
			  ARM_INST_LDRSTR_U_START);
	Rt = ARM_INST_BITS(inst,
			   ARM_INST_LDRSTR_RT_END,
			   ARM_INST_LDRSTR_RT_START);
	imm12 = ARM_INST_BITS(inst,
			      ARM_INST_LDRSTR_IMM12_END,
			      ARM_INST_LDRSTR_IMM12_START);
	imm32 = arm_zero_extend(imm12, 32);
	add = (U == 1) ? TRUE : FALSE;
	if (arm_condition_passed(cond, regs)) {
		address = arm_align(regs->pc, 4);
		address = (add) ? (address + imm32) : (address - imm32);
		data = 0x0;
		if ((rc = cpu_vcpu_cp15_mem_read(vcpu, regs, address, 
						 &data, 4, FALSE))) {
			return rc;
		}
		cpu_vcpu_reg_write(vcpu, regs, Rt, data);
	}
	regs->pc += 4;
	arm_funcstat_end(vcpu, ARM_FUNCSTAT_LDR_L);
	return VMM_OK;
}

/** Emulate 'ldr (register)' instruction */
static int arm_inst_ldr_r(u32 inst, 
				arch_regs_t * regs, struct vmm_vcpu * vcpu)
{
	int rc;
	u32 cond, P, U, W, Rn, Rt, imm5, type, Rm;
	u32 shift_t, shift_n, offset, offset_addr, address, data;
	bool index, add, wback;
	arm_funcstat_start(vcpu, ARM_FUNCSTAT_LDR_R);
	cond = ARM_INST_DECODE(inst, ARM_INST_COND_MASK, ARM_INST_COND_SHIFT);
	P = ARM_INST_BITS(inst,
			  ARM_INST_LDRSTR_P_END,
			  ARM_INST_LDRSTR_P_START);
	U = ARM_INST_BITS(inst,
			  ARM_INST_LDRSTR_U_END,
			  ARM_INST_LDRSTR_U_START);
	W = ARM_INST_BITS(inst,
			  ARM_INST_LDRSTR_W_END,
			  ARM_INST_LDRSTR_W_START);
	Rn = ARM_INST_BITS(inst,
			   ARM_INST_LDRSTR_RN_END,
			   ARM_INST_LDRSTR_RN_START);
	Rt = ARM_INST_BITS(inst,
			   ARM_INST_LDRSTR_RT_END,
			   ARM_INST_LDRSTR_RT_START);
	imm5 = ARM_INST_BITS(inst,
			     ARM_INST_LDRSTR_IMM5_END,
			     ARM_INST_LDRSTR_IMM5_START);
	type = ARM_INST_BITS(inst,
			     ARM_INST_LDRSTR_TYPE_END,
			     ARM_INST_LDRSTR_TYPE_START);
	Rm = ARM_INST_BITS(inst,
			   ARM_INST_LDRSTR_RM_END,
			   ARM_INST_LDRSTR_RM_START);
	if ((P == 0) && (W == 1)) {
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	}
	index = (P == 1) ? TRUE : FALSE;
	add = (U == 1) ? TRUE : FALSE;
	wback = ((P == 0) || (W == 1)) ? TRUE : FALSE;
	shift_n = arm_decode_imm_shift(type, imm5, &shift_t);
	if (Rm == 15) {
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	}
	if (wback && (Rn == 15 || Rn == Rt)) {
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	}
	if (arm_condition_passed(cond, regs)) {
		offset = arm_shift(cpu_vcpu_reg_read(vcpu, regs, Rm), 
				   shift_t, 
				   shift_n, 
				  (regs->cpsr >> CPSR_CARRY_SHIFT) & 0x1);
		address = cpu_vcpu_reg_read(vcpu, regs, Rn);
		offset_addr = (add) ? (address + offset) : 
					(address - offset);
		address = (index) ? offset_addr : address;
		data = 0x0;
		if ((rc = cpu_vcpu_cp15_mem_read(vcpu, regs, address, 
						 &data, 4, FALSE))) {
			return rc;
		}
		cpu_vcpu_reg_write(vcpu, regs, Rt, data);
		if (wback) {
			cpu_vcpu_reg_write(vcpu, regs, Rn, offset_addr);
		}
	}
	regs->pc += 4;
	arm_funcstat_end(vcpu, ARM_FUNCSTAT_LDR_R);
	return VMM_OK;
}

/** Emulate 'ldrb (immediate)' instruction */
static int arm_inst_ldrb_i(u32 inst, 
				arch_regs_t * regs, struct vmm_vcpu * vcpu)
{
	int rc;
	u32 cond, P, U, W, Rn, Rt, imm12;
	u32 imm32, offset_addr, address; u8 data;
	bool index, add, wback;
	arm_funcstat_start(vcpu, ARM_FUNCSTAT_LDRB_I);
	cond = ARM_INST_DECODE(inst, ARM_INST_COND_MASK, ARM_INST_COND_SHIFT);
	P = ARM_INST_BITS(inst,
			  ARM_INST_LDRSTR_P_END,
			  ARM_INST_LDRSTR_P_START);
	U = ARM_INST_BITS(inst,
			  ARM_INST_LDRSTR_U_END,
			  ARM_INST_LDRSTR_U_START);
	W = ARM_INST_BITS(inst,
			  ARM_INST_LDRSTR_W_END,
			  ARM_INST_LDRSTR_W_START);
	Rn = ARM_INST_BITS(inst,
			   ARM_INST_LDRSTR_RN_END,
			   ARM_INST_LDRSTR_RN_START);
	Rt = ARM_INST_BITS(inst,
			   ARM_INST_LDRSTR_RT_END,
			   ARM_INST_LDRSTR_RT_START);
	imm12 = ARM_INST_BITS(inst,
			      ARM_INST_LDRSTR_IMM12_END,
			      ARM_INST_LDRSTR_IMM12_START);
	if ((P == 0) && (W == 1)) {
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	}
	imm32 = arm_zero_extend(imm12, 32);
	index = (P == 1) ? TRUE : FALSE;
	add = (U == 1) ? TRUE : FALSE;
	wback = ((P == 0) || (W == 1)) ? TRUE : FALSE;
	if (Rt == 15) {
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	}
	if (wback && (Rn == Rt)) {
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	}
	if (arm_condition_passed(cond, regs)) {
		address = cpu_vcpu_reg_read(vcpu, regs, Rn);
		offset_addr = (add) ? (address + imm32) : 
					(address - imm32);
		address = (index) ? offset_addr : address;
		data = 0x0;
		if ((rc = cpu_vcpu_cp15_mem_read(vcpu, regs, address, 
						 &data, 1, FALSE))) {
			return rc;
		}
		cpu_vcpu_reg_write(vcpu, regs, Rt, arm_zero_extend(data, 32));
		if (wback) {
			cpu_vcpu_reg_write(vcpu, regs, Rn, offset_addr);
		}
	}
	regs->pc += 4;
	arm_funcstat_end(vcpu, ARM_FUNCSTAT_LDRB_I);
	return VMM_OK;
}

/** Emulate 'ldrb (literal)' instruction */
static int arm_inst_ldrb_l(u32 inst, 
				arch_regs_t * regs, struct vmm_vcpu * vcpu)
{
	int rc;
	u32 cond, U, Rt, imm12;
	u32 imm32, address; u8 data;
	bool add;
	arm_funcstat_start(vcpu, ARM_FUNCSTAT_LDRB_L);
	cond = ARM_INST_DECODE(inst, ARM_INST_COND_MASK, ARM_INST_COND_SHIFT);
	U = ARM_INST_BITS(inst,
			  ARM_INST_LDRSTR_U_END,
			  ARM_INST_LDRSTR_U_START);
	Rt = ARM_INST_BITS(inst,
			   ARM_INST_LDRSTR_RT_END,
			   ARM_INST_LDRSTR_RT_START);
	imm12 = ARM_INST_BITS(inst,
			      ARM_INST_LDRSTR_IMM12_END,
			      ARM_INST_LDRSTR_IMM12_START);
	imm32 = arm_zero_extend(imm12, 32);
	add = (U == 1) ? TRUE : FALSE;
	if (Rt == 15) {
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	}
	if (arm_condition_passed(cond, regs)) {
		address = arm_align(regs->pc, 4);
		address = (add) ? (address + imm32) : (address - imm32);
		data = 0x0;
		if ((rc = cpu_vcpu_cp15_mem_read(vcpu, regs, address, 
						 &data, 1, FALSE))) {
			return rc;
		}
		cpu_vcpu_reg_write(vcpu, regs, Rt, arm_zero_extend(data, 32));
	}
	regs->pc += 4;
	arm_funcstat_end(vcpu, ARM_FUNCSTAT_LDRB_L);
	return VMM_OK;
}

/** Emulate 'ldrb (register)' instruction */
static int arm_inst_ldrb_r(u32 inst, 
				arch_regs_t * regs, struct vmm_vcpu * vcpu)
{
	int rc;
	u32 cond, P, U, W, Rn, Rt, imm5, type, Rm;
	u32 shift_t, shift_n, offset, offset_addr, address; u8 data;
	bool index, add, wback;
	arm_funcstat_start(vcpu, ARM_FUNCSTAT_LDRB_R);
	cond = ARM_INST_DECODE(inst, ARM_INST_COND_MASK, ARM_INST_COND_SHIFT);
	P = ARM_INST_BITS(inst,
			  ARM_INST_LDRSTR_P_END,
			  ARM_INST_LDRSTR_P_START);
	U = ARM_INST_BITS(inst,
			  ARM_INST_LDRSTR_U_END,
			  ARM_INST_LDRSTR_U_START);
	W = ARM_INST_BITS(inst,
			  ARM_INST_LDRSTR_W_END,
			  ARM_INST_LDRSTR_W_START);
	Rn = ARM_INST_BITS(inst,
			   ARM_INST_LDRSTR_RN_END,
			   ARM_INST_LDRSTR_RN_START);
	Rt = ARM_INST_BITS(inst,
			   ARM_INST_LDRSTR_RT_END,
			   ARM_INST_LDRSTR_RT_START);
	imm5 = ARM_INST_BITS(inst,
			     ARM_INST_LDRSTR_IMM5_END,
			     ARM_INST_LDRSTR_IMM5_START);
	type = ARM_INST_BITS(inst,
			     ARM_INST_LDRSTR_TYPE_END,
			     ARM_INST_LDRSTR_TYPE_START);
	Rm = ARM_INST_BITS(inst,
			   ARM_INST_LDRSTR_RM_END,
			   ARM_INST_LDRSTR_RM_START);
	if ((P == 0) && (W == 1)) {
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	}
	index = (P == 1) ? TRUE : FALSE;
	add = (U == 1) ? TRUE : FALSE;
	wback = ((P == 0) || (W == 1)) ? TRUE : FALSE;
	shift_n = arm_decode_imm_shift(type, imm5, &shift_t);
	if ((Rt == 15) || (Rm == 15)) {
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	}
	if (wback && (Rn == 15 || Rn == Rt)) {
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	}
	if (arm_condition_passed(cond, regs)) {
		offset = arm_shift(cpu_vcpu_reg_read(vcpu, regs, Rm), 
				   shift_t, 
				   shift_n, 
				  (regs->cpsr >> CPSR_CARRY_SHIFT) & 0x1);
		address = cpu_vcpu_reg_read(vcpu, regs, Rn);
		offset_addr = (add) ? (address + offset) : 
					(address - offset);
		address = (index) ? offset_addr : address;
		data = 0x0;
		if ((rc = cpu_vcpu_cp15_mem_read(vcpu, regs, address, 
						 &data, 1, FALSE))) {
			return rc;
		}
		cpu_vcpu_reg_write(vcpu, regs, Rt, arm_zero_extend(data, 32));
		if (wback) {
			cpu_vcpu_reg_write(vcpu, regs, Rn, offset_addr);
		}
	}
	regs->pc += 4;
	arm_funcstat_end(vcpu, ARM_FUNCSTAT_LDRB_R);
	return VMM_OK;
}

/** Emulate load/store instructions */
static int arm_instgrp_ldrstr(u32 inst, 
				arch_regs_t *regs, struct vmm_vcpu *vcpu)
{
	u32 op1, rn;
	u32 is_xx0x0, is_0x010, is_xx0x1, is_0x011, is_xx1x0, is_0x110;
	u32 is_xx1x1, is_0x111;

	op1 = ARM_INST_DECODE(inst,
			      ARM_INST_LDRSTR_OP1_MASK,
			      ARM_INST_LDRSTR_OP1_SHIFT);

	if (!(inst & ARM_INST_LDRSTR_A_MASK)) {
		is_0x010 = !(op1 & 0x5) && !(op1 & 0x10) && (op1 & 0x2);
		if (!is_0x010) {
			is_xx0x0 = !(op1 & 0x5);
			if (is_xx0x0) {
				/* STR (immediate) */
				return arm_inst_str_i(inst, regs, vcpu);
			}
		} else {
			/* STRT */
			/* FIXME: Not available */
			return VMM_EFAIL;
		}
		is_0x011 = !(op1 & 0x14) && (op1 & 0x3);
		if (!is_0x011) {
			is_xx0x1 = !(op1 & 0x4) && (op1 & 0x1);
			if (is_xx0x1) {
				rn = ARM_INST_DECODE(inst,
			     			ARM_INST_LDRSTR_RN_MASK, 
			     			ARM_INST_LDRSTR_RN_SHIFT);
				if (rn == 0xF) {
					/* LDR (literal) */
					return arm_inst_ldr_l(inst, regs, vcpu);
				} else {
					/* LDR (immediate) */
					return arm_inst_ldr_i(inst, regs, vcpu);
				}
			}
		} else {
			/* LDRT */
			/* FIXME: Not available */
			return VMM_EFAIL;
		}
		is_0x110 = !(op1 & 0x11) && (op1 & 0x6);
		if (!is_0x110) {
			is_xx1x0 = (op1 & 0x4) && !(op1 & 0x1);
			if (is_xx1x0) {
				/* STRB (immediate) */
				return arm_inst_strb_i(inst, regs, vcpu);
			}
		} else {
			/* STRBT */
			/* FIXME: Not available */
			return VMM_EFAIL;
		} 
		is_0x111 = !(op1 & 0x10) && (op1 & 0x7);
		if (!is_0x111) {
			is_xx1x1 = (op1 & 0x5);
			if (is_xx1x1) {
				rn = ARM_INST_DECODE(inst,
			 			ARM_INST_LDRSTR_RN_MASK, 
			     			ARM_INST_LDRSTR_RN_SHIFT);
				if (rn == 0xF) {
					/* LDRB (literal) */
					return arm_inst_ldrb_l(inst, regs, vcpu);
				} else {
					/* LDRB (immediate) */
					return arm_inst_ldrb_i(inst, regs, vcpu);
				}
			}
		} else {
			/* LDRBT */
			/* FIXME: Not available */
			return VMM_EFAIL;
		}
	} else if (!(inst & ARM_INST_LDRSTR_B_MASK)) {
		is_0x010 = !(op1 & 0x5) && !(op1 & 0x10) && (op1 & 0x2);
		if (!is_0x010) {
			is_xx0x0 = !(op1 & 0x5);
			if (is_xx0x0) {
				/* STR (register) */
				return arm_inst_str_r(inst, regs, vcpu);
			}
		} else {
			/* STRT */
			/* FIXME: Not available */
			return VMM_EFAIL;
		}
		is_0x011 = !(op1 & 0x14) && (op1 & 0x3);
		if (!is_0x011) {
			is_xx0x1 = !(op1 & 0x4) && (op1 & 0x1);
			if (is_xx0x1) {
				/* LDR (register) */
				return arm_inst_ldr_r(inst, regs, vcpu);
			}
		} else {
			/* LDRT */
			/* FIXME: Not available */
			return VMM_EFAIL;
		}
		is_0x110 = !(op1 & 0x11) && (op1 & 0x6);
		if (!is_0x110) {
			is_xx1x0 = (op1 & 0x4) && !(op1 & 0x1);
			if (is_xx1x0) {
				/* STRB (register) */
				return arm_inst_strb_r(inst, regs, vcpu);
			}
		} else {
			/* STRBT */
			/* FIXME: Not available */
			return VMM_EFAIL;
		} 
		is_0x111 = !(op1 & 0x10) && (op1 & 0x7);
		if (!is_0x111) {
			is_xx1x1 = (op1 & 0x5);
			if (is_xx1x1) {
				/* LDRB (register) */
				return arm_inst_ldrb_r(inst, regs, vcpu);
			} 
		} else {
			/* LDRBT */
			/* FIXME: Not available */
			return VMM_EFAIL;
		}
	}
	arm_unpredictable(regs, vcpu, inst, __func__);
	return VMM_EFAIL;
}

/** Emulate media instructions */
static int arm_instgrp_media(u32 inst, 
				arch_regs_t *regs, struct vmm_vcpu *vcpu)
{
	/* Don't need to emulate this instruction group */
	return VMM_EFAIL;
}

/** Emulate branch, branch with link, and block transfer instructions */
static int arm_instgrp_brblk(u32 inst, 
				arch_regs_t *regs, struct vmm_vcpu *vcpu)
{
	/* Don't need to emulate this instruction group */
	return VMM_EFAIL;
}

static int arm_instgrp_coproc(u32 inst, 
				arch_regs_t *regs, struct vmm_vcpu *vcpu)
{
	/* Don't need to emulate this instruction group */
	return VMM_EFAIL;
}

int cpu_vcpu_emulate_arm_inst(struct vmm_vcpu *vcpu, 
			      arch_regs_t *regs, bool is_hypercall)
{
	int rc;
	u32 inst, op1, op;
	physical_addr_t inst_pa;

	/* Determine instruction physical address */
	va2pa_ns_pr(regs->pc);
	inst_pa = read_par64();
	inst_pa &= PAR64_PA_MASK;
	inst_pa |= (regs->pc & 0x00000FFF);

	/* Read the faulting instruction */
	rc = vmm_host_physical_read(inst_pa, &inst, sizeof(inst));
	if (rc != sizeof(inst)) {
		return VMM_EFAIL;
	}

	/* If we know that we are in a hypercall 
	 * then skip unnecessary instruction decoding */
	if (is_hypercall) {
		return arm_instgrp_hypercall(inst, regs, vcpu);
	}

	op1 = ARM_INST_DECODE(inst,
			      ARM_INST_OP1_MASK, ARM_INST_OP1_SHIFT);
	switch (op1 & 0x6) {
	case 0x0:
		/* Data-processing and 
		 * miscellaneous instructions */
		return arm_instgrp_dataproc(inst, regs, vcpu);
		break;
	case 0x2:
		op = ARM_INST_DECODE(inst, ARM_INST_OP_MASK,
				     ARM_INST_OP_SHIFT);
		if (((op1 & 0x1) == 0x0) ||
		    (((op1 & 0x1) == 0x1) && op == 0x0)) {
			/* Load/store word and 
			 * unsigned byte instructions */
			return arm_instgrp_ldrstr(inst, regs, vcpu);
		} else {
			/* Media instructions */
			return arm_instgrp_media(inst, regs, vcpu);
		}
		break;
	case 0x4:
		/* Branch, branch with link, and 
		 * block data transfer instructions */
		return arm_instgrp_brblk(inst, regs, vcpu);
		break;
	case 0x6:
		/* Supervisor Call, and 
		 * coprocessor instructions */
		return arm_instgrp_coproc(inst, regs, vcpu);
		break;
	default:
		break;
	};

	arm_unpredictable(regs, vcpu, inst, __func__);

	return VMM_EFAIL;
}

