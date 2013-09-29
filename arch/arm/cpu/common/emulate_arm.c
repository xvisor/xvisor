/**
 * Copyright (c) 2011 Anup Patel.
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
 * @file emulate_arm.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief source code to emulate ARM instructions
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_vcpu_irq.h>
#include <arch_regs.h>
#include <cpu_defines.h>
#include <cpu_vcpu_helper.h>
#include <cpu_vcpu_coproc.h>
#include <cpu_vcpu_mem.h>
#include <emulate_arm.h>

void arm_unpredictable(arch_regs_t * regs, struct vmm_vcpu * vcpu, 
			u32 inst, const char *reason)
{
	vmm_printf("Unprecidable Instruction 0x%08x\n", inst);
	vmm_printf("Reason: %s\n", reason);
	cpu_vcpu_halt(vcpu, regs);
}

u32 arm_sign_extend(u32 imm, u32 len, u32 bits)
{
	if (imm & (1 << (len - 1))) {
		imm = imm | (~((1 << len) - 1));
	}
	return imm & ((1 << bits) - 1);
}

bool arm_condition_check(u32 cond, arch_regs_t * regs)
{
	bool ret = FALSE;
	if (cond == 0xE) {
		return TRUE;
	}
	switch (cond >> 1) {
	case 0:
		ret = (arm_cpsr(regs) & CPSR_ZERO_MASK) ? TRUE : FALSE;
		break;
	case 1:
		ret = (arm_cpsr(regs) & CPSR_CARRY_MASK) ? TRUE : FALSE;
		break;
	case 2:
		ret = (arm_cpsr(regs) & CPSR_NEGATIVE_MASK) ? TRUE : FALSE;
		break;
	case 3:
		ret = (arm_cpsr(regs) & CPSR_OVERFLOW_MASK) ? TRUE : FALSE;
		break;
	case 4:
		ret = (arm_cpsr(regs) & CPSR_CARRY_MASK) ? TRUE : FALSE;
		ret = (ret && !(arm_cpsr(regs) & CPSR_ZERO_MASK)) ? 
								TRUE : FALSE;
		break;
	case 5:
		if (arm_cpsr(regs) & CPSR_NEGATIVE_MASK) {
			ret = (arm_cpsr(regs) & CPSR_OVERFLOW_MASK) ? 
								TRUE : FALSE;
		} else {
			ret = (arm_cpsr(regs) & CPSR_OVERFLOW_MASK) ? 
								FALSE : TRUE;
		}
		break;
	case 6:
		if (arm_cpsr(regs) & CPSR_NEGATIVE_MASK) {
			ret = (arm_cpsr(regs) & CPSR_OVERFLOW_MASK) ? 
								TRUE : FALSE;
		} else {
			ret = (arm_cpsr(regs) & CPSR_OVERFLOW_MASK) ? 
								FALSE : TRUE;
		}
		ret = (ret && !(arm_cpsr(regs) & CPSR_ZERO_MASK)) ? 
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

u32 arm_decode_imm_shift(u32 type, u32 imm5, u32 *shift_t)
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

u32 arm_shift_c(u32 val, u32 shift_t, u32 shift_n, u32 cin, u32 *cout)
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

u32 arm_add_with_carry(u32 x, u32 y, u32 cin, u32 *cout, u32 *oout)
{
	u32 uresult = x + y + cin;
	if (cout) {
		if ((uresult < x) || (uresult < y)) {
			*cout = 1;
		} else {
			*cout = 0;
		}
	}
	if (oout) {
		s32 sresult = (s32)x + (s32)y + (s32)cin;
		if ((s32)uresult == sresult) {
			*oout = 0;
		} else {
			*oout = 1;
		}
	}
	return uresult;
}

/** Emulate 'ldrh (immediate)' instruction */
static int arm_inst_ldrh_i(u32 inst, 
				arch_regs_t * regs, struct vmm_vcpu * vcpu)
{
	int rc;
	u32 cond, P, U, W, Rn, Rt, imm4H, imm4L;
	u32 imm32, offset_addr, address; u16 data;
	bool index, add, wback;
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
	imm4H = ARM_INST_BITS(inst,
			      ARM_INST_LDRSTR_IMM4H_END,
			      ARM_INST_LDRSTR_IMM4H_START);
	imm4L = ARM_INST_BITS(inst,
			      ARM_INST_LDRSTR_IMM4L_END,
			      ARM_INST_LDRSTR_IMM4L_START);
	if ((P == 0) && (W == 1)) {
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	}
	imm32 = arm_zero_extend((imm4H << 4) | imm4L, 32);
	index = (P == 1) ? TRUE : FALSE;
	add = (U == 1) ? TRUE : FALSE;
	wback = ((P == 0) || (W == 1)) ? TRUE : FALSE;
	if ((Rt == 15) || (wback && (Rn == Rt))) {
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	}
	if (arm_condition_passed(cond, regs)) {
		address = cpu_vcpu_reg_read(vcpu, regs, Rn);
		offset_addr = (add) ? (address + imm32) : 
					(address - imm32);
		address = (index) ? offset_addr : address;
		data = 0x0;
		if ((rc = cpu_vcpu_mem_read(vcpu, regs, address, 
						 &data, 2, FALSE))) {
			return rc;
		}
		if (wback) {
			cpu_vcpu_reg_write(vcpu, regs, Rn, offset_addr);
		}
		cpu_vcpu_reg_write(vcpu, regs, Rt, arm_zero_extend(data, 32));
	}
	arm_pc(regs) += 4;
	return VMM_OK;
}

/** Emulate 'ldrh (literal)' instruction */
static int arm_inst_ldrh_l(u32 inst, 
				arch_regs_t * regs, struct vmm_vcpu * vcpu)
{
	int rc;
	u32 cond, U, Rt, imm4H, imm4L;
	u32 imm32, address; u16 data;
	bool add;
	cond = ARM_INST_DECODE(inst, ARM_INST_COND_MASK, ARM_INST_COND_SHIFT);
	U = ARM_INST_BITS(inst,
			  ARM_INST_LDRSTR_U_END,
			  ARM_INST_LDRSTR_U_START);
	Rt = ARM_INST_BITS(inst,
			   ARM_INST_LDRSTR_RT_END,
			   ARM_INST_LDRSTR_RT_START);
	imm4H = ARM_INST_BITS(inst,
			      ARM_INST_LDRSTR_IMM4H_END,
			      ARM_INST_LDRSTR_IMM4H_START);
	imm4L = ARM_INST_BITS(inst,
			      ARM_INST_LDRSTR_IMM4L_END,
			      ARM_INST_LDRSTR_IMM4L_START);
	imm32 = arm_zero_extend((imm4H << 4) | imm4L, 32);
	add = (U == 1) ? TRUE : FALSE;
	if (Rt == 15) {
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	}
	if (arm_condition_passed(cond, regs)) {
		address = arm_align(arm_pc(regs), 4);
		address = (add) ? (address + imm32) : (address - imm32);
		data = 0x0;
		if ((rc = cpu_vcpu_mem_read(vcpu, regs, address, 
						 &data, 2, FALSE))) {
			return rc;
		}
		cpu_vcpu_reg_write(vcpu, regs, Rt, arm_zero_extend(data, 32));
	}
	arm_pc(regs) += 4;
	return VMM_OK;
}

/** Emulate 'ldrh (register)' instruction */
static int arm_inst_ldrh_r(u32 inst, 
				arch_regs_t * regs, struct vmm_vcpu * vcpu)
{
	int rc;
	u32 cond, P, U, W, Rn, Rt, Rm;
	u32 shift_t, shift_n, offset, offset_addr, address; u16 data;
	bool index, add, wback;
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
	shift_t = arm_shift_lsl;
	shift_n = 0;
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
				  (arm_cpsr(regs) >> CPSR_CARRY_SHIFT) & 0x1);
		address = cpu_vcpu_reg_read(vcpu, regs, Rn);
		offset_addr = (add) ? (address + offset) : 
					(address - offset);
		address = (index) ? offset_addr : address;
		data = 0x0;
		if ((rc = cpu_vcpu_mem_read(vcpu, regs, address, 
						 &data, 2, FALSE))) {
			return rc;
		}
		if (wback) {
			cpu_vcpu_reg_write(vcpu, regs, Rn, offset_addr);
		}
		cpu_vcpu_reg_write(vcpu, regs, Rt, arm_zero_extend(data, 32));
	}
	arm_pc(regs) += 4;
	return VMM_OK;
}

/** Emulate 'ldrht' intruction */
static int arm_inst_ldrht(u32 inst, 
				arch_regs_t * regs, struct vmm_vcpu * vcpu)
{
	int rc;
	u32 cond, U, Rn, Rt, Rm, imm4H, imm4L;
	u32 imm32, offset, offset_addr, address; u16 data;
	bool regform, add;
	cond = ARM_INST_DECODE(inst, ARM_INST_COND_MASK, ARM_INST_COND_SHIFT);
	U = ARM_INST_BITS(inst,
			  ARM_INST_LDRSTR_U_END,
			  ARM_INST_LDRSTR_U_START);
	Rn = ARM_INST_BITS(inst,
			   ARM_INST_LDRSTR_RN_END,
			   ARM_INST_LDRSTR_RN_START);
	Rt = ARM_INST_BITS(inst,
			   ARM_INST_LDRSTR_RT_END,
			   ARM_INST_LDRSTR_RT_START);
	Rm = ARM_INST_BITS(inst,
			   ARM_INST_LDRSTR_RM_END,
			   ARM_INST_LDRSTR_RM_START);
	imm4H = ARM_INST_BITS(inst,
			      ARM_INST_LDRSTR_IMM4H_END,
			      ARM_INST_LDRSTR_IMM4H_START);
	imm4L = ARM_INST_BITS(inst,
			      ARM_INST_LDRSTR_IMM4L_END,
			      ARM_INST_LDRSTR_IMM4L_START);
	regform = ARM_INST_BITS(inst,
				ARM_INST_LDRSTR_REGFORM1_END,
				ARM_INST_LDRSTR_REGFORM1_START) ? FALSE : TRUE;
	if (regform) {
		if ((Rt == 15) || (Rn == 15) || (Rn == Rt) || (Rm == 15)) {
			arm_unpredictable(regs, vcpu, inst, __func__);
			return VMM_EFAIL;
		}
		imm32 = 0;
	} else {
		if ((Rt == 15) || (Rn == 15) || (Rn == Rt)) {
			arm_unpredictable(regs, vcpu, inst, __func__);
			return VMM_EFAIL;
		}
		imm32 = arm_zero_extend((imm4H << 4) | imm4L, 32);
	}
	add = (U == 1) ? TRUE : FALSE;
	if (arm_condition_passed(cond, regs)) {
		offset = (regform) ? cpu_vcpu_reg_read(vcpu, regs, Rm) : imm32;
		address = cpu_vcpu_reg_read(vcpu, regs, Rn);
		offset_addr = (add) ? (address + offset) : (address - offset);
		data = 0x0;
		if ((rc = cpu_vcpu_mem_read(vcpu, regs, address, 
						 &data, 2, TRUE))) {
			return rc;
		}
		cpu_vcpu_reg_write(vcpu, regs, Rn, offset_addr);
		cpu_vcpu_reg_write(vcpu, regs, Rt, arm_zero_extend(data, 32));
	}
	arm_pc(regs) += 4;
	return VMM_OK;
}

/** Emulate 'ldrex' instruction */
static int arm_inst_ldrex(u32 inst, 
		arch_regs_t * regs, struct vmm_vcpu * vcpu)
{
	int rc;
	u32 cond, Rn, Rt;
	u32 address, data; 
	cond = ARM_INST_DECODE(inst, ARM_INST_COND_MASK, ARM_INST_COND_SHIFT);
	Rn = ARM_INST_BITS(inst,
			ARM_INST_LDRSTR_RN_END,
			ARM_INST_LDRSTR_RN_START);
	Rt = ARM_INST_BITS(inst,
			ARM_INST_LDRSTR_RT_END,
			ARM_INST_LDRSTR_RT_START);
	if ((Rt == 15) || (Rn == 15)) {
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	}

	if (arm_condition_passed(cond, regs)) {
		address = cpu_vcpu_reg_read(vcpu, regs, Rn);

		if ((rc = cpu_vcpu_mem_readex(vcpu, regs, address, 
						&data, sizeof(data), FALSE))) {
			return rc;
		}

		cpu_vcpu_reg_write(vcpu, regs, Rt, data);
	}
	arm_pc(regs) += 4;
	return VMM_OK;
}

/** Emulate 'strex' instruction */
static int arm_inst_strex(u32 inst, 
				arch_regs_t * regs, struct vmm_vcpu * vcpu)
{
	int rc;
	u32 cond, Rn, Rt, Rd;
	u32 address, data;
	cond = ARM_INST_DECODE(inst, ARM_INST_COND_MASK, ARM_INST_COND_SHIFT);
	Rn = ARM_INST_BITS(inst,
			ARM_INST_LDRSTR_RN_END,
			ARM_INST_LDRSTR_RN_START);
	/* Rd field in strex encoding is in place of Rt */
	Rd = ARM_INST_BITS(inst,
			ARM_INST_LDRSTR_RT_END,
			ARM_INST_LDRSTR_RT_START);
	/* Rt field in strex encoding is in place of Rm */
	Rt = ARM_INST_BITS(inst,
			ARM_INST_LDRSTR_RM_END,
			ARM_INST_LDRSTR_RM_START);
	if ((Rd == 15) || (Rt == 15) || (Rn == 15)) {
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	}
	if ((Rd == Rn) || (Rd == Rt)) {
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	}
	if (arm_condition_passed(cond, regs)) {
		address = cpu_vcpu_reg_read(vcpu, regs, Rn);
		data = cpu_vcpu_reg_read(vcpu, regs, Rt);

		if ((rc = cpu_vcpu_mem_writeex(vcpu, regs, address, 
						&data, sizeof(data), FALSE))) {
			return rc;
		}

		cpu_vcpu_reg_write(vcpu, regs, Rd, 0);
	}
	arm_pc(regs) += 4;
	return VMM_OK;
}

/** Emulate 'strh (immediate)' instruction */
static int arm_inst_strh_i(u32 inst, 
		arch_regs_t * regs, struct vmm_vcpu * vcpu)
{
	int rc;
	u32 cond, P, U, W, Rn, Rt, imm4H, imm4L;
	u32 imm32, address, offset_addr; u16 data;
	bool index, add, wback;
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
	imm4H = ARM_INST_BITS(inst,
			      ARM_INST_LDRSTR_IMM4H_END,
			      ARM_INST_LDRSTR_IMM4H_START);
	imm4L = ARM_INST_BITS(inst,
			      ARM_INST_LDRSTR_IMM4L_END,
			      ARM_INST_LDRSTR_IMM4L_START);
	if ((P == 0) && (W == 1)) {
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	}
	imm32 = arm_zero_extend((imm4H << 4) | imm4L, 32);
	index = (P == 1) ? TRUE : FALSE;
	add = (U == 1) ? TRUE : FALSE;
	wback = ((P == 0) || (W == 1)) ? TRUE : FALSE;
	if ((Rt == 15) || (wback && ((Rn == 15) || (Rn == Rt)))) {
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	}
	if (arm_condition_passed(cond, regs)) {
		address = cpu_vcpu_reg_read(vcpu, regs, Rn);
		offset_addr = (add) ? (address + imm32) : (address - imm32);
		address = (index) ? offset_addr : address;
		data = cpu_vcpu_reg_read(vcpu, regs, Rt) & 0xFFFF;
		if ((rc = cpu_vcpu_mem_write(vcpu, regs, address, 
						  &data, 2, FALSE))) {
			return rc;
		}
		if (wback) {
			cpu_vcpu_reg_write(vcpu, regs, Rn, offset_addr);
		}
	}
	arm_pc(regs) += 4;
	return VMM_OK;
}

/** Emulate 'strh (register)' instruction */
static int arm_inst_strh_r(u32 inst, 
				arch_regs_t * regs, struct vmm_vcpu * vcpu)
{
	int rc;
	u32 cond, P, U, W, Rn, Rt, Rm;
	u32 shift_t, shift_n, offset, offset_addr, address; u16 data;
	bool index, add, wback;
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
	shift_t = arm_shift_lsl;
	shift_n = 0;
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
				  (arm_cpsr(regs) >> CPSR_CARRY_SHIFT) & 0x1);
		address = cpu_vcpu_reg_read(vcpu, regs, Rn);
		offset_addr = (add) ? (address + offset) : (address - offset);
		address = (index) ? offset_addr : address;
		data = cpu_vcpu_reg_read(vcpu, regs, Rt) & 0xFFFF;
		if ((rc = cpu_vcpu_mem_write(vcpu, regs, address, 
						  &data, 2, FALSE))) {
			return rc;
		}
		if (wback) {
			cpu_vcpu_reg_write(vcpu, regs, Rn, offset_addr);
		}
	}
	arm_pc(regs) += 4;
	return VMM_OK;
}

/** Emulate 'strht' intruction */
static int arm_inst_strht(u32 inst, 
				arch_regs_t * regs, struct vmm_vcpu * vcpu)
{
	int rc;
	u32 cond, U, Rn, Rt, Rm, imm4H, imm4L;
	u32 imm32, offset, offset_addr, address; u16 data;
	bool regform, add;
	cond = ARM_INST_DECODE(inst, ARM_INST_COND_MASK, ARM_INST_COND_SHIFT);
	U = ARM_INST_BITS(inst,
			  ARM_INST_LDRSTR_U_END,
			  ARM_INST_LDRSTR_U_START);
	Rn = ARM_INST_BITS(inst,
			   ARM_INST_LDRSTR_RN_END,
			   ARM_INST_LDRSTR_RN_START);
	Rt = ARM_INST_BITS(inst,
			   ARM_INST_LDRSTR_RT_END,
			   ARM_INST_LDRSTR_RT_START);
	Rm = ARM_INST_BITS(inst,
			   ARM_INST_LDRSTR_RM_END,
			   ARM_INST_LDRSTR_RM_START);
	imm4H = ARM_INST_BITS(inst,
			      ARM_INST_LDRSTR_IMM4H_END,
			      ARM_INST_LDRSTR_IMM4H_START);
	imm4L = ARM_INST_BITS(inst,
			      ARM_INST_LDRSTR_IMM4L_END,
			      ARM_INST_LDRSTR_IMM4L_START);
	regform = ARM_INST_BITS(inst,
				ARM_INST_LDRSTR_REGFORM1_END,
				ARM_INST_LDRSTR_REGFORM1_START) ? FALSE : TRUE;
	if (regform) {
		if ((Rt == 15) || (Rn == 15) || (Rn == Rt) || (Rm == 15)) {
			arm_unpredictable(regs, vcpu, inst, __func__);
			return VMM_EFAIL;
		}
		imm32 = 0;
	} else {
		if ((Rt == 15) || (Rn == 15) || (Rn == Rt)) {
			arm_unpredictable(regs, vcpu, inst, __func__);
			return VMM_EFAIL;
		}
		imm32 = arm_zero_extend((imm4H << 4) | imm4L, 32);
	}
	add = (U == 1) ? TRUE : FALSE;
	if (arm_condition_passed(cond, regs)) {
		offset = (regform) ? cpu_vcpu_reg_read(vcpu, regs, Rm) : imm32;
		address = cpu_vcpu_reg_read(vcpu, regs, Rn);
		offset_addr = (add) ? (address + offset) : (address - offset);
		data = cpu_vcpu_reg_read(vcpu, regs, Rt) & 0xFFFF;
		if ((rc = cpu_vcpu_mem_write(vcpu, regs, address, 
						  &data, 2, TRUE))) {
			return rc;
		}
		cpu_vcpu_reg_write(vcpu, regs, Rn, offset_addr);
	}
	arm_pc(regs) += 4;
	return VMM_OK;
}

/** Emulate 'ldrsh (immediate)' instruction */
static int arm_inst_ldrsh_i(u32 inst, 
				arch_regs_t * regs, struct vmm_vcpu * vcpu)
{
	int rc;
	u32 cond, P, U, W, Rn, Rt, imm4H, imm4L;
	u32 imm32, offset_addr, address; u16 data;
	bool index, add, wback;
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
	imm4H = ARM_INST_BITS(inst,
			      ARM_INST_LDRSTR_IMM4H_END,
			      ARM_INST_LDRSTR_IMM4H_START);
	imm4L = ARM_INST_BITS(inst,
			      ARM_INST_LDRSTR_IMM4L_END,
			      ARM_INST_LDRSTR_IMM4L_START);
	if ((P == 0) && (W == 1)) {
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	}
	imm32 = arm_zero_extend((imm4H << 4) | imm4L, 32);
	index = (P == 1) ? TRUE : FALSE;
	add = (U == 1) ? TRUE : FALSE;
	wback = ((P == 0) || (W == 1)) ? TRUE : FALSE;
	if ((Rt == 15) || (wback && (Rn == Rt))) {
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	}
	if (arm_condition_passed(cond, regs)) {
		address = cpu_vcpu_reg_read(vcpu, regs, Rn);
		offset_addr = (add) ? (address + imm32) : 
					(address - imm32);
		address = (index) ? offset_addr : address;
		data = 0x0;
		if ((rc = cpu_vcpu_mem_read(vcpu, regs, address, 
						 &data, 2, FALSE))) {
			return rc;
		}
		if (wback) {
			cpu_vcpu_reg_write(vcpu, regs, Rn, offset_addr);
		}
		cpu_vcpu_reg_write(vcpu, regs, Rt, 
					arm_sign_extend(data, 16, 32));
	}
	arm_pc(regs) += 4;
	return VMM_OK;
}

/** Emulate 'ldrsh (literal)' instruction */
static int arm_inst_ldrsh_l(u32 inst, 
				arch_regs_t * regs, struct vmm_vcpu * vcpu)
{
	int rc;
	u32 cond, U, Rt, imm4H, imm4L;
	u32 imm32, address; u16 data;
	bool add;
	cond = ARM_INST_DECODE(inst, ARM_INST_COND_MASK, ARM_INST_COND_SHIFT);
	U = ARM_INST_BITS(inst,
			  ARM_INST_LDRSTR_U_END,
			  ARM_INST_LDRSTR_U_START);
	Rt = ARM_INST_BITS(inst,
			   ARM_INST_LDRSTR_RT_END,
			   ARM_INST_LDRSTR_RT_START);
	imm4H = ARM_INST_BITS(inst,
			      ARM_INST_LDRSTR_IMM4H_END,
			      ARM_INST_LDRSTR_IMM4H_START);
	imm4L = ARM_INST_BITS(inst,
			      ARM_INST_LDRSTR_IMM4L_END,
			      ARM_INST_LDRSTR_IMM4L_START);
	imm32 = arm_zero_extend((imm4H << 4) | imm4L, 32);
	add = (U == 1) ? TRUE : FALSE;
	if (Rt == 15) {
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	}
	if (arm_condition_passed(cond, regs)) {
		address = arm_align(arm_pc(regs), 4);
		address = (add) ? (address + imm32) : (address - imm32);
		data = 0x0;
		if ((rc = cpu_vcpu_mem_read(vcpu, regs, address, 
						 &data, 2, FALSE))) {
			return rc;
		}
		cpu_vcpu_reg_write(vcpu, regs, Rt, 
					arm_sign_extend(data, 16, 32));
	}
	arm_pc(regs) += 4;
	return VMM_OK;
}

/** Emulate 'ldrsh (register)' instruction */
static int arm_inst_ldrsh_r(u32 inst, 
				arch_regs_t * regs, struct vmm_vcpu * vcpu)
{
	int rc;
	u32 cond, P, U, W, Rn, Rt, Rm;
	u32 shift_t, shift_n, offset, offset_addr, address; u16 data;
	bool index, add, wback;
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
	shift_t = arm_shift_lsl;
	shift_n = 0;
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
				  (arm_cpsr(regs) >> CPSR_CARRY_SHIFT) & 0x1);
		address = cpu_vcpu_reg_read(vcpu, regs, Rn);
		offset_addr = (add) ? (address + offset) : 
					(address - offset);
		address = (index) ? offset_addr : address;
		data = 0x0;
		if ((rc = cpu_vcpu_mem_read(vcpu, regs, address, 
						 &data, 2, FALSE))) {
			return rc;
		}
		if (wback) {
			cpu_vcpu_reg_write(vcpu, regs, Rn, offset_addr);
		}
		cpu_vcpu_reg_write(vcpu, regs, Rt, 
					arm_sign_extend(data, 16, 32));
	}
	arm_pc(regs) += 4;
	return VMM_OK;
}

/** Emulate 'ldrsht' intruction */
static int arm_inst_ldrsht(u32 inst, 
				arch_regs_t * regs, struct vmm_vcpu * vcpu)
{
	int rc;
	u32 cond, U, Rn, Rt, Rm, imm4H, imm4L;
	u32 imm32, offset, offset_addr, address; u16 data;
	bool regform, add;
	cond = ARM_INST_DECODE(inst, ARM_INST_COND_MASK, ARM_INST_COND_SHIFT);
	U = ARM_INST_BITS(inst,
			  ARM_INST_LDRSTR_U_END,
			  ARM_INST_LDRSTR_U_START);
	Rn = ARM_INST_BITS(inst,
			   ARM_INST_LDRSTR_RN_END,
			   ARM_INST_LDRSTR_RN_START);
	Rt = ARM_INST_BITS(inst,
			   ARM_INST_LDRSTR_RT_END,
			   ARM_INST_LDRSTR_RT_START);
	Rm = ARM_INST_BITS(inst,
			   ARM_INST_LDRSTR_RM_END,
			   ARM_INST_LDRSTR_RM_START);
	imm4H = ARM_INST_BITS(inst,
			      ARM_INST_LDRSTR_IMM4H_END,
			      ARM_INST_LDRSTR_IMM4H_START);
	imm4L = ARM_INST_BITS(inst,
			      ARM_INST_LDRSTR_IMM4L_END,
			      ARM_INST_LDRSTR_IMM4L_START);
	regform = ARM_INST_BITS(inst,
				ARM_INST_LDRSTR_REGFORM1_END,
				ARM_INST_LDRSTR_REGFORM1_START) ? FALSE : TRUE;
	if (regform) {
		if ((Rt == 15) || (Rn == 15) || (Rn == Rt) || (Rm == 15)) {
			arm_unpredictable(regs, vcpu, inst, __func__);
			return VMM_EFAIL;
		}
		imm32 = 0;
	} else {
		if ((Rt == 15) || (Rn == 15) || (Rn == Rt)) {
			arm_unpredictable(regs, vcpu, inst, __func__);
			return VMM_EFAIL;
		}
		imm32 = arm_zero_extend((imm4H << 4) | imm4L, 32);
	}
	add = (U == 1) ? TRUE : FALSE;
	if (arm_condition_passed(cond, regs)) {
		offset = (regform) ? cpu_vcpu_reg_read(vcpu, regs, Rm) : imm32;
		address = cpu_vcpu_reg_read(vcpu, regs, Rn);
		offset_addr = (add) ? (address + offset) : (address - offset);
		data = 0x0;
		if ((rc = cpu_vcpu_mem_read(vcpu, regs, address, 
						 &data, 2, TRUE))) {
			return rc;
		}
		cpu_vcpu_reg_write(vcpu, regs, Rn, offset_addr);
		cpu_vcpu_reg_write(vcpu, regs, Rt, 
					arm_sign_extend(data, 16, 32));
	}
	arm_pc(regs) += 4;
	return VMM_OK;
}

/** Emulate 'ldrsb (immediate)' instruction */
static int arm_inst_ldrsb_i(u32 inst, 
				arch_regs_t * regs, struct vmm_vcpu * vcpu)
{
	int rc;
	u32 cond, P, U, W, Rn, Rt, imm4H, imm4L;
	u32 imm32, offset_addr, address; u8 data;
	bool index, add, wback;
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
	imm4H = ARM_INST_BITS(inst,
			      ARM_INST_LDRSTR_IMM4H_END,
			      ARM_INST_LDRSTR_IMM4H_START);
	imm4L = ARM_INST_BITS(inst,
			      ARM_INST_LDRSTR_IMM4L_END,
			      ARM_INST_LDRSTR_IMM4L_START);
	if ((P == 0) && (W == 1)) {
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	}
	imm32 = arm_zero_extend((imm4H << 4) | imm4L, 32);
	index = (P == 1) ? TRUE : FALSE;
	add = (U == 1) ? TRUE : FALSE;
	wback = ((P == 0) || (W == 1)) ? TRUE : FALSE;
	if ((Rt == 15) || (wback && (Rn == Rt))) {
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	}
	if (arm_condition_passed(cond, regs)) {
		address = cpu_vcpu_reg_read(vcpu, regs, Rn);
		offset_addr = (add) ? (address + imm32) : 
					(address - imm32);
		address = (index) ? offset_addr : address;
		data = 0x0;
		if ((rc = cpu_vcpu_mem_read(vcpu, regs, address, 
						 &data, 1, FALSE))) {
			return rc;
		}
		if (wback) {
			cpu_vcpu_reg_write(vcpu, regs, Rn, offset_addr);
		}
		cpu_vcpu_reg_write(vcpu, regs, Rt, 
					arm_sign_extend(data, 8, 32));
	}
	arm_pc(regs) += 4;
	return VMM_OK;
}

/** Emulate 'ldrsb (literal)' instruction */
static int arm_inst_ldrsb_l(u32 inst, 
				arch_regs_t * regs, struct vmm_vcpu * vcpu)
{
	int rc;
	u32 cond, U, Rt, imm4H, imm4L;
	u32 imm32, address; u8 data;
	bool add;
	cond = ARM_INST_DECODE(inst, ARM_INST_COND_MASK, ARM_INST_COND_SHIFT);
	U = ARM_INST_BITS(inst,
			  ARM_INST_LDRSTR_U_END,
			  ARM_INST_LDRSTR_U_START);
	Rt = ARM_INST_BITS(inst,
			   ARM_INST_LDRSTR_RT_END,
			   ARM_INST_LDRSTR_RT_START);
	imm4H = ARM_INST_BITS(inst,
			      ARM_INST_LDRSTR_IMM4H_END,
			      ARM_INST_LDRSTR_IMM4H_START);
	imm4L = ARM_INST_BITS(inst,
			      ARM_INST_LDRSTR_IMM4L_END,
			      ARM_INST_LDRSTR_IMM4L_START);
	imm32 = arm_zero_extend((imm4H << 4) | imm4L, 32);
	add = (U == 1) ? TRUE : FALSE;
	if (Rt == 15) {
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	}
	if (arm_condition_passed(cond, regs)) {
		address = arm_align(arm_pc(regs), 4);
		address = (add) ? (address + imm32) : (address - imm32);
		data = 0x0;
		if ((rc = cpu_vcpu_mem_read(vcpu, regs, address, 
						 &data, 1, FALSE))) {
			return rc;
		}
		cpu_vcpu_reg_write(vcpu, regs, Rt, 
					arm_sign_extend(data, 8, 32));
	}
	arm_pc(regs) += 4;
	return VMM_OK;
}

/** Emulate 'ldrsb (register)' instruction */
static int arm_inst_ldrsb_r(u32 inst, 
				arch_regs_t * regs, struct vmm_vcpu * vcpu)
{
	int rc;
	u32 cond, P, U, W, Rn, Rt, Rm;
	u32 shift_t, shift_n, offset, offset_addr, address; u8 data;
	bool index, add, wback;
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
	shift_t = arm_shift_lsl;
	shift_n = 0;
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
				  (arm_cpsr(regs) >> CPSR_CARRY_SHIFT) & 0x1);
		address = cpu_vcpu_reg_read(vcpu, regs, Rn);
		offset_addr = (add) ? (address + offset) : 
					(address - offset);
		address = (index) ? offset_addr : address;
		data = 0x0;
		if ((rc = cpu_vcpu_mem_read(vcpu, regs, address, 
						 &data, 1, FALSE))) {
			return rc;
		}
		if (wback) {
			cpu_vcpu_reg_write(vcpu, regs, Rn, offset_addr);
		}
		cpu_vcpu_reg_write(vcpu, regs, Rt, 
					arm_sign_extend(data, 8, 32));
	}
	arm_pc(regs) += 4;
	return VMM_OK;
}

/** Emulate 'ldrsbt' intruction */
static int arm_inst_ldrsbt(u32 inst, 
				arch_regs_t * regs, struct vmm_vcpu * vcpu)
{
	int rc;
	u32 cond, U, Rn, Rt, Rm, imm4H, imm4L;
	u32 imm32, offset, offset_addr, address; u8 data;
	bool regform, add;
	cond = ARM_INST_DECODE(inst, ARM_INST_COND_MASK, ARM_INST_COND_SHIFT);
	U = ARM_INST_BITS(inst,
			  ARM_INST_LDRSTR_U_END,
			  ARM_INST_LDRSTR_U_START);
	Rn = ARM_INST_BITS(inst,
			   ARM_INST_LDRSTR_RN_END,
			   ARM_INST_LDRSTR_RN_START);
	Rt = ARM_INST_BITS(inst,
			   ARM_INST_LDRSTR_RT_END,
			   ARM_INST_LDRSTR_RT_START);
	Rm = ARM_INST_BITS(inst,
			   ARM_INST_LDRSTR_RM_END,
			   ARM_INST_LDRSTR_RM_START);
	imm4H = ARM_INST_BITS(inst,
			      ARM_INST_LDRSTR_IMM4H_END,
			      ARM_INST_LDRSTR_IMM4H_START);
	imm4L = ARM_INST_BITS(inst,
			      ARM_INST_LDRSTR_IMM4L_END,
			      ARM_INST_LDRSTR_IMM4L_START);
	regform = ARM_INST_BITS(inst,
				ARM_INST_LDRSTR_REGFORM1_END,
				ARM_INST_LDRSTR_REGFORM1_START) ? FALSE : TRUE;
	if (regform) {
		if ((Rt == 15) || (Rn == 15) || (Rn == Rt) || (Rm == 15)) {
			arm_unpredictable(regs, vcpu, inst, __func__);
			return VMM_EFAIL;
		}
		imm32 = 0;
	} else {
		if ((Rt == 15) || (Rn == 15) || (Rn == Rt)) {
			arm_unpredictable(regs, vcpu, inst, __func__);
			return VMM_EFAIL;
		}
		imm32 = arm_zero_extend((imm4H << 4) | imm4L, 32);
	}
	add = (U == 1) ? TRUE : FALSE;
	if (arm_condition_passed(cond, regs)) {
		offset = (regform) ? cpu_vcpu_reg_read(vcpu, regs, Rm) : imm32;
		address = cpu_vcpu_reg_read(vcpu, regs, Rn);
		offset_addr = (add) ? (address + offset) : (address - offset);
		data = 0x0;
		if ((rc = cpu_vcpu_mem_read(vcpu, regs, address, 
						 &data, 1, TRUE))) {
			return rc;
		}
		cpu_vcpu_reg_write(vcpu, regs, Rt, 
					arm_sign_extend(data, 8, 32));
		cpu_vcpu_reg_write(vcpu, regs, Rn, offset_addr);
	}
	arm_pc(regs) += 4;
	return VMM_OK;
}

/** Emulate 'ldrd (immediate)' instruction */
static int arm_inst_ldrd_i(u32 inst, 
				arch_regs_t * regs, struct vmm_vcpu * vcpu)
{
	int rc;
	u32 cond, P, U, W, Rn, Rt, imm4H, imm4L;
	u32 imm32, offset_addr, address, data;
	bool index, add, wback;
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
	imm4H = ARM_INST_BITS(inst,
			      ARM_INST_LDRSTR_IMM4H_END,
			      ARM_INST_LDRSTR_IMM4H_START);
	imm4L = ARM_INST_BITS(inst,
			      ARM_INST_LDRSTR_IMM4L_END,
			      ARM_INST_LDRSTR_IMM4L_START);
	if (Rt & 0x1) {
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	}
	if ((P == 0) && (W == 1)) {
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	}
	imm32 = arm_zero_extend((imm4H << 4) | imm4L, 32);
	index = (P == 1) ? TRUE : FALSE;
	add = (U == 1) ? TRUE : FALSE;
	wback = ((P == 0) || (W == 1)) ? TRUE : FALSE;
	if (wback && ((Rn == Rt) || (Rn == (Rt + 1)))) {
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	}
	if (Rt == 14) {
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	}
	if (arm_condition_passed(cond, regs)) {
		address = cpu_vcpu_reg_read(vcpu, regs, Rn);
		offset_addr = (add) ? (address + imm32) : 
					(address - imm32);
		address = (index) ? offset_addr : address;
		data = 0x0;
		if ((rc = cpu_vcpu_mem_read(vcpu, regs, address, 
						 &data, 4, FALSE))) {
			return rc;
		}
		cpu_vcpu_reg_write(vcpu, regs, Rt, data);
		data = 0x0;
		if ((rc = cpu_vcpu_mem_read(vcpu, regs, address + 4, 
						 &data, 4, FALSE))) {
			return rc;
		}
		cpu_vcpu_reg_write(vcpu, regs, Rt + 1, data);
		if (wback) {
			cpu_vcpu_reg_write(vcpu, regs, Rn, offset_addr);
		}
	}
	arm_pc(regs) += 4;
	return VMM_OK;
}

/** Emulate 'ldrd (literal)' instruction */
static int arm_inst_ldrd_l(u32 inst, 
				arch_regs_t * regs, struct vmm_vcpu * vcpu)
{
	int rc;
	u32 cond, U, Rt, imm4H, imm4L;
	u32 imm32, address, data;
	bool add;
	cond = ARM_INST_DECODE(inst, ARM_INST_COND_MASK, ARM_INST_COND_SHIFT);
	U = ARM_INST_BITS(inst,
			  ARM_INST_LDRSTR_U_END,
			  ARM_INST_LDRSTR_U_START);
	Rt = ARM_INST_BITS(inst,
			   ARM_INST_LDRSTR_RT_END,
			   ARM_INST_LDRSTR_RT_START);
	imm4H = ARM_INST_BITS(inst,
			      ARM_INST_LDRSTR_IMM4H_END,
			      ARM_INST_LDRSTR_IMM4H_START);
	imm4L = ARM_INST_BITS(inst,
			      ARM_INST_LDRSTR_IMM4L_END,
			      ARM_INST_LDRSTR_IMM4L_START);
	if (Rt & 0x1) {
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	}
	imm32 = arm_zero_extend((imm4H << 4) | imm4L, 32);
	add = (U == 1) ? TRUE : FALSE;
	if (Rt == 14) {
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	}
	if (arm_condition_passed(cond, regs)) {
		address = arm_align(arm_pc(regs), 4);
		address = (add) ? (address + imm32) : (address - imm32);
		data = 0x0;
		if ((rc = cpu_vcpu_mem_read(vcpu, regs, address, 
						 &data, 4, FALSE))) {
			return rc;
		}
		cpu_vcpu_reg_write(vcpu, regs, Rt, data);
		data = 0x0;
		if ((rc = cpu_vcpu_mem_read(vcpu, regs, address + 4, 
						 &data, 4, FALSE))) {
			return rc;
		}
		cpu_vcpu_reg_write(vcpu, regs, Rt + 1, data);
	}
	arm_pc(regs) += 4;
	return VMM_OK;
}

/** Emulate 'ldrd (register)' instruction */
static int arm_inst_ldrd_r(u32 inst, 
				arch_regs_t * regs, struct vmm_vcpu * vcpu)
{
	int rc;
	u32 cond, P, U, W, Rn, Rt, Rm;
	u32 offset, offset_addr, address, data;
	bool index, add, wback;
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
	Rm = ARM_INST_BITS(inst,
			   ARM_INST_LDRSTR_RM_END,
			   ARM_INST_LDRSTR_RM_START);
	if (Rt & 0x1) {
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	}
	if ((P == 0) && (W == 1)) {
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	}
	index = (P == 1) ? TRUE : FALSE;
	add = (U == 1) ? TRUE : FALSE;
	wback = ((P == 0) || (W == 1)) ? TRUE : FALSE;
	if ((Rt == 14) || (Rm == 15) || (Rm == Rt) || (Rm == (Rt + 1))) {
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	}
	if (wback && (Rn == 15 || (Rn == Rt) || (Rn == (Rt + 1)))) {
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	}
	if (arm_condition_passed(cond, regs)) {
		offset = cpu_vcpu_reg_read(vcpu, regs, Rm);
		address = cpu_vcpu_reg_read(vcpu, regs, Rn);
		offset_addr = (add) ? (address + offset) : (address - offset);
		address = (index) ? offset_addr : address;
		data = 0x0;
		if ((rc = cpu_vcpu_mem_read(vcpu, regs, address, 
						 &data, 4, FALSE))) {
			return rc;
		}
		cpu_vcpu_reg_write(vcpu, regs, Rt, data);
		data = 0x0;
		if ((rc = cpu_vcpu_mem_read(vcpu, regs, address + 4, 
						 &data, 4, FALSE))) {
			return rc;
		}
		cpu_vcpu_reg_write(vcpu, regs, Rt + 1, data);
		if (wback) {
			cpu_vcpu_reg_write(vcpu, regs, Rn, offset_addr);
		}
	}
	arm_pc(regs) += 4;
	return VMM_OK;
}

/** Emulate 'strd (immediate)' instruction */
static int arm_inst_strd_i(u32 inst, 
				arch_regs_t * regs, struct vmm_vcpu * vcpu)
{
	int rc;
	u32 cond, P, U, W, Rn, Rt, imm4H, imm4L;
	u32 imm32, offset_addr, address, data;
	bool index, add, wback;
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
	imm4H = ARM_INST_BITS(inst,
			      ARM_INST_LDRSTR_IMM4H_END,
			      ARM_INST_LDRSTR_IMM4H_START);
	imm4L = ARM_INST_BITS(inst,
			      ARM_INST_LDRSTR_IMM4L_END,
			      ARM_INST_LDRSTR_IMM4L_START);
	if (Rt & 0x1) {
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	}
	if ((P == 0) && (W == 1)) {
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	}
	imm32 = arm_zero_extend((imm4H << 4) | imm4L, 32);
	index = (P == 1) ? TRUE : FALSE;
	add = (U == 1) ? TRUE : FALSE;
	wback = ((P == 0) || (W == 1)) ? TRUE : FALSE;
	if (wback && ((Rn == 15) || (Rn == Rt) || (Rn == (Rt + 1)))) {
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	}
	if (Rt == 14) {
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	}
	if (arm_condition_passed(cond, regs)) {
		address = cpu_vcpu_reg_read(vcpu, regs, Rn);
		offset_addr = (add) ? (address + imm32) : 
					(address - imm32);
		address = (index) ? offset_addr : address;
		data = cpu_vcpu_reg_read(vcpu, regs, Rt);
		if ((rc = cpu_vcpu_mem_write(vcpu, regs, address, 
						  &data, 4, FALSE))) {
			return rc;
		}
		data = cpu_vcpu_reg_read(vcpu, regs, Rt + 1);
		if ((rc = cpu_vcpu_mem_write(vcpu, regs, address + 4, 
						  &data, 4, FALSE))) {
			return rc;
		}
		if (wback) {
			cpu_vcpu_reg_write(vcpu, regs, Rn, offset_addr);
		}
	}
	arm_pc(regs) += 4;
	return VMM_OK;
}

/** Emulate 'strd (register)' instruction */
static int arm_inst_strd_r(u32 inst, 
				arch_regs_t * regs, struct vmm_vcpu * vcpu)
{
	int rc;
	u32 cond, P, U, W, Rn, Rt, Rm;
	u32 offset, offset_addr, address, data;
	bool index, add, wback;
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
	Rm = ARM_INST_BITS(inst,
			   ARM_INST_LDRSTR_RM_END,
			   ARM_INST_LDRSTR_RM_START);
	if (Rt & 0x1) {
		vmm_vcpu_irq_assert(vcpu, CPU_UNDEF_INST_IRQ, 0x0);
		return VMM_OK;
	}
	if ((P == 0) && (W == 1)) {
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	}
	index = (P == 1) ? TRUE : FALSE;
	add = (U == 1) ? TRUE : FALSE;
	wback = ((P == 0) || (W == 1)) ? TRUE : FALSE;
	if ((Rt == 14) || (Rm == 15)) {
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	}
	if (wback && (Rn == 15 || (Rn == Rt) || (Rn == (Rt + 1)))) {
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	}
	if (arm_condition_passed(cond, regs)) {
		offset = cpu_vcpu_reg_read(vcpu, regs, Rm);
		address = cpu_vcpu_reg_read(vcpu, regs, Rn);
		offset_addr = (add) ? (address + offset) : 
					(address - offset);
		address = (index) ? offset_addr : address;
		data = cpu_vcpu_reg_read(vcpu, regs, Rt);
		if ((rc = cpu_vcpu_mem_write(vcpu, regs, address, 
						  &data, 4, FALSE))) {
			return rc;
		}
		data = cpu_vcpu_reg_read(vcpu, regs, Rt + 1);
		if ((rc = cpu_vcpu_mem_write(vcpu, regs, address + 4, 
						  &data, 4, FALSE))) {
			return rc;
		}
		if (wback) {
			cpu_vcpu_reg_write(vcpu, regs, Rn, offset_addr);
		}
	}
	arm_pc(regs) += 4;
	return VMM_OK;
}

/** Emulate 'movw (immediate)' instruction */
static int arm_inst_movw_i(u32 inst, 
				arch_regs_t * regs, struct vmm_vcpu * vcpu)
{
	u32 cond, Rd, imm4, imm12;
	u32 result;
	cond = ARM_INST_DECODE(inst, ARM_INST_COND_MASK, ARM_INST_COND_SHIFT);
	Rd = ARM_INST_BITS(inst,
			  ARM_INST_MOVW_I_RD_END,
			  ARM_INST_MOVW_I_RD_START);
	imm4 = ARM_INST_BITS(inst,
			  ARM_INST_MOVW_I_IMM4_END,
			  ARM_INST_MOVW_I_IMM4_START);
	imm12 = ARM_INST_BITS(inst,
			  ARM_INST_MOVW_I_IMM12_END,
			  ARM_INST_MOVW_I_IMM12_START);
	if (Rd == 15) {
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	}
	result = (imm4 << 12) | imm12;
	if (arm_condition_passed(cond, regs)) {
		if (Rd == 15) {
			arm_pc(regs) = result;
		} else {
			cpu_vcpu_reg_write(vcpu, regs, Rd, result);
		}
	}
	arm_pc(regs) += 4;
	return VMM_OK;
}

/** Emulate data processing instructions */
static int arm_instgrp_dataproc(u32 inst, 
				arch_regs_t * regs, struct vmm_vcpu * vcpu)
{
	u32 op, op1, Rn, op2;

	op = ARM_INST_DECODE(inst,
			     ARM_INST_DATAPROC_OP_MASK,
			     ARM_INST_DATAPROC_OP_SHIFT);
	op1 = ARM_INST_DECODE(inst,
			      ARM_INST_DATAPROC_OP1_MASK,
			      ARM_INST_DATAPROC_OP1_SHIFT);

	if (op) {
		switch (op1) {
		case 0B10000: /* 10000 */
			/* MOVW (immediate) */
			return arm_inst_movw_i(inst, regs, vcpu);
		case 0B10100: /* 10100 */
			/* FIXME: 
			 * High halfword 16-bit immediate load, MOVT
			 */
			break; 
		case 0B10010: /* 10x10 */
		case 0B10110:
			/* FIXME:
			 * MSR (immediate), and hints
			 */
			break;
		default: /* not 10xx0 */
			/* FIXME:
			 * Data-processing (immediate)
			 */
			break;
		};
	} else {
		op2 = ARM_INST_DECODE(inst,
			      ARM_INST_DATAPROC_OP2_MASK,
			      ARM_INST_DATAPROC_OP2_SHIFT);
		if (!(op1 & 0B10000) && (op1 & 0B00010)) {
			/* Extra load/store instructions (unpriviledged) */
			switch (op2) {
			case 0B1011:
				if (op1 & 0B00001) {
					/* LDRHT */
					return arm_inst_ldrht(inst, 
								regs, vcpu);
				} else {
					/* STRHT */
					return arm_inst_strht(inst, 
								regs, vcpu);
				}
				break;
			case 0B1101:
				if (op1 & 0B00001) {
					/* LDRSBT */
					return arm_inst_ldrsbt(inst, 
								regs, vcpu);
				}
				break;
			case 0B1111:
				if (op1 & 0B00001) {
					/* LDRSHT */
					return arm_inst_ldrsht(inst, 
								regs, vcpu);
				}
				break;
			default:
				break;
			};
		} else {
			/* Extra load/store instructions */
			switch (op2) {
			case 0B1011:
				switch (op1) {
				case 0B00000: /* xx0x0 */
				case 0B00010:
				case 0B01000:
				case 0B01010:
				case 0B10000:
				case 0B10010:
				case 0B11000:
				case 0B11010:
					/* STRH (register) */
					return arm_inst_strh_r(inst, 
								regs, vcpu);
				case 0B00001: /* xx0x1 */
				case 0B00011:
				case 0B01001:
				case 0B01011:
				case 0B10001:
				case 0B10011:
				case 0B11001:
				case 0B11011:
					/* LDRH (register) */
					return arm_inst_ldrh_r(inst, 
								regs, vcpu);
				case 0B00100: /* xx1x0 */
				case 0B00110:
				case 0B01100:
				case 0B01110:
				case 0B10100:
				case 0B10110:
				case 0B11100:
				case 0B11110:
					/* STRH (immediate, ARM) */
					return arm_inst_strh_i(inst, 
								regs, vcpu);
				case 0B00101: /* xx1x1 */
				case 0B00111:
				case 0B01101:
				case 0B01111:
				case 0B10101:
				case 0B10111:
				case 0B11101:
				case 0B11111:
					Rn = ARM_INST_DECODE(inst,
					     ARM_INST_DATAPROC_RN_MASK,
					     ARM_INST_DATAPROC_RN_SHIFT);
					if (Rn == 0xF) {
						/* LDRH (literal) */
						return arm_inst_ldrh_l(inst, 
								regs, vcpu);
					} else {
						/* LDRH (immediate, ARM) */
						return arm_inst_ldrh_i(inst, 
								regs, vcpu);
					}
					break;
				default:
					break;
				};
				break;
			case 0B1101:
				switch (op1) {
				case 0B00000: /* xx0x0 */
				case 0B00010:
				case 0B01000:
				case 0B01010:
				case 0B10000:
				case 0B10010:
				case 0B11000:
				case 0B11010:
					/* LDRD (register) */
					return arm_inst_ldrd_r(inst, 
								regs, vcpu);
				case 0B00001: /* xx0x1 */
				case 0B00011:
				case 0B01001:
				case 0B01011:
				case 0B10001:
				case 0B10011:
				case 0B11001:
				case 0B11011:
					/* LDRSB (register) */
					return arm_inst_ldrsb_r(inst, 
								regs, vcpu);
				case 0B00100: /* xx1x0 */
				case 0B00110:
				case 0B01100:
				case 0B01110:
				case 0B10100:
				case 0B10110:
				case 0B11100:
				case 0B11110:
					Rn = ARM_INST_DECODE(inst,
					     ARM_INST_DATAPROC_RN_MASK,
					     ARM_INST_DATAPROC_RN_SHIFT);
					if (Rn == 0xF) {
						/* LDRD (literal) */
						return arm_inst_ldrd_l(inst, 
								regs, vcpu);
					} else {
						/* LDRD (immediate) */
						return arm_inst_ldrd_i(inst, 
								regs, vcpu);
					}
					break;
				case 0B00101: /* xx1x1 */
				case 0B00111:
				case 0B01101:
				case 0B01111:
				case 0B10101:
				case 0B10111:
				case 0B11101:
				case 0B11111:
					Rn = ARM_INST_DECODE(inst,
					     ARM_INST_DATAPROC_RN_MASK,
					     ARM_INST_DATAPROC_RN_SHIFT);
					if (Rn == 0xF) {
						/* LDRSB (literal) */
						return arm_inst_ldrsb_l(inst, 
								regs, vcpu);
					} else {
						/* LDRSB (immediate) */
						return arm_inst_ldrsb_i(inst, 
								regs, vcpu);
					}
					break;
				default:
					break;
				};
				break;
			case 0B1111:
				switch (op1) {
				case 0B00000: /* xx0x0 */
				case 0B00010:
				case 0B01000:
				case 0B01010:
				case 0B10000:
				case 0B10010:
				case 0B11000:
				case 0B11010:
					/* STRD (register) */
					return arm_inst_strd_r(inst, 
								regs, vcpu);
				case 0B00001: /* xx0x1 */
				case 0B00011:
				case 0B01001:
				case 0B01011:
				case 0B10001:
				case 0B10011:
				case 0B11001:
				case 0B11011:
					/* LDRSH (register) */
					return arm_inst_ldrsh_r(inst, 
								regs, vcpu);
				case 0B00100: /* xx1x0 */
				case 0B00110:
				case 0B01100:
				case 0B01110:
				case 0B10100:
				case 0B10110:
				case 0B11100:
				case 0B11110:
					/* STRD (immediate) */
					return arm_inst_strd_i(inst, 
								regs, vcpu);
				case 0B00101: /* xx1x1 */
				case 0B00111:
				case 0B01101:
				case 0B01111:
				case 0B10101:
				case 0B10111:
				case 0B11101:
				case 0B11111:
					Rn = ARM_INST_DECODE(inst,
					     ARM_INST_DATAPROC_RN_MASK,
					     ARM_INST_DATAPROC_RN_SHIFT);
					if (Rn == 0xF) {
						/* LDRSH (literal) */
						return arm_inst_ldrsh_l(inst, 
								regs, vcpu);
					} else {
						/* LDRSH (immediate) */
						return arm_inst_ldrsh_i(inst, 
								regs, vcpu);
					}
					break;
				default:
					break;
				};
				break;
			default:
				break;
			};
		}
		if ((op1 & 0B10000) && (op2==0B1001)) {
			/* Synchronization primitives */
			switch (op1) {
			case 0B11000:
				/* STREX */
				return arm_inst_strex(inst, regs, vcpu);
			case 0B11001:
				/* LDREX */
				return arm_inst_ldrex(inst, regs, vcpu);
			default:
				break;
			};
		}
	}

	arm_unpredictable(regs, vcpu, inst, __func__);
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
		if ((rc = cpu_vcpu_mem_write(vcpu, regs, address, 
						  &data, 4, FALSE))) {
			return rc;
		}
		if ((P == 0) || (W == 1)) {
			cpu_vcpu_reg_write(vcpu, regs, Rn, offset_addr);
		}
	}
	arm_pc(regs) += 4;
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
				  (arm_cpsr(regs) >> CPSR_CARRY_SHIFT) & 0x1);
		address = cpu_vcpu_reg_read(vcpu, regs, Rn);
		offset_addr = (add) ? (address + offset) : (address - offset);
		address = (index) ? offset_addr : address;
		data = cpu_vcpu_reg_read(vcpu, regs, Rt);
		if ((rc = cpu_vcpu_mem_write(vcpu, regs, address, 
						  &data, 4, FALSE))) {
			return rc;
		}
		if (wback) {
			cpu_vcpu_reg_write(vcpu, regs, Rn, offset_addr);
		}
	}
	arm_pc(regs) += 4;
	return VMM_OK;
}

/** Emulate 'strt' intruction */
static int arm_inst_strt(u32 inst, 
				arch_regs_t * regs, struct vmm_vcpu * vcpu)
{
	int rc;
	u32 cond, U, Rn, Rt, imm12, imm32, imm5, type, Rm;
	u32 shift_t, shift_n, offset, offset_addr, address; u32 data;
	bool regform, add;
	cond = ARM_INST_DECODE(inst, ARM_INST_COND_MASK, ARM_INST_COND_SHIFT);
	U = ARM_INST_BITS(inst,
			  ARM_INST_LDRSTR_U_END,
			  ARM_INST_LDRSTR_U_START);
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
	imm12 = ARM_INST_BITS(inst,
			      ARM_INST_LDRSTR_IMM12_END,
			      ARM_INST_LDRSTR_IMM12_START);
	regform = ARM_INST_BITS(inst,
				ARM_INST_LDRSTR_REGFORM2_END,
				ARM_INST_LDRSTR_REGFORM2_START) ? TRUE : FALSE;
	if (regform) {
		if ((Rm == 15) || (Rn == 15) || (Rn == Rt)) {
			arm_unpredictable(regs, vcpu, inst, __func__);
			return VMM_EFAIL;
		}
		imm32 = 0;
		shift_n = arm_decode_imm_shift(type, imm5, &shift_t);
	} else {
		if ((Rn == 15) || (Rn == Rt)) {
			arm_unpredictable(regs, vcpu, inst, __func__);
			return VMM_EFAIL;
		}
		imm32 = arm_zero_extend(imm12, 32);
		shift_t = 0;
		shift_n = 0;
	}
	add = (U == 1) ? TRUE : FALSE;
	if (arm_condition_passed(cond, regs)) {
		offset = (regform) ? 
			 arm_shift(cpu_vcpu_reg_read(vcpu, regs, Rm), 
				  shift_t, 
				  shift_n, 
				  (arm_cpsr(regs) >> CPSR_CARRY_SHIFT) & 0x1)
				  : imm32;
		address = cpu_vcpu_reg_read(vcpu, regs, Rn);
		offset_addr = (add) ? (address + offset) : 
					(address - offset);
		data = cpu_vcpu_reg_read(vcpu, regs, Rt);
		if ((rc = cpu_vcpu_mem_write(vcpu, regs, address, 
						  &data, 4, TRUE))) {
			return rc;
		}
		cpu_vcpu_reg_write(vcpu, regs, Rn, offset_addr);
	}
	arm_pc(regs) += 4;
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
		if ((rc = cpu_vcpu_mem_write(vcpu, regs, address, 
						  &data, 1, FALSE))) {
			return rc;
		}
		if (wback) {
			cpu_vcpu_reg_write(vcpu, regs, Rn, offset_addr);
		}
	}
	arm_pc(regs) += 4;
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
				  (arm_cpsr(regs) & CPSR_CARRY_SHIFT) & 0x1);
		address = cpu_vcpu_reg_read(vcpu, regs, Rn);
		offset_addr = (add) ? (address + offset) : 
					(address - offset);
		address = (index) ? offset_addr : address;
		data = cpu_vcpu_reg_read(vcpu, regs, Rt) & 0xFF;
		if ((rc = cpu_vcpu_mem_write(vcpu, regs, address, 
						  &data, 1, FALSE))) {
			return rc;
		}
		if (wback) {
			cpu_vcpu_reg_write(vcpu, regs, Rn, offset_addr);
		}
	}
	arm_pc(regs) += 4;
	return VMM_OK;
}

/** Emulate 'strbt' intruction */
static int arm_inst_strbt(u32 inst, 
				arch_regs_t * regs, struct vmm_vcpu * vcpu)
{
	int rc;
	u32 cond, U, Rn, Rt, imm12, imm32, imm5, type, Rm;
	u32 shift_t, shift_n, offset, offset_addr, address; u8 data;
	bool regform, add;
	cond = ARM_INST_DECODE(inst, ARM_INST_COND_MASK, ARM_INST_COND_SHIFT);
	U = ARM_INST_BITS(inst,
			  ARM_INST_LDRSTR_U_END,
			  ARM_INST_LDRSTR_U_START);
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
	imm12 = ARM_INST_BITS(inst,
			      ARM_INST_LDRSTR_IMM12_END,
			      ARM_INST_LDRSTR_IMM12_START);
	regform = ARM_INST_BITS(inst,
				ARM_INST_LDRSTR_REGFORM2_END,
				ARM_INST_LDRSTR_REGFORM2_START) ? TRUE : FALSE;
	if (regform) {
		if ((Rt == 15) || (Rm == 15) || (Rn == 15) || (Rn == Rt)) {
			arm_unpredictable(regs, vcpu, inst, __func__);
			return VMM_EFAIL;
		}
		imm32 = 0;
		shift_n = arm_decode_imm_shift(type, imm5, &shift_t);
	} else {
		if ((Rt == 15) || (Rn == 15) || (Rn == Rt)) {
			arm_unpredictable(regs, vcpu, inst, __func__);
			return VMM_EFAIL;
		}
		imm32 = arm_zero_extend(imm12, 32);
		shift_t = 0;
		shift_n = 0;
	}
	add = (U == 1) ? TRUE : FALSE;
	if (arm_condition_passed(cond, regs)) {
		offset = (regform) ? 
			 arm_shift(cpu_vcpu_reg_read(vcpu, regs, Rm), 
				  shift_t, 
				  shift_n, 
				  (arm_cpsr(regs) >> CPSR_CARRY_SHIFT) & 0x1)
				  : imm32;
		address = cpu_vcpu_reg_read(vcpu, regs, Rn);
		offset_addr = (add) ? (address + offset) : 
					(address - offset);
		data = cpu_vcpu_reg_read(vcpu, regs, Rt) & 0xFF;
		if ((rc = cpu_vcpu_mem_write(vcpu, regs, address, 
						  &data, 1, TRUE))) {
			return rc;
		}
		cpu_vcpu_reg_write(vcpu, regs, Rn, offset_addr);
	}
	arm_pc(regs) += 4;
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
		if ((rc = cpu_vcpu_mem_read(vcpu, regs, address, 
						 &data, 4, FALSE))) {
			return rc;
		}
		cpu_vcpu_reg_write(vcpu, regs, Rt, data);
		if ((P == 0) || (W == 1)) {
			cpu_vcpu_reg_write(vcpu, regs, Rn, offset_addr);
		}
	}
	arm_pc(regs) += 4;
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
		address = arm_align(arm_pc(regs), 4);
		address = (add) ? (address + imm32) : (address - imm32);
		data = 0x0;
		if ((rc = cpu_vcpu_mem_read(vcpu, regs, address, 
						 &data, 4, FALSE))) {
			return rc;
		}
		cpu_vcpu_reg_write(vcpu, regs, Rt, data);
	}
	arm_pc(regs) += 4;
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
				  (arm_cpsr(regs) >> CPSR_CARRY_SHIFT) & 0x1);
		address = cpu_vcpu_reg_read(vcpu, regs, Rn);
		offset_addr = (add) ? (address + offset) : 
					(address - offset);
		address = (index) ? offset_addr : address;
		data = 0x0;
		if ((rc = cpu_vcpu_mem_read(vcpu, regs, address, 
						 &data, 4, FALSE))) {
			return rc;
		}
		cpu_vcpu_reg_write(vcpu, regs, Rt, data);
		if (wback) {
			cpu_vcpu_reg_write(vcpu, regs, Rn, offset_addr);
		}
	}
	arm_pc(regs) += 4;
	return VMM_OK;
}

/** Emulate 'ldrt' intruction */
static int arm_inst_ldrt(u32 inst, 
				arch_regs_t * regs, struct vmm_vcpu * vcpu)
{
	int rc;
	u32 cond, U, Rn, Rt, imm12, imm32, imm5, type, Rm;
	u32 shift_t, shift_n, offset, offset_addr, address; u32 data;
	bool regform, add;
	cond = ARM_INST_DECODE(inst, ARM_INST_COND_MASK, ARM_INST_COND_SHIFT);
	U = ARM_INST_BITS(inst,
			  ARM_INST_LDRSTR_U_END,
			  ARM_INST_LDRSTR_U_START);
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
	imm12 = ARM_INST_BITS(inst,
			      ARM_INST_LDRSTR_IMM12_END,
			      ARM_INST_LDRSTR_IMM12_START);
	regform = ARM_INST_BITS(inst,
				ARM_INST_LDRSTR_REGFORM2_END,
				ARM_INST_LDRSTR_REGFORM2_START) ? TRUE : FALSE;
	if (regform) {
		if ((Rt == 15) || (Rm == 15) || (Rn == 15) || (Rn == Rt)) {
			arm_unpredictable(regs, vcpu, inst, __func__);
			return VMM_EFAIL;
		}
		imm32 = 0;
		shift_n = arm_decode_imm_shift(type, imm5, &shift_t);
	} else {
		if ((Rt == 15) || (Rn == 15) || (Rn == Rt)) {
			arm_unpredictable(regs, vcpu, inst, __func__);
			return VMM_EFAIL;
		}
		imm32 = arm_zero_extend(imm12, 32);
		shift_t = 0;
		shift_n = 0;
	}
	add = (U == 1) ? TRUE : FALSE;
	if (arm_condition_passed(cond, regs)) {
		offset = (regform) ? 
			 arm_shift(cpu_vcpu_reg_read(vcpu, regs, Rm), 
				  shift_t, 
				  shift_n, 
				  (arm_cpsr(regs) >> CPSR_CARRY_SHIFT) & 0x1)
				  : imm32;
		address = cpu_vcpu_reg_read(vcpu, regs, Rn);
		offset_addr = (add) ? (address + offset) : 
					(address - offset);
		cpu_vcpu_reg_write(vcpu, regs, Rn, offset_addr);
		data = 0x0;
		if ((rc = cpu_vcpu_mem_read(vcpu, regs, address, 
						 &data, 4, TRUE))) {
			return rc;
		}
		cpu_vcpu_reg_write(vcpu, regs, Rt, arm_zero_extend(data, 32));
	}
	arm_pc(regs) += 4;
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
		if ((rc = cpu_vcpu_mem_read(vcpu, regs, address, 
						 &data, 1, FALSE))) {
			return rc;
		}
		cpu_vcpu_reg_write(vcpu, regs, Rt, arm_zero_extend(data, 32));
		if (wback) {
			cpu_vcpu_reg_write(vcpu, regs, Rn, offset_addr);
		}
	}
	arm_pc(regs) += 4;
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
		address = arm_align(arm_pc(regs), 4);
		address = (add) ? (address + imm32) : (address - imm32);
		data = 0x0;
		if ((rc = cpu_vcpu_mem_read(vcpu, regs, address, 
						 &data, 1, FALSE))) {
			return rc;
		}
		cpu_vcpu_reg_write(vcpu, regs, Rt, arm_zero_extend(data, 32));
	}
	arm_pc(regs) += 4;
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
				  (arm_cpsr(regs) >> CPSR_CARRY_SHIFT) & 0x1);
		address = cpu_vcpu_reg_read(vcpu, regs, Rn);
		offset_addr = (add) ? (address + offset) : 
					(address - offset);
		address = (index) ? offset_addr : address;
		data = 0x0;
		if ((rc = cpu_vcpu_mem_read(vcpu, regs, address, 
						 &data, 1, FALSE))) {
			return rc;
		}
		cpu_vcpu_reg_write(vcpu, regs, Rt, arm_zero_extend(data, 32));
		if (wback) {
			cpu_vcpu_reg_write(vcpu, regs, Rn, offset_addr);
		}
	}
	arm_pc(regs) += 4;
	return VMM_OK;
}

/** Emulate 'ldrbt' intruction */
static int arm_inst_ldrbt(u32 inst, 
				arch_regs_t * regs, struct vmm_vcpu * vcpu)
{
	int rc;
	u32 cond, U, Rn, Rt, imm12, imm32, imm5, type, Rm;
	u32 shift_t, shift_n, offset, offset_addr, address; u8 data;
	bool regform, add;
	cond = ARM_INST_DECODE(inst, ARM_INST_COND_MASK, ARM_INST_COND_SHIFT);
	U = ARM_INST_BITS(inst,
			  ARM_INST_LDRSTR_U_END,
			  ARM_INST_LDRSTR_U_START);
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
	imm12 = ARM_INST_BITS(inst,
			      ARM_INST_LDRSTR_IMM12_END,
			      ARM_INST_LDRSTR_IMM12_START);
	regform = ARM_INST_BITS(inst,
				ARM_INST_LDRSTR_REGFORM2_END,
				ARM_INST_LDRSTR_REGFORM2_START) ? TRUE : FALSE;
	if (regform) {
		if ((Rt == 15) || (Rm == 15) || (Rn == 15) || (Rn == Rt)) {
			arm_unpredictable(regs, vcpu, inst, __func__);
			return VMM_EFAIL;
		}
		imm32 = 0;
		shift_n = arm_decode_imm_shift(type, imm5, &shift_t);
	} else {
		if ((Rt == 15) || (Rn == 15) || (Rn == Rt)) {
			arm_unpredictable(regs, vcpu, inst, __func__);
			return VMM_EFAIL;
		}
		imm32 = arm_zero_extend(imm12, 32);
		shift_t = 0;
		shift_n = 0;
	}
	add = (U == 1) ? TRUE : FALSE;
	if (arm_condition_passed(cond, regs)) {
		offset = (regform) ? 
			 arm_shift(cpu_vcpu_reg_read(vcpu, regs, Rm), 
				  shift_t, 
				  shift_n, 
				  (arm_cpsr(regs) >> CPSR_CARRY_SHIFT) & 0x1)
				  : imm32;
		address = cpu_vcpu_reg_read(vcpu, regs, Rn);
		offset_addr = (add) ? (address + offset) : 
					(address - offset);
		data = 0x0;
		if ((rc = cpu_vcpu_mem_read(vcpu, regs, address, 
						 &data, 1, TRUE))) {
			return rc;
		}
		cpu_vcpu_reg_write(vcpu, regs, Rt, arm_zero_extend(data, 32));
		cpu_vcpu_reg_write(vcpu, regs, Rn, offset_addr);
	}
	arm_pc(regs) += 4;
	return VMM_OK;
}

/** Emulate load/store instructions */
static int arm_instgrp_ldrstr(u32 inst, 
				arch_regs_t * regs, struct vmm_vcpu * vcpu)
{
	u32 op1, rn;

	op1 = ARM_INST_DECODE(inst,
			      ARM_INST_LDRSTR_OP1_MASK,
			      ARM_INST_LDRSTR_OP1_SHIFT);

	if (!(inst & ARM_INST_LDRSTR_A_MASK)) {
		switch (op1) {
		case 0B00000: /* xx0x0 not 0x010 */
		case 0B01000:
		case 0B10000:
		case 0B10010:
		case 0B11000:
		case 0B11010:
			/* STR (immediate) */
			return arm_inst_str_i(inst, regs, vcpu);
		case 0B00010: /* 0x010 */
		case 0B01010:
			/* STRT */
			return arm_inst_strt(inst, regs, vcpu);
		case 0B00001: /* xx0x1 not 0x011 */
		case 0B01001:
		case 0B10001:
		case 0B10011:
		case 0B11001:
		case 0B11011:
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
			break;
		case 0B00011: /* 0x011 */
		case 0B01011:
			/* LDRT */
			return arm_inst_ldrt(inst, regs, vcpu);
		case 0B00100: /* xx1x0 not 0x110 */
		case 0B01100:
		case 0B10100:
		case 0B10110:
		case 0B11100:
		case 0B11110:
			/* STRB (immediate) */
			return arm_inst_strb_i(inst, regs, vcpu);
		case 0B00110: /* 0x110 */
		case 0B01110:
			/* STRBT */
			return arm_inst_strbt(inst, regs, vcpu);
		case 0B00101: /* xx1x1 not 0x111 */
		case 0B01101:
		case 0B10101:
		case 0B10111:
		case 0B11101:
		case 0B11111:
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
			break;
		case 0B00111: /* 0x111 */
		case 0B01111:
			/* LDRBT */
			return arm_inst_ldrbt(inst, regs, vcpu);
		default:
			break;
		};
	} else if (!(inst & ARM_INST_LDRSTR_B_MASK)) {
		switch (op1) {
		case 0B00000: /* xx0x0 not 0x010 */
		case 0B01000:
		case 0B10000:
		case 0B10010:
		case 0B11000:
		case 0B11010:
			/* STR (register) */
			return arm_inst_str_r(inst, regs, vcpu);
		case 0B00010: /* 0x010 */
		case 0B01010:
			/* STRT */
			return arm_inst_strt(inst, regs, vcpu);
		case 0B00001: /* xx0x1 not 0x011 */
		case 0B01001:
		case 0B10001:
		case 0B10011:
		case 0B11001:
		case 0B11011:
			/* LDR (register) */
			return arm_inst_ldr_r(inst, regs, vcpu);
		case 0B00011: /* 0x011 */
		case 0B01011:
			/* LDRT */
			return arm_inst_ldrt(inst, regs, vcpu);
		case 0B00100: /* xx1x0 not 0x110 */
		case 0B01100:
		case 0B10100:
		case 0B10110:
		case 0B11100:
		case 0B11110:
			/* STRB (register) */
			return arm_inst_strb_r(inst, regs, vcpu);
		case 0B00110: /* 0x110 */
		case 0B01110:
			/* STRBT */
			return arm_inst_strbt(inst, regs, vcpu);
		case 0B00101: /* xx1x1 not 0x111 */
		case 0B01101:
		case 0B10101:
		case 0B10111:
		case 0B11101:
		case 0B11111:
			/* LDRB (register) */
			return arm_inst_ldrb_r(inst, regs, vcpu);
		case 0B00111: /* 0x111 */
		case 0B01111:
			/* LDRBT */
			return arm_inst_ldrbt(inst, regs, vcpu);
		default:
			break;
		};
	}

	arm_unpredictable(regs, vcpu, inst, __func__);
	return VMM_EFAIL;
}

/** Emulate media instructions */
static int arm_instgrp_media(u32 inst, 
				arch_regs_t * regs, struct vmm_vcpu * vcpu)
{
	arm_unpredictable(regs, vcpu, inst, __func__);
	return VMM_EFAIL;
}

/** Emulate Block load (LDMIA, LDMDA, LDMIB, LDMDB) instructions */
static int arm_inst_ldm(u32 inst, 
				arch_regs_t * regs, struct vmm_vcpu * vcpu)
{
	u32 cond, wback, Rn, op, reg_list, rc;
	bool is_xx1xxx; /* whether increment or decrement */
	bool is_x1xxxx; /* whether before or after */ 
	u32 mask = 0, bit_count = 0;
	u32 data = 0;
	u32 address = 0, old_address = 0;
	int i = 0;

	cond = ARM_INST_DECODE(inst, ARM_INST_COND_MASK, ARM_INST_COND_SHIFT);
	op = ARM_INST_BITS(inst, ARM_INST_BRBLK_OP_END, ARM_INST_BRBLK_OP_START);
	Rn = ARM_INST_BITS(inst,
			   ARM_INST_LDMSTM_RN_END,
			   ARM_INST_LDMSTM_RN_START);
	reg_list = ARM_INST_BITS(inst,
				 ARM_INST_LDMSTM_REGLIST_END,
				 ARM_INST_LDMSTM_REGLIST_START);

	wback = ARM_INST_BITS(inst, ARM_INST_LDMSTM_W_END, ARM_INST_LDMSTM_W_START);

	if ((Rn == 15) || (!reg_list)) {
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	}

	if (wback && (reg_list & (0x1 << Rn))) {
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	}

	/* get all the flags */
	is_xx1xxx = !!(op & 0x08);
	is_x1xxxx = !!(op & 0x10);   

	mask = 0x1;
	/* get the bitcount */
	for (i = 0; i <= 15; i++) {
		if (reg_list & mask) {
			bit_count++;
		}
		mask = mask << 1;
	}
	
	if (arm_condition_passed(cond, regs)) {
		old_address = cpu_vcpu_reg_read(vcpu, regs, Rn);

		if (is_xx1xxx){
			/* increment operation, if before then add 4 to it */
			address = old_address + (4 * is_x1xxxx);
		} else {
			/* decrement operation if after then add 4 to it */
			address = old_address - (4 * bit_count) + (4 * !(is_x1xxxx));
		}

		mask = 0x1;
		/* parse through reg_list */
		for (i = 0; i < 15; i++) {
			if (reg_list & mask) {
				if ((rc = cpu_vcpu_mem_read(vcpu, regs,
						address, &data, 4, FALSE))) {
					return rc;
				}
				cpu_vcpu_reg_write(vcpu, regs, i, data);
				address = address + 4;			
			} 
			mask = mask << 1;
		}
		if (reg_list >> 15) {
			/* TODO: check the address bits to select instruction set */
			if ((rc = cpu_vcpu_mem_read(vcpu, regs,
						address, &(arm_pc(regs)), 4, FALSE))) {
				return rc;
			}
		}

		if (wback) {
			if (is_xx1xxx){
				/* increment operation */
				cpu_vcpu_reg_write(vcpu, regs, Rn, old_address + (4 * bit_count));
			} else {
				/* decrement operation */
				cpu_vcpu_reg_write(vcpu, regs, Rn, old_address - (4 * bit_count));
			}
		}
	}

	arm_pc(regs) += 4;

	return VMM_OK;
}

/** Emulate Block store (STMIA, STMDA, STMIB, STMDB) instructions */
static int arm_inst_stm(u32 inst, 
				arch_regs_t * regs, struct vmm_vcpu * vcpu)
{
	u32 cond, wback, Rn, op, reg_list, rc;
	bool is_xx1xxx; /* whether increment or decrement */
	bool is_x1xxxx; /* whether before or after */ 
	u32 mask = 0, bit_count = 0;
	u32 data = 0;
	u32 address = 0, old_address = 0;
	int i = 0;

	cond = ARM_INST_DECODE(inst, ARM_INST_COND_MASK, ARM_INST_COND_SHIFT);
	op = ARM_INST_BITS(inst, ARM_INST_BRBLK_OP_END, ARM_INST_BRBLK_OP_START);
	Rn = ARM_INST_BITS(inst,
			   ARM_INST_LDMSTM_RN_END,
			   ARM_INST_LDMSTM_RN_START);
	reg_list = ARM_INST_BITS(inst,
				 ARM_INST_LDMSTM_REGLIST_END,
				 ARM_INST_LDMSTM_REGLIST_START);

	wback = ARM_INST_BITS(inst, ARM_INST_LDMSTM_W_END, ARM_INST_LDMSTM_W_START);

	if ((Rn == 15) || (!reg_list)) {
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	}

	/* get all the flags */
	is_xx1xxx = !!(op & 0x08);
	is_x1xxxx = !!(op & 0x10);   

	mask = 0x1;
	/* get the bitcount */
	for (i = 0; i <= 15; i++) {
		if (reg_list & mask) {
			bit_count++;
		}
		mask = mask << 1;
	}
	
	if (arm_condition_passed(cond, regs)) {
		old_address = cpu_vcpu_reg_read(vcpu, regs, Rn);

		if (is_xx1xxx){
			/* increment operation, if before then add 4 to it */
			address = old_address + (4 * is_x1xxxx);
		} else {
			/* decrement operation if after then add 4 to it */
			address = old_address - (4 * bit_count) + (4 * !(is_x1xxxx));
		}

		mask = 0x1;
		/* parse through reg_list */
		for (i = 0; i < 15; i++) {
			if (reg_list & mask) {
				data = cpu_vcpu_reg_read(vcpu, regs, i);
				if ((rc = cpu_vcpu_mem_write(vcpu, regs,
						address, &data, 4, FALSE))) {
					return rc;
				}
				address = address + 4;			
			}
			mask = mask << 1;
		}
		if (reg_list >> 15) {
			data = arm_pc(regs) + 8;
			if ((rc = cpu_vcpu_mem_write(vcpu, regs, address, &data, 4, FALSE))) {
				return rc;
			}
		}
		if (wback) {
			if (is_xx1xxx){
				/* increment operation */
				cpu_vcpu_reg_write(vcpu, regs, Rn, old_address + (4 * bit_count));
			} else {
				/* decrement operation */
				cpu_vcpu_reg_write(vcpu, regs, Rn, old_address - (4 * bit_count));
			}
		}
	}

	arm_pc(regs) += 4;

	return VMM_OK;
}

/** Emulate branch, branch with link, and block transfer instructions */
static int arm_instgrp_brblk(u32 inst, 
				arch_regs_t * regs, struct vmm_vcpu * vcpu)
{
	u32 op, Rn, reg_list;
	u32 is_1xxxxx; /* whether branch or block */
	u32 is_xxxxx1; /* whether load or store */
	bool is_list_geq_2; /* whether reg_list length is greater than equal to 2 */

	op = ARM_INST_BITS(inst, ARM_INST_BRBLK_OP_END, ARM_INST_BRBLK_OP_START);
	Rn = ARM_INST_BITS(inst, ARM_INST_LDMSTM_RN_END, ARM_INST_LDMSTM_RN_START);
	reg_list = ARM_INST_BITS(inst,
				 ARM_INST_LDMSTM_REGLIST_END,
				 ARM_INST_LDMSTM_REGLIST_START);
	is_1xxxxx = (op & 0x20);
	is_list_geq_2 = !!(reg_list && ((reg_list - 1) & reg_list));	

	if(!is_1xxxxx) {
		is_xxxxx1 = (op & 0x01);
		if (!is_xxxxx1) {
			/* Emulate Store block transfer instructions */
			if ((op == 0x12) && (Rn == 13) && is_list_geq_2){
				/* TODO: Emulate PUSH instruction */				

			} else {
				/* Emulate rest of the store multiple */
				return arm_inst_stm(inst, regs, vcpu);
			}
		} else {
			/* Emulate Load block transfer instructions */	
			if ((op == 0x0B) && (Rn == 13) && is_list_geq_2){
				/* TODO: Emulate POP instruction */				

			} else {
				/* Emulate rest of the load multiple */
				return arm_inst_ldm(inst, regs, vcpu);
			}
		}

	} else {
		/* TODO: Emulate branch and branch with link instructions */
	}

	arm_unpredictable(regs, vcpu, inst, __func__);
	return VMM_EFAIL;
}

/** Emulate 'stc/stc2' instruction */
static int arm_inst_stcx(u32 inst, 
				arch_regs_t * regs, struct vmm_vcpu * vcpu)
{
	int rc;
	u32 cond, P, U, D, W, Rn, CRd, coproc, imm8;
	u32 imm32, offset_addr, address, i, data;
	bool index, add, wback, uopt;
	struct cpu_vcpu_coproc *cp = NULL;
	cond = ARM_INST_DECODE(inst, ARM_INST_COND_MASK, ARM_INST_COND_SHIFT);
	P = ARM_INST_BITS(inst, ARM_INST_STCX_P_END, ARM_INST_STCX_P_START);
	U = ARM_INST_BITS(inst, ARM_INST_STCX_U_END, ARM_INST_STCX_U_START);
	D = ARM_INST_BITS(inst, ARM_INST_STCX_D_END, ARM_INST_STCX_D_START);
	W = ARM_INST_BITS(inst, ARM_INST_STCX_W_END, ARM_INST_STCX_W_START);
	Rn = ARM_INST_BITS(inst, ARM_INST_STCX_RN_END, ARM_INST_STCX_RN_START);
	CRd = ARM_INST_BITS(inst,
			    ARM_INST_STCX_CRD_END, ARM_INST_STCX_CRD_START);
	coproc = ARM_INST_BITS(inst,
			       ARM_INST_STCX_COPROC_END,
			       ARM_INST_STCX_COPROC_START);
	imm8 = ARM_INST_BITS(inst,
			     ARM_INST_STCX_IMM8_END, ARM_INST_STCX_IMM8_START);
	imm32 = arm_zero_extend((imm8 << 2), 32);
	index = (P == 1) ? TRUE : FALSE;
	add = (U == 1) ? TRUE : FALSE;
	wback = (W == 1) ? TRUE : FALSE;
	uopt = ((P == 0) && (W == 0) && (U == 1)) ? TRUE : FALSE;
	cp = cpu_vcpu_coproc_get(coproc);
	if ((Rn == 15) && wback) {
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	}
	if ((cp->ldcstc_accept == NULL) || 
	    (cp->ldcstc_done == NULL) ||
	    (cp->ldcstc_read == NULL)) {
		vmm_vcpu_irq_assert(vcpu, CPU_UNDEF_INST_IRQ, 0x0);
		return VMM_OK;
	}
	if (arm_condition_passed(cond, regs)) {
		if (!cp->ldcstc_accept(vcpu, regs, D, CRd, uopt, imm8)) {
			vmm_vcpu_irq_assert(vcpu, CPU_UNDEF_INST_IRQ, 0x0);
			return VMM_OK;
		} else {
			data = cpu_vcpu_reg_read(vcpu, regs, Rn);
			offset_addr = (add) ? (data + imm32) : (data - imm32);
			address = (index) ? offset_addr : data;
			i = 0;
			while (!cp->ldcstc_done(vcpu, regs, i, D, 
							CRd, uopt, imm8)) {
				data = cp->ldcstc_read(vcpu, regs, i, D, 
								CRd, uopt, imm8);
				if ((rc = cpu_vcpu_mem_write(vcpu, regs,
						address, &data, 4, FALSE))) {
					return rc;
				}
				address += 4;
			}
			if (wback) {
				cpu_vcpu_reg_write(vcpu, regs, Rn, offset_addr);
			}
		}
	}
	arm_pc(regs) += 4;
	return VMM_OK;
}

/** Emulate 'ldc_i/ldc2_i' instruction */
static int arm_inst_ldcx_i(u32 inst, 
				arch_regs_t * regs, struct vmm_vcpu * vcpu)
{
	int rc;
	u32 cond, P, U, D, W, Rn, CRd, coproc, imm8;
	u32 imm32, offset_addr, address, i, data;
	bool index, add, wback, uopt;
	struct cpu_vcpu_coproc *cp = NULL;
	cond = ARM_INST_DECODE(inst, ARM_INST_COND_MASK, ARM_INST_COND_SHIFT);
	P = ARM_INST_BITS(inst, ARM_INST_LDCX_I_P_END, ARM_INST_LDCX_I_P_START);
	U = ARM_INST_BITS(inst, ARM_INST_LDCX_I_U_END, ARM_INST_LDCX_I_U_START);
	D = ARM_INST_BITS(inst, ARM_INST_LDCX_I_D_END, ARM_INST_LDCX_I_D_START);
	W = ARM_INST_BITS(inst, ARM_INST_LDCX_I_W_END, ARM_INST_LDCX_I_W_START);
	Rn = ARM_INST_BITS(inst,
			   ARM_INST_LDCX_I_RN_END, ARM_INST_LDCX_I_RN_START);
	CRd = ARM_INST_BITS(inst,
			    ARM_INST_LDCX_I_CRD_END, ARM_INST_LDCX_I_CRD_START);
	coproc = ARM_INST_BITS(inst,
			       ARM_INST_LDCX_I_COPROC_END,
			       ARM_INST_LDCX_I_COPROC_START);
	imm8 = ARM_INST_BITS(inst,
			     ARM_INST_LDCX_I_IMM8_END,
			     ARM_INST_LDCX_I_IMM8_START);
	imm32 = arm_zero_extend((imm8 << 2), 32);
	index = (P == 1) ? TRUE : FALSE;
	add = (U == 1) ? TRUE : FALSE;
	wback = (W == 1) ? TRUE : FALSE;
	uopt = ((P == 0) && (W == 0) && (U == 1)) ? TRUE : FALSE;
	cp = cpu_vcpu_coproc_get(coproc);
	if ((Rn == 15) && wback) {
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	}
	if ((cp->ldcstc_accept == NULL) || 
	    (cp->ldcstc_done == NULL) ||
	    (cp->ldcstc_write == NULL)) {
		vmm_vcpu_irq_assert(vcpu, CPU_UNDEF_INST_IRQ, 0x0);
		return VMM_OK;
	}
	if (arm_condition_passed(cond, regs)) {
		if (!cp->ldcstc_accept(vcpu, regs, D, CRd, uopt, imm8)) {
			vmm_vcpu_irq_assert(vcpu, CPU_UNDEF_INST_IRQ, 0x0);
			return VMM_OK;
		} else {
			data = cpu_vcpu_reg_read(vcpu, regs, Rn);
			offset_addr = (add) ? (data + imm32) : (data - imm32);
			address = (index) ? offset_addr : data;
			i = 0;
			while (!cp->ldcstc_done(vcpu, regs, i, D, 
							CRd, uopt, imm8)) {
				data = 0x0;
				if ((rc = cpu_vcpu_mem_read(vcpu, regs,
						address, &data, 4, FALSE))) {
					return rc;
				}
				cp->ldcstc_write(vcpu, regs, i, D, 
							CRd, uopt, imm8, data);
				address += 4;
			}
			if (wback) {
				cpu_vcpu_reg_write(vcpu, regs, Rn, offset_addr);
			}
		}
	}
	arm_pc(regs) += 4;
	return VMM_OK;
}

/** Emulate 'ldc_l/ldc2_l' instruction */
static int arm_inst_ldcx_l(u32 inst, 
				arch_regs_t * regs, struct vmm_vcpu * vcpu)
{
	int rc;
	u32 cond, P, U, D, W, CRd, coproc, imm8;
	u32 imm32, offset_addr, address, i, data;
	bool index, add, uopt;
	struct cpu_vcpu_coproc *cp = NULL;
	cond = ARM_INST_DECODE(inst, ARM_INST_COND_MASK, ARM_INST_COND_SHIFT);
	P = ARM_INST_BITS(inst, ARM_INST_LDCX_L_P_END, ARM_INST_LDCX_L_P_START);
	U = ARM_INST_BITS(inst, ARM_INST_LDCX_L_U_END, ARM_INST_LDCX_L_U_START);
	D = ARM_INST_BITS(inst, ARM_INST_LDCX_L_D_END, ARM_INST_LDCX_L_D_START);
	W = ARM_INST_BITS(inst, ARM_INST_LDCX_L_W_END, ARM_INST_LDCX_L_W_START);
	CRd = ARM_INST_BITS(inst,
			    ARM_INST_LDCX_L_CRD_END, ARM_INST_LDCX_L_CRD_START);
	coproc = ARM_INST_BITS(inst,
			       ARM_INST_LDCX_L_COPROC_END,
			       ARM_INST_LDCX_L_COPROC_START);
	imm8 = ARM_INST_BITS(inst,
			     ARM_INST_LDCX_L_IMM8_END,
			     ARM_INST_LDCX_L_IMM8_START);
	imm32 = arm_zero_extend((imm8 << 2), 32);
	index = (P == 1) ? TRUE : FALSE;
	add = (U == 1) ? TRUE : FALSE;
	uopt = ((P == 0) && (W == 0) && (U == 1)) ? TRUE : FALSE;
	cp = cpu_vcpu_coproc_get(coproc);
	if (W == 1) {
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	}
	if ((cp->ldcstc_accept == NULL) || 
	    (cp->ldcstc_done == NULL) ||
	    (cp->ldcstc_write == NULL)) {
		vmm_vcpu_irq_assert(vcpu, CPU_UNDEF_INST_IRQ, 0x0);
		return VMM_OK;
	}
	if (arm_condition_passed(cond, regs)) {
		if (!cp->ldcstc_accept(vcpu, regs, D, CRd, uopt, imm8)) {
			vmm_vcpu_irq_assert(vcpu, CPU_UNDEF_INST_IRQ, 0x0);
			return VMM_OK;
		} else {
			offset_addr = (add) ? (arm_align(arm_pc(regs), 4) + imm32) :
					      (arm_align(arm_pc(regs), 4) - imm32);
			address = (index) ? offset_addr : arm_align(arm_pc(regs), 4);
			i = 0;
			while (!cp->ldcstc_done(vcpu, regs, i, D, 
							CRd, uopt, imm8)) {
				data = 0x0;
				if ((rc = cpu_vcpu_mem_read(vcpu, regs,
						address, &data, 4, FALSE))) {
					return rc;
				}
				cp->ldcstc_write(vcpu, regs, i, D, 
							CRd, uopt, imm8, data);
				address += 4;
			}
		}
	}
	arm_pc(regs) += 4;
	return VMM_OK;
}

/** Emulate 'mcrr/mcrr2' instruction */
static int arm_inst_mcrrx(u32 inst, 
				arch_regs_t * regs, struct vmm_vcpu * vcpu)
{
	u32 cond, Rt2, Rt, coproc, opc1, CRm;
	u32 data, data2;
	struct cpu_vcpu_coproc *cp = NULL;
	cond = ARM_INST_DECODE(inst, ARM_INST_COND_MASK, ARM_INST_COND_SHIFT);
	Rt2 = ARM_INST_BITS(inst,
			    ARM_INST_MCRRX_RT2_END, 
			    ARM_INST_MCRRX_RT2_START);
	Rt = ARM_INST_BITS(inst,
			   ARM_INST_MCRRX_RT_END, 
			   ARM_INST_MCRRX_RT_START);
	coproc = ARM_INST_BITS(inst,
			       ARM_INST_MCRRX_COPROC_END,
			       ARM_INST_MCRRX_COPROC_START);
	opc1 = ARM_INST_BITS(inst, 
			     ARM_INST_MCRRX_OPC1_END, 
			     ARM_INST_MCRRX_OPC1_START);
	CRm = ARM_INST_BITS(inst,
			    ARM_INST_MCRRX_CRM_END, 
			    ARM_INST_MCRRX_CRM_START);
	cp = cpu_vcpu_coproc_get(coproc);
	if ((Rt == 15) || (Rt2 == 15)) {
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	}
	if ((cp->write2 == NULL)) {
		vmm_vcpu_irq_assert(vcpu, CPU_UNDEF_INST_IRQ, 0x0);
		return VMM_OK;
	}
	if (arm_condition_passed(cond, regs)) {
		data = cpu_vcpu_reg_read(vcpu, regs, Rt);
		data2 = cpu_vcpu_reg_read(vcpu, regs, Rt2);
		if (!cp->write2(vcpu, regs, opc1, CRm, data, data2)) {
			vmm_vcpu_irq_assert(vcpu, CPU_UNDEF_INST_IRQ, 0x0);
			return VMM_OK;
		}
	}
	arm_pc(regs) += 4;
	return VMM_OK;
}

/** Emulate 'mrrc/mrrc2' instruction */
static int arm_inst_mrrcx(u32 inst, 
				arch_regs_t * regs, struct vmm_vcpu * vcpu)
{
	u32 cond, Rt2, Rt, coproc, opc1, CRm;
	u32 data, data2;
	struct cpu_vcpu_coproc *cp = NULL;
	cond = ARM_INST_DECODE(inst, ARM_INST_COND_MASK, ARM_INST_COND_SHIFT);
	Rt2 = ARM_INST_BITS(inst,
			    ARM_INST_MRRCX_RT2_END, 
			    ARM_INST_MRRCX_RT2_START);
	Rt = ARM_INST_BITS(inst,
			   ARM_INST_MRRCX_RT_END, 
			   ARM_INST_MRRCX_RT_START);
	coproc = ARM_INST_BITS(inst,
			       ARM_INST_MRRCX_COPROC_END,
			       ARM_INST_MRRCX_COPROC_START);
	opc1 = ARM_INST_BITS(inst, 
			     ARM_INST_MRRCX_OPC1_END, 
			     ARM_INST_MRRCX_OPC1_START);
	CRm = ARM_INST_BITS(inst,
			    ARM_INST_MRRCX_CRM_END, 
			    ARM_INST_MRRCX_CRM_START);
	cp = cpu_vcpu_coproc_get(coproc);
	if ((Rt == 15) || (Rt2 == 15)) {
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	}
	if ((cp->read2 == NULL)) {
		vmm_vcpu_irq_assert(vcpu, CPU_UNDEF_INST_IRQ, 0x0);
		return VMM_OK;
	}
	if (arm_condition_passed(cond, regs)) {
		data = 0x0;
		data2 = 0x0;
		if (!cp->read2(vcpu, regs, opc1, CRm, &data, &data2)) {
			vmm_vcpu_irq_assert(vcpu, CPU_UNDEF_INST_IRQ, 0x0);
			return VMM_OK;
		}
		cpu_vcpu_reg_write(vcpu, regs, Rt, data);
		cpu_vcpu_reg_write(vcpu, regs, Rt2, data2);
	}
	arm_pc(regs) += 4;
	return VMM_OK;
}

/** Emulate 'cdp/cdp2' instruction */
static int arm_inst_cdpx(u32 inst, 
				arch_regs_t * regs, struct vmm_vcpu * vcpu)
{
	u32 cond, opc1, opc2, coproc, CRd, CRn, CRm;
	struct cpu_vcpu_coproc *cp = NULL;
	cond = ARM_INST_DECODE(inst, ARM_INST_COND_MASK, ARM_INST_COND_SHIFT);
	opc1 = ARM_INST_BITS(inst, 
			     ARM_INST_CDPX_OPC1_END, ARM_INST_CDPX_OPC1_START);
	CRn = ARM_INST_BITS(inst,
			    ARM_INST_CDPX_CRN_END, ARM_INST_CDPX_CRN_START);
	CRd = ARM_INST_BITS(inst,
			    ARM_INST_CDPX_CRD_END, ARM_INST_CDPX_CRD_START);
	coproc = ARM_INST_BITS(inst,
			       ARM_INST_CDPX_COPROC_END,
			       ARM_INST_CDPX_COPROC_START);
	opc2 = ARM_INST_BITS(inst, 
			     ARM_INST_CDPX_OPC2_END, ARM_INST_CDPX_OPC2_START);
	CRm = ARM_INST_BITS(inst,
			    ARM_INST_CDPX_CRM_END, ARM_INST_CDPX_CRM_START);
	cp = cpu_vcpu_coproc_get(coproc);
	if ((cp->data_process == NULL)) {
		vmm_vcpu_irq_assert(vcpu, CPU_UNDEF_INST_IRQ, 0x0);
		return VMM_OK;
	}
	if (arm_condition_passed(cond, regs)) {
		if (!cp->data_process(vcpu, regs, opc1, opc2, CRd, CRn, CRm)) {
			vmm_vcpu_irq_assert(vcpu, CPU_UNDEF_INST_IRQ, 0x0);
			return VMM_OK;
		}
	}
	arm_pc(regs) += 4;
	return VMM_OK;
}

/** Emulate 'mcr/mcr2' instruction */
static int arm_inst_mcrx(u32 inst, 
				arch_regs_t * regs, struct vmm_vcpu * vcpu)
{
	u32 cond, opc1, opc2, coproc, Rt, CRn, CRm;
	u32 data;
	struct cpu_vcpu_coproc *cp = NULL;
	cond = ARM_INST_DECODE(inst, ARM_INST_COND_MASK, ARM_INST_COND_SHIFT);
	opc1 = ARM_INST_BITS(inst, 
			     ARM_INST_MCRX_OPC1_END, ARM_INST_MCRX_OPC1_START);
	CRn = ARM_INST_BITS(inst,
			    ARM_INST_MCRX_CRN_END, ARM_INST_MCRX_CRN_START);
	Rt = ARM_INST_BITS(inst,
			    ARM_INST_MCRX_RT_END, ARM_INST_MCRX_RT_START);
	coproc = ARM_INST_BITS(inst,
			       ARM_INST_MCRX_COPROC_END,
			       ARM_INST_MCRX_COPROC_START);
	opc2 = ARM_INST_BITS(inst, 
			     ARM_INST_MCRX_OPC2_END, ARM_INST_MCRX_OPC2_START);
	CRm = ARM_INST_BITS(inst,
			    ARM_INST_MCRX_CRM_END, ARM_INST_MCRX_CRM_START);	
	cp = cpu_vcpu_coproc_get(coproc);
	if ((cp->write == NULL)) {
		vmm_vcpu_irq_assert(vcpu, CPU_UNDEF_INST_IRQ, 0x0);
		return VMM_OK;
	}
	if (arm_condition_passed(cond, regs)) {
		data = cpu_vcpu_reg_read(vcpu, regs, Rt);
		if (!cp->write(vcpu, regs, opc1, opc2, CRn, CRm, data)) {
			vmm_vcpu_irq_assert(vcpu, CPU_UNDEF_INST_IRQ, 0x0);
			return VMM_OK;
		}
	}
	arm_pc(regs) += 4;
	return VMM_OK;
}

/** Emulate 'mrc/mrc2' instruction */
static int arm_inst_mrcx(u32 inst, 
				arch_regs_t * regs, struct vmm_vcpu * vcpu)
{
	u32 cond, opc1, opc2, coproc, Rt, CRn, CRm;
	u32 data;
	struct cpu_vcpu_coproc *cp = NULL;
	cond = ARM_INST_DECODE(inst, ARM_INST_COND_MASK, ARM_INST_COND_SHIFT);
	opc1 = ARM_INST_BITS(inst, 
			     ARM_INST_MRCX_OPC1_END, ARM_INST_MRCX_OPC1_START);
	CRn = ARM_INST_BITS(inst,
			    ARM_INST_MRCX_CRN_END, ARM_INST_MRCX_CRN_START);
	Rt = ARM_INST_BITS(inst,
			    ARM_INST_MRCX_RT_END, ARM_INST_MRCX_RT_START);
	coproc = ARM_INST_BITS(inst,
			       ARM_INST_MRCX_COPROC_END,
			       ARM_INST_MRCX_COPROC_START);
	opc2 = ARM_INST_BITS(inst, 
			     ARM_INST_MRCX_OPC2_END, ARM_INST_MRCX_OPC2_START);
	CRm = ARM_INST_BITS(inst,
			    ARM_INST_MRCX_CRM_END, ARM_INST_MRCX_CRM_START);	
	cp = cpu_vcpu_coproc_get(coproc);
	if ((cp->read == NULL)) {
		vmm_vcpu_irq_assert(vcpu, CPU_UNDEF_INST_IRQ, 0x0);
		return VMM_OK;
	}
	if (arm_condition_passed(cond, regs)) {
		data = 0x0;
		if (!cp->read(vcpu, regs, opc1, opc2, CRn, CRm, &data)) {
			vmm_vcpu_irq_assert(vcpu, CPU_UNDEF_INST_IRQ, 0x0);
			return VMM_OK;
		}
		/* If the PC is the target register then the mrc
		 * instruction does not change its value.
		 */
		if (Rt < 15) {
			cpu_vcpu_reg_write(vcpu, regs, Rt, data);
		}
	}
	arm_pc(regs) += 4;
	return VMM_OK;
}

/** Emulate 'svc' instruction */
static int arm_inst_svc(u32 inst, 
				arch_regs_t * regs, struct vmm_vcpu * vcpu)
{
	/* For now don't do anything for SVC instructions */
	arm_pc(regs) += 4;
	return VMM_OK;
}

static int arm_instgrp_coproc(u32 inst, 
				arch_regs_t * regs, struct vmm_vcpu * vcpu)
{
	u32 op1, rn, cpro;

	op1 = ARM_INST_DECODE(inst,
			      ARM_INST_COPROC_OP1_MASK,
			      ARM_INST_COPROC_OP1_SHIFT);
	cpro = ARM_INST_DECODE(inst,
			       ARM_INST_COPROC_CPRO_MASK,
			       ARM_INST_COPROC_CPRO_SHIFT);

	switch (cpro) {
	case 0B1010: /* SMID and Floating point instructions */
	case 0B1011:
		switch (op1) {
		case 0B000000: /* 00000x */
		case 0B000001:
			/* Undefined instructions */
			vmm_vcpu_irq_assert(vcpu, CPU_UNDEF_INST_IRQ, 0x0);
			return VMM_OK;
		case 0B000010: /* 0xxxxx not 000x0x*/
		case 0B000011:
		case 0B000110:
		case 0B000111:
		case 0B001000:
		case 0B001001:
		case 0B001010:
		case 0B001011:
		case 0B001100:
		case 0B001101:
		case 0B001110:
		case 0B001111:
		case 0B010000:
		case 0B010001:
		case 0B010010:
		case 0B010011:
		case 0B010100:
		case 0B010101:
		case 0B010110:
		case 0B010111:
		case 0B011000:
		case 0B011001:
		case 0B011010:
		case 0B011011:
		case 0B011100:
		case 0B011101:
		case 0B011110:
		case 0B011111:
			/* FIXME:
			 * Advanced SIMD, VFP Extension register 
			 * load/store instructions 
			 */
			break;
		case 0B000100: /* 00010x */
		case 0B000101:
			/* FIXME:
			 * Advanced SIMD, VFP 64-bit transfers between 
			 * ARM core and extension registers 
			 */
			break;
		case 0B100000: /* 10xxxx */
		case 0B100001:
		case 0B100010:
		case 0B100011:
		case 0B100100:
		case 0B100101:
		case 0B100110:
		case 0B100111:
		case 0B101000:
		case 0B101001:
		case 0B101010:
		case 0B101011:
		case 0B101100:
		case 0B101101:
		case 0B101110:
		case 0B101111:
			if (inst & ARM_INST_COPROC_OP_MASK) {
				/* FIXME:
				 * Advanced SIMD, VFP 8, 16, & 32-bit transfer
				 * transfer between ARM core and extension 
				 * registers 
				 */
			} else {
				/* FIXME:
				 * VFP data-processing instructions 
				 */
			}
			break;
		case 0B110000: /* 11xxxx */
		case 0B110001:
		case 0B110010:
		case 0B110011:
		case 0B110100:
		case 0B110101:
		case 0B110110:
		case 0B110111:
		case 0B111000:
		case 0B111001:
		case 0B111010:
		case 0B111011:
		case 0B111100:
		case 0B111101:
		case 0B111110:
		case 0B111111:
			/* Supervisor Call SVC (previously SWI) */
			return arm_inst_svc(inst, regs, vcpu);
		default:
			break;
		};
		break;
	default: /* Generic coprocessor instructions */
		switch (op1) {
		case 0B000000:  /* 00000x */
		case 0B000001:
			/* Undefined instructions */
			vmm_vcpu_irq_assert(vcpu, CPU_UNDEF_INST_IRQ, 0x0);
			return VMM_OK;
		case 0B000010: /* 0xxxx0 not 000x00*/
		case 0B000110:
		case 0B001000:
		case 0B001010:
		case 0B001100:
		case 0B001110:
		case 0B010000:
		case 0B010010:
		case 0B010100:
		case 0B010110:
		case 0B011000:
		case 0B011010:
		case 0B011100:
		case 0B011110:
			/* Store Coprocessor 
			 * STC, STC2 
			 */
			return arm_inst_stcx(inst, regs, vcpu);
		case 0B000011: /* 0xxxx1 not 000x01 */
		case 0B000111:
		case 0B001001:
		case 0B001011:
		case 0B001101:
		case 0B001111:
		case 0B010001:
		case 0B010011:
		case 0B010101:
		case 0B010111:
		case 0B011001:
		case 0B011011:
		case 0B011101:
		case 0B011111:
			rn = ARM_INST_DECODE(inst,
					ARM_INST_COPROC_RN_MASK, 
					ARM_INST_COPROC_RN_SHIFT);
			if (rn == 0xF) {
				/* Load Coprocessor 
				 * LDC, LDC2 (literal) 
				 */
				return arm_inst_ldcx_l(inst, regs, vcpu);
			} else {
				/* Load Coprocessor 
				 * LDC, LDC2 (immediate) 
				 */
				return arm_inst_ldcx_i(inst, regs, vcpu);
			}
			break;
		case 0B000100: /* 000100 */
			/* Move to Coprocessor from two ARM core registers
			 * MCRR, MCRR2 
			 */
			return arm_inst_mcrrx(inst, regs, vcpu);
		case 0B000101: /* 000101 */
			/* Move to two ARM core registers from Coprocessor
			 * MRRC, MRRC2 
			 */
			return arm_inst_mrrcx(inst, regs, vcpu);
		case 0B100000: /* 10xxx0 */
		case 0B100010:
		case 0B100100:
		case 0B100110:
		case 0B101000:
		case 0B101010:
		case 0B101100:
		case 0B101110:
			if (inst & ARM_INST_COPROC_OP_MASK) {
				/* Move to Coprocessor from ARM core register
				 * MCR, MCR2 
				 */
				return arm_inst_mcrx(inst, regs, vcpu);
			} else {
				/* Coprocessor data operations 
				 * CDP, CDP2 
				 */
				return arm_inst_cdpx(inst, regs, vcpu);
			}
			break;
		case 0B100001: /* 10xxx1 */
		case 0B100011:
		case 0B100101:
		case 0B100111:
		case 0B101001:
		case 0B101011:
		case 0B101101:
		case 0B101111:
			if (inst & ARM_INST_COPROC_OP_MASK) {
				/* Move to ARM core register from Coprocessor
				 * MRC, MRC2 
				 */
				return arm_inst_mrcx(inst, regs, vcpu);
			} else {
				/* Coprocessor data operations 
				 * CDP, CDP2 
				 */
				return arm_inst_cdpx(inst, regs, vcpu);
			}
			break;
		case 0B110000: /* 11xxxx */
		case 0B110001:
		case 0B110010:
		case 0B110011:
		case 0B110100:
		case 0B110101:
		case 0B110110:
		case 0B110111:
		case 0B111000:
		case 0B111001:
		case 0B111010:
		case 0B111011:
		case 0B111100:
		case 0B111101:
		case 0B111110:
		case 0B111111:
			/* Supervisor Call SVC (previously SWI) */
			return arm_inst_svc(inst, regs, vcpu);
		default:
			break;
		};
		break;
	};

	arm_unpredictable(regs, vcpu, inst, __func__);
	return VMM_EFAIL;
}

int emulate_arm_inst(struct vmm_vcpu *vcpu, arch_regs_t *regs, u32 inst)
{
	u32 op1, op;

	/* Sanity check */
	if (!vcpu) {
		return VMM_EFAIL;
	}
	if (!vcpu->is_normal) {
		return VMM_EFAIL;
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
