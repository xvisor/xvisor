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
 * @file cpu_vcpu_emulate_arm.c
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief source code to emulate ARM instructions
 */

#include <vmm_error.h>
#include <vmm_cpu.h>
#include <vmm_regs.h>
#include <vmm_scheduler.h>
#include <vmm_vcpu_irq.h>
#include <cpu_defines.h>
#include <cpu_vcpu_helper.h>
#include <cpu_vcpu_coproc.h>
#include <cpu_vcpu_cp15.h>
#include <cpu_vcpu_emulate_arm.h>

#define arm_unpredictable(regs, vcpu)		cpu_vcpu_halt(vcpu, regs)
#define arm_zero_extend(imm, bits)		((u32)(imm))
#define arm_align(addr, nbytes)			((addr) - ((addr) % (nbytes)))

static inline u32 arm_sign_extend(u32 imm, u32 len, u32 bits)
{
	if (imm & (1 << (len - 1))) {
		imm = imm | (~((1 << len) - 1));
	}
	return imm & ((1 << bits) - 1);
}

bool arm_condition_passed(u32 cond, vmm_user_regs_t * regs)
{
	bool ret = FALSE;
	cond &= 0xF;
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
		ret = (regs->cpsr & CPSR_NEGATIVE_MASK) ? TRUE : FALSE;
		if (ret) {
			ret = (regs->cpsr & CPSR_OVERFLOW_MASK) ? 
								TRUE : FALSE;
		} else {
			ret = (regs->cpsr & CPSR_OVERFLOW_MASK) ? 
								FALSE : TRUE;
		}
		break;
	case 6:
		ret = (regs->cpsr & CPSR_NEGATIVE_MASK) ? TRUE : FALSE;
		if (ret) {
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

void arm_decode_imm_shift(u32 type, u32 imm5, u32 *shift_t, u32 *shift_n)
{
	switch (type) {
	case 0:
		*shift_t = arm_shift_lsl;
		*shift_n = imm5;
		break;
	case 1:
		*shift_t = arm_shift_lsr;
		*shift_n = (imm5) ? imm5 : 32;
		break;
	case 2:
		*shift_t = arm_shift_asr;
		*shift_n = (imm5) ? imm5 : 32;
		break;
	case 3:
		if (imm5) {
			*shift_t = arm_shift_ror;
			*shift_n = imm5;
		} else {
			*shift_t = arm_shift_rrx;
			*shift_n = 1;
		}
		break;
	default:
		break;
	};
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

u32 arm_shift(u32 val, u32 shift_t, u32 shift_n, u32 cin)
{
	return arm_shift_c(val, shift_t, shift_n, cin, NULL);
}

u32 arm_expand_imm_c(u32 imm12, u32 cin, u32 *cout)
{
	return arm_shift_c((imm12 & 0xFF), 
			   arm_shift_ror, 
			   2 * (imm12 >> 8), 
			   cin, cout);
}

u32 arm_expand_imm(vmm_user_regs_t * regs, u32 imm12)
{
	return arm_expand_imm_c(imm12, 
				(regs->cpsr >> CPSR_CARRY_SHIFT) & 0x1,
				NULL);
}

u32 arm_add_with_carry(u32 x, u32 y, u32 cin, u32 *cout, u32 *oout)
{
	s32 sresult;
	u32 carry, overflow, uresult = 0x0;
	uresult = x + y + cin;
	sresult = (s32)x + (s32)y + (s32)cin;
	if ((uresult < x) || (uresult < y)) {
		carry = 1;
	} else {
		carry = 0;
	}
	if ((s32)uresult == sresult) {
		overflow = 0;
	} else {
		overflow = 1;
	}
	if (cout) {
		*cout = carry;
	}
	if (oout) {
		*oout = overflow;
	}
	return uresult;
}

/** Emulate 'cps' hypercall */
int arm_hypercall_cps(u32 id, u32 subid, u32 inst,
		       vmm_user_regs_t * regs, vmm_vcpu_t * vcpu)
{
	u32 cpsr, imod, M, A, I, F, mode;
	imod = ARM_INST_BITS(inst,
			     ARM_HYPERCALL_CPS_IMOD_END,
			     ARM_HYPERCALL_CPS_IMOD_START);
	M = ARM_INST_BITS(inst,
			  ARM_HYPERCALL_CPS_M_END, ARM_HYPERCALL_CPS_M_START);
	A = ARM_INST_BITS(inst,
			  ARM_HYPERCALL_CPS_A_END, ARM_HYPERCALL_CPS_A_START);
	I = ARM_INST_BITS(inst,
			  ARM_HYPERCALL_CPS_I_END, ARM_HYPERCALL_CPS_I_START);
	F = ARM_INST_BITS(inst,
			  ARM_HYPERCALL_CPS_F_END, ARM_HYPERCALL_CPS_F_START);
	mode = ARM_INST_BITS(inst,
			     ARM_HYPERCALL_CPS_MODE_END,
			     ARM_HYPERCALL_CPS_MODE_START);
	cpsr = cpu_vcpu_cpsr_retrive(vcpu, regs);
	if (M) {
		cpsr &= ~CPSR_MODE_MASK;
		cpsr |= mode;
	}
	if (A) {
		if (imod == 0x2) {
			cpsr &= ~CPSR_ASYNC_ABORT_DISABLED;
		} else if (imod == 0x3) {
			cpsr |= CPSR_ASYNC_ABORT_DISABLED;
		}
	}
	if (I) {
		if (imod == 0x2) {
			cpsr &= ~CPSR_IRQ_DISABLED;
		} else if (imod == 0x3) {
			cpsr |= CPSR_IRQ_DISABLED;
		}
	}
	if (F) {
		if (imod == 0x2) {
			cpsr &= ~CPSR_FIQ_DISABLED;
		} else if (imod == 0x3) {
			cpsr |= CPSR_FIQ_DISABLED;
		}
	}
	cpu_vcpu_cpsr_update(vcpu, regs, cpsr);
	regs->pc += 4;
	return VMM_OK;
}

/** Emulate 'mrs' hypercall */
int arm_hypercall_mrs(u32 id, u32 subid, u32 inst,
		       vmm_user_regs_t * regs, vmm_vcpu_t * vcpu)
{
	u32 cond, Rd, R, psr;
	cond = ARM_INST_DECODE(inst, ARM_INST_COND_MASK, ARM_INST_COND_SHIFT);
	Rd = ARM_INST_BITS(inst,
			   ARM_HYPERCALL_MRS_RD_END,
			   ARM_HYPERCALL_MRS_RD_START);
	R = ARM_INST_BITS(inst,
			  ARM_HYPERCALL_MRS_R_END, ARM_HYPERCALL_MRS_R_START);
	if (arm_condition_passed(cond, regs)) {
		if (R) {
			psr = cpu_vcpu_spsr_retrive(vcpu);
		} else {
			psr = cpu_vcpu_cpsr_retrive(vcpu, regs);
		}
		if (Rd < 15) {
			cpu_vcpu_reg_write(vcpu, regs, Rd, psr);
		} else {
			arm_unpredictable(regs, vcpu);
			return VMM_EFAIL;
		}
	}
	regs->pc += 4;
	return VMM_OK;
}

/** Emulate 'msr_i' hypercall */
int arm_hypercall_msr_i(u32 id, u32 subid, u32 inst,
			 vmm_user_regs_t * regs, vmm_vcpu_t * vcpu)
{
	u32 cond, mask, imm12, R, psr, tmask;
	cond = ARM_INST_DECODE(inst, ARM_INST_COND_MASK, ARM_INST_COND_SHIFT);
	mask = ARM_INST_BITS(inst,
			     ARM_HYPERCALL_MSR_I_MASK_END,
			     ARM_HYPERCALL_MSR_I_MASK_START);
	imm12 = ARM_INST_BITS(inst,
			      ARM_HYPERCALL_MSR_I_IMM12_END,
			      ARM_HYPERCALL_MSR_I_IMM12_START);
	R = ARM_INST_BITS(inst,
			  ARM_HYPERCALL_MSR_I_R_END,
			  ARM_HYPERCALL_MSR_I_R_START);
	if (arm_condition_passed(cond, regs)) {
		psr = arm_expand_imm(regs, imm12);
		if (!mask) {
			arm_unpredictable(regs, vcpu);
			return VMM_EFAIL;
		}
		tmask = 0x0;
		tmask |= (mask & 0x1) ? 0xFF : 0x00;
		tmask |= (mask & 0x2) ? 0xFF00 : 0x00;
		tmask |= (mask & 0x4) ? 0xFF0000 : 0x00;
		tmask |= (mask & 0x8) ? 0xFF000000 : 0x00;
		psr &= tmask;
		if (R) {
			psr |= (~tmask & cpu_vcpu_spsr_retrive(vcpu));
			cpu_vcpu_spsr_update(vcpu, psr);
		} else {
			psr |= (~tmask & cpu_vcpu_cpsr_retrive(vcpu, regs));
			cpu_vcpu_cpsr_update(vcpu, regs, psr);
		}
	}
	regs->pc += 4;
	return VMM_OK;
}

/** Emulate 'msr_r' hypercall */
int arm_hypercall_msr_r(u32 id, u32 subid, u32 inst,
			 vmm_user_regs_t * regs, vmm_vcpu_t * vcpu)
{
	u32 cond, mask, Rn, R, psr, tmask;
	cond = ARM_INST_DECODE(inst, ARM_INST_COND_MASK, ARM_INST_COND_SHIFT);
	mask = ARM_INST_BITS(inst,
			     ARM_HYPERCALL_MSR_R_MASK_END,
			     ARM_HYPERCALL_MSR_R_MASK_START);
	Rn = ARM_INST_BITS(inst,
			   ARM_HYPERCALL_MSR_R_RN_END,
			   ARM_HYPERCALL_MSR_R_RN_START);
	R = ARM_INST_BITS(inst,
			  ARM_HYPERCALL_MSR_R_R_END,
			  ARM_HYPERCALL_MSR_R_R_START);
	if (arm_condition_passed(cond, regs)) {
		if (Rn < 15) {
			psr = cpu_vcpu_reg_read(vcpu, regs, Rn);
		} else {
			arm_unpredictable(regs, vcpu);
			return VMM_EFAIL;
		}
		if (!mask) {
			arm_unpredictable(regs, vcpu);
			return VMM_EFAIL;
		}
		tmask = 0x0;
		tmask |= (mask & 0x1) ? 0xFF : 0x00;
		tmask |= (mask & 0x2) ? 0xFF00 : 0x00;
		tmask |= (mask & 0x4) ? 0xFF0000 : 0x00;
		tmask |= (mask & 0x8) ? 0xFF000000 : 0x00;
		psr &= tmask;
		if (R) {
			psr |= (~tmask & cpu_vcpu_spsr_retrive(vcpu));
			cpu_vcpu_spsr_update(vcpu, psr);
		} else {
			psr |= (~tmask & cpu_vcpu_cpsr_retrive(vcpu, regs));
			cpu_vcpu_cpsr_update(vcpu, regs, psr);
		}
	}
	regs->pc += 4;
	return VMM_OK;
}

/** Emulate 'rfe' hypercall */
int arm_hypercall_rfe(u32 id, u32 subid, u32 inst,
			 vmm_user_regs_t * regs, vmm_vcpu_t * vcpu)
{
	int rc;
	u32 cond, Rn, P, U, W;
	u32 cpsr, address, data;
	bool wback, increment, wordhigher;
	cond = ARM_INST_DECODE(inst, ARM_INST_COND_MASK, ARM_INST_COND_SHIFT);
	P = ARM_INST_BITS(inst,
			  ARM_HYPERCALL_RFE_P_END,
			  ARM_HYPERCALL_RFE_P_START);
	U = ARM_INST_BITS(inst,
			  ARM_HYPERCALL_RFE_U_END,
			  ARM_HYPERCALL_RFE_U_START);
	W = ARM_INST_BITS(inst,
			  ARM_HYPERCALL_RFE_W_END,
			  ARM_HYPERCALL_RFE_W_START);
	Rn = ARM_INST_BITS(inst,
			   ARM_HYPERCALL_RFE_RN_END,
			   ARM_HYPERCALL_RFE_RN_START);
	wback = (W == 1) ? TRUE : FALSE;
	increment = (U == 1) ? TRUE : FALSE;
	wordhigher = (P == U) ? TRUE : FALSE;
	if (Rn == 15) {
		arm_unpredictable(regs, vcpu);
		return VMM_EFAIL;
	}
	if (arm_condition_passed(cond, regs)) {
		cpsr = cpu_vcpu_cpsr_retrive(vcpu, regs);
		cpsr &= CPSR_MODE_MASK;
		if (cpsr == CPSR_MODE_USER) {
			arm_unpredictable(regs, vcpu);
			return VMM_EFAIL;
		}
		address = cpu_vcpu_reg_read(vcpu, regs, Rn);
		address = (increment) ? address : (address - 8);
		address = (wordhigher) ? (address + 4) : address;
		data = 0x0;
		if ((rc = cpu_vcpu_cp15_mem_read(vcpu, regs, address + 4, 
						 &data, 4, FALSE))) {
			return rc;
		}
		cpu_vcpu_cpsr_update(vcpu, regs, data);
		data = 0x0;
		if ((rc = cpu_vcpu_cp15_mem_read(vcpu, regs, address, 
						 &data, 4, FALSE))) {
			return rc;
		}
		regs->pc = data;
		if (wback) {
			address = cpu_vcpu_reg_read(vcpu, regs, Rn);
			address = (increment) ? (address + 8) : (address - 8);
			cpu_vcpu_reg_write(vcpu, regs, Rn, address);
		}
		/* Steps unique to exception return */
		vmm_vcpu_irq_deassert(vcpu);
	} else {
		regs->pc += 4;
	}
	return VMM_OK;
}

/** Emulate 'srs' hypercall */
int arm_hypercall_srs(u32 id, u32 subid, u32 inst,
			 vmm_user_regs_t * regs, vmm_vcpu_t * vcpu)
{
	int rc;
	u32 cond, P, U, W, mode;
	u32 cpsr, base, address, data;
	bool wback, increment, wordhigher;
	cond = ARM_INST_DECODE(inst, ARM_INST_COND_MASK, ARM_INST_COND_SHIFT);
	P = ARM_INST_BITS(inst,
			  ARM_HYPERCALL_SRS_P_END,
			  ARM_HYPERCALL_SRS_P_START);
	U = ARM_INST_BITS(inst,
			  ARM_HYPERCALL_SRS_U_END,
			  ARM_HYPERCALL_SRS_U_START);
	W = ARM_INST_BITS(inst,
			  ARM_HYPERCALL_SRS_W_END,
			  ARM_HYPERCALL_SRS_W_START);
	mode = ARM_INST_BITS(inst,
			     ARM_HYPERCALL_SRS_MODE_END,
			     ARM_HYPERCALL_SRS_MODE_START);
	wback = (W == 1) ? TRUE : FALSE;
	increment = (U == 1) ? TRUE : FALSE;
	wordhigher = (P == U) ? TRUE : FALSE;
	if (arm_condition_passed(cond, regs)) {
		cpsr = cpu_vcpu_cpsr_retrive(vcpu, regs);
		cpsr &= CPSR_MODE_MASK;
		if ((cpsr == CPSR_MODE_USER) ||
		    (cpsr == CPSR_MODE_SYSTEM)) {
			arm_unpredictable(regs, vcpu);
			return VMM_EFAIL;
		}
		base = cpu_vcpu_regmode_read(vcpu, regs, mode, 13);
		address = (increment) ? base : (base - 8);
		address = (wordhigher) ? (address + 4) : address;
		data = regs->lr;
		if ((rc = cpu_vcpu_cp15_mem_write(vcpu, regs, address, 
						  &data, 4, FALSE))) {
			return rc;
		}
		address += 4;
		data = cpu_vcpu_spsr_retrive(vcpu);
		if ((rc = cpu_vcpu_cp15_mem_write(vcpu, regs, address, 
						  &data, 4, FALSE))) {
			return rc;
		}
		if (wback) {
			address = (increment) ? (base + 8) : (base - 8);
			cpu_vcpu_regmode_write(vcpu, regs, mode, 13, address);
		}
	}
	return VMM_OK;
}

/** Emulate 'ldm_ue' hypercall */
int arm_hypercall_ldm_ue(u32 id, u32 inst,
			 vmm_user_regs_t * regs, vmm_vcpu_t * vcpu)
{
	int rc;
	u32 cond, Rn, U, P, W, reg_list;
	u32 cpsr, address, i, mask, length, data;
	bool wback, increment, wordhigher;
	cond = ARM_INST_DECODE(inst, ARM_INST_COND_MASK, ARM_INST_COND_SHIFT);
	Rn = ARM_INST_BITS(inst,
			   ARM_HYPERCALL_LDM_UE_RN_END,
			   ARM_HYPERCALL_LDM_UE_RN_START);
	P = ((id - ARM_HYPERCALL_LDM_UE_ID0) & 0x4) >> 2;
	U = ((id - ARM_HYPERCALL_LDM_UE_ID0) & 0x2) >> 1;
	W = ((id - ARM_HYPERCALL_LDM_UE_ID0) & 0x1);
	reg_list = ARM_INST_BITS(inst,
				 ARM_HYPERCALL_LDM_UE_REGLIST_END,
				 ARM_HYPERCALL_LDM_UE_REGLIST_START);
	increment = (U == 1) ? TRUE : FALSE;
	wordhigher = (P == U) ? TRUE : FALSE;
	if (Rn == 15) {
		arm_unpredictable(regs, vcpu);
		return VMM_EFAIL;
	}
	if (reg_list & 0x8000) { 
		/* LDM (Exception Return) */
		wback = (W == 1) ? TRUE : FALSE;
		if (wback && (reg_list & (0x1 << Rn))) {
			arm_unpredictable(regs, vcpu);
			return VMM_EFAIL;
		}
		if (arm_condition_passed(cond, regs)) {
			cpsr = cpu_vcpu_cpsr_retrive(vcpu, regs);
			cpsr &= CPSR_MODE_MASK;
			if ((cpsr == CPSR_MODE_USER) ||
			    (cpsr == CPSR_MODE_SYSTEM)) {
				arm_unpredictable(regs, vcpu);
				return VMM_EFAIL;
			}
			mask = 0x1;
			length = 4;
			for (i = 0; i < 15; i++) {
				if (reg_list & mask) {
					length += 4;
				}
				mask = mask << 1;
			}
			address = cpu_vcpu_reg_read(vcpu, regs, Rn);
			address = (increment) ? address : address - length;
			address = (wordhigher) ? (address + 4) : address;
			mask = 0x1;
			for (i = 0; i < 15; i++) {
				if (reg_list & mask) {
					data = 0x0;
					if ((rc = cpu_vcpu_cp15_mem_read(vcpu, 
					    regs, address, &data, 4, FALSE))) {
						return rc;
					}
					cpu_vcpu_reg_write(vcpu, regs, i, data);
					address += 4;
				}
				mask = mask << 1;
			}
			data = 0x0;
			if ((rc = cpu_vcpu_cp15_mem_read(vcpu, regs, address, 
							 &data, 4, FALSE))) {
				return rc;
			}
			if (wback && !(reg_list & (0x1 << Rn))) {
				address = cpu_vcpu_reg_read(vcpu, regs, Rn);
				address = (increment) ? address + length : 
							address - length;
				cpu_vcpu_reg_write(vcpu, regs, Rn, address);
			}
			cpsr = cpu_vcpu_spsr_retrive(vcpu);
			cpu_vcpu_cpsr_update(vcpu, regs, cpsr);
			regs->pc = data;
			/* Steps unique to exception return */
			vmm_vcpu_irq_deassert(vcpu);
		} else {
			regs->pc += 4;
		}
	} else {
		/* LDM (User Registers) */
		if ((W == 1) || !reg_list) {
			arm_unpredictable(regs, vcpu);
			return VMM_EFAIL;
		}
		if (arm_condition_passed(cond, regs)) {
			cpsr = cpu_vcpu_cpsr_retrive(vcpu, regs);
			cpsr &= CPSR_MODE_MASK;
			if ((cpsr == CPSR_MODE_USER) ||
			    (cpsr == CPSR_MODE_SYSTEM)) {
				arm_unpredictable(regs, vcpu);
				return VMM_EFAIL;
			}
			mask = 0x1;
			length = 0;
			for (i = 0; i < 15; i++) {
				if (reg_list & mask) {
					length += 4;
				}
				mask = mask << 1;
			}
			address = cpu_vcpu_reg_read(vcpu, regs, Rn);
			address = (increment) ? address : address - length;
			address = (wordhigher) ? (address + 4) : address;
			mask = 0x1;
			for (i = 0; i < 15; i++) {
				if (reg_list & mask) {
					data = 0x0;
					if ((rc = cpu_vcpu_cp15_mem_read(vcpu, 
					    regs, address, &data, 4, FALSE))) {
						return rc;
					}
					cpu_vcpu_regmode_write(vcpu, regs, 
								CPSR_MODE_USER,
								i, data);
					address += 4;
				}
				mask = mask << 1;
			}
		}
		regs->pc += 4;
	}
	return VMM_OK;
}

/** Emulate 'stm_u' hypercall */
int arm_hypercall_stm_u(u32 id, u32 inst,
			 vmm_user_regs_t * regs, vmm_vcpu_t * vcpu)
{
	int rc;
	u32 cond, Rn, P, U, reg_list;
	u32 i, cpsr, mask, length, address, data;
	bool increment, wordhigher;
	cond = ARM_INST_DECODE(inst, ARM_INST_COND_MASK, ARM_INST_COND_SHIFT);
	Rn = ARM_INST_BITS(inst,
			   ARM_HYPERCALL_STM_U_RN_END,
			   ARM_HYPERCALL_STM_U_RN_START);
	P = ((id - ARM_HYPERCALL_STM_U_ID0) & 0x2) >> 1;
	U = ((id - ARM_HYPERCALL_STM_U_ID0) & 0x1);
	reg_list = ARM_INST_BITS(inst,
				 ARM_HYPERCALL_STM_U_REGLIST_END,
				 ARM_HYPERCALL_STM_U_REGLIST_START);
	increment = (U == 1) ? TRUE : FALSE;
	wordhigher = (P == U) ? TRUE : FALSE;
	if ((Rn == 15) || !reg_list) {
		arm_unpredictable(regs, vcpu);
		return VMM_EFAIL;
	}
	if (arm_condition_passed(cond, regs)) {
		cpsr = cpu_vcpu_cpsr_retrive(vcpu, regs);
		cpsr &= CPSR_MODE_MASK;
		if ((cpsr == CPSR_MODE_USER) ||
		    (cpsr == CPSR_MODE_SYSTEM)) {
			arm_unpredictable(regs, vcpu);
			return VMM_EFAIL;
		}
		mask = 0x1;
		length = 0;
		for (i = 0; i < 16; i++) {
			if (reg_list & mask) {
				length += 4;
			}
			mask = mask << 1;
		}
		address = cpu_vcpu_reg_read(vcpu, regs, Rn);
		address = (increment) ? address : address - length;
		address = (wordhigher) ? address + 4 : address;
		mask = 0x1;
		for (i = 0; i < 16; i++) {
			if (reg_list & mask) {
				data = cpu_vcpu_regmode_read(vcpu, regs, 
							CPSR_MODE_USER, i);
				if ((rc = cpu_vcpu_cp15_mem_write(vcpu, regs, 
						address, &data, 4, FALSE))) {
					return rc;
				}
				address += 4;
			}
			mask = mask << 1;
		}
	}
	regs->pc += 4;
	return VMM_OK;
}

/** Emulate 'subs_rel' hypercall */
int arm_hypercall_subs_rel(u32 id, u32 inst,
			 vmm_user_regs_t * regs, vmm_vcpu_t * vcpu)
{
	u32 cond, opcode, Rn, imm12, imm5, type, Rm;
	bool register_form, shift_t, shift_n, operand2, result, spsr;
	cond = ARM_INST_DECODE(inst, ARM_INST_COND_MASK, ARM_INST_COND_SHIFT);
	opcode = ARM_INST_BITS(inst,
				ARM_HYPERCALL_SUBS_REL_OPCODE_END,
				ARM_HYPERCALL_SUBS_REL_OPCODE_START);
	Rn = ARM_INST_BITS(inst,
			   ARM_HYPERCALL_SUBS_REL_RN_END,
			   ARM_HYPERCALL_SUBS_REL_RN_START);
	imm12 = ARM_INST_BITS(inst,
			      ARM_HYPERCALL_SUBS_REL_IMM12_END,
			      ARM_HYPERCALL_SUBS_REL_IMM12_START);
	imm5 = ARM_INST_BITS(inst,
			     ARM_HYPERCALL_SUBS_REL_IMM5_END,
			     ARM_HYPERCALL_SUBS_REL_IMM5_START);
	type = ARM_INST_BITS(inst,
			     ARM_HYPERCALL_SUBS_REL_TYPE_END,
			     ARM_HYPERCALL_SUBS_REL_TYPE_START);
	Rm = ARM_INST_BITS(inst,
			   ARM_HYPERCALL_SUBS_REL_RM_END,
			   ARM_HYPERCALL_SUBS_REL_RM_START);
	register_form = (id == ARM_HYPERCALL_SUBS_REL_ID0) ? TRUE : FALSE;
	if (arm_condition_passed(cond, regs)) {
		if (register_form) {
			arm_decode_imm_shift(type, imm5, &shift_t, &shift_n);
			operand2 = cpu_vcpu_reg_read(vcpu, regs, Rm);
			operand2 = arm_shift(operand2, shift_t, shift_n, 
					 (regs->cpsr & CPSR_CARRY_MASK) >>
					 CPSR_CARRY_SHIFT);
		} else {
			operand2 = arm_expand_imm(regs, imm12);
		}
		result = cpu_vcpu_reg_read(vcpu, regs, Rn);
		switch (opcode) {
		case 0x0: /* AND */
			result = result & operand2;
			break;
		case 0x1: /* EOR */
			result = result ^ operand2;
			break;
		case 0x2: /* SUB */
			result = arm_add_with_carry(result, ~operand2, 
								1, NULL, NULL);
			break;
		case 0x3: /* RSB */
			result = arm_add_with_carry(~result, operand2, 
								1, NULL, NULL);
			break;
		case 0x4: /* ADD */
			result = arm_add_with_carry(result, operand2, 
								0, NULL, NULL);
			break;
		case 0x5: /* ADC */
			if (regs->cpsr & CPSR_CARRY_MASK) {
				result = arm_add_with_carry(result, operand2, 
								1, NULL, NULL);
			} else {
				result = arm_add_with_carry(result, operand2, 
								0, NULL, NULL);
			}
			break;
		case 0x6: /* SBC */
			if (regs->cpsr & CPSR_CARRY_MASK) {
				result = arm_add_with_carry(result, ~operand2, 
								1, NULL, NULL);
			} else {
				result = arm_add_with_carry(result, ~operand2, 
								0, NULL, NULL);
			}
			break;
		case 0x7: /* RSC */
			if (regs->cpsr & CPSR_CARRY_MASK) {
				result = arm_add_with_carry(~result, operand2, 
								1, NULL, NULL);
			} else {
				result = arm_add_with_carry(~result, operand2, 
								0, NULL, NULL);
			}
			break;
		case 0xC: /* ORR*/
			result = result | operand2;
			break;
		case 0xD: /* MOV */
			result = operand2;
			break;
		case 0xE: /* BIC */
			result = result & ~operand2;
			break;
		case 0xF: /* MVN */
			result = ~operand2;
			break;
		default:
			arm_unpredictable(regs, vcpu);
			return VMM_EFAIL;
			break;
		};
		spsr = cpu_vcpu_spsr_retrive(vcpu);
		cpu_vcpu_cpsr_update(vcpu, regs, spsr);
		regs->pc = result;
		/* Steps unique to exception return */
		vmm_vcpu_irq_deassert(vcpu);
	} else {
		regs->pc += 4;
	}
	return VMM_OK;
}

/** Emulate hypercall instruction */
int arm_instgrp_hypercall(u32 inst, vmm_user_regs_t * regs, vmm_vcpu_t * vcpu)
{
	u32 id, subid;
	id = ARM_INST_DECODE(inst,
			     ARM_INST_HYPERCALL_ID_MASK,
			     ARM_INST_HYPERCALL_ID_SHIFT);
	subid = ARM_INST_DECODE(inst,
				ARM_INST_HYPERCALL_SUBID_MASK,
				ARM_INST_HYPERCALL_SUBID_SHIFT);
	switch (id) {
	case ARM_HYPERCALL_CPS_ID:
		switch (subid) {
		case ARM_HYPERCALL_CPS_SUBID:
			return arm_hypercall_cps(id, subid, inst, regs, vcpu);
			break;
		case ARM_HYPERCALL_MRS_SUBID:
			return arm_hypercall_mrs(id, subid, inst, regs, vcpu);
			break;
		case ARM_HYPERCALL_MSR_I_SUBID:
			return arm_hypercall_msr_i(id, subid, inst, regs, vcpu);
			break;
		case ARM_HYPERCALL_MSR_R_SUBID:
			return arm_hypercall_msr_r(id, subid, inst, regs, vcpu);
			break;
		case ARM_HYPERCALL_RFE_SUBID:
			return arm_hypercall_rfe(id, subid, inst, regs, vcpu);
			break;
		case ARM_HYPERCALL_SRS_SUBID:
			return arm_hypercall_srs(id, subid, inst, regs, vcpu);
			break;
		default:
			break;
		};
		break;
	case ARM_HYPERCALL_LDM_UE_ID0:
	case ARM_HYPERCALL_LDM_UE_ID1:
	case ARM_HYPERCALL_LDM_UE_ID2:
	case ARM_HYPERCALL_LDM_UE_ID3:
	case ARM_HYPERCALL_LDM_UE_ID4:
	case ARM_HYPERCALL_LDM_UE_ID5:
	case ARM_HYPERCALL_LDM_UE_ID6:
	case ARM_HYPERCALL_LDM_UE_ID7:
			return arm_hypercall_ldm_ue(id, inst, regs, vcpu);
			break;
	case ARM_HYPERCALL_STM_U_ID0:
	case ARM_HYPERCALL_STM_U_ID1:
	case ARM_HYPERCALL_STM_U_ID2:
	case ARM_HYPERCALL_STM_U_ID3:
		return arm_hypercall_stm_u(id, inst, regs, vcpu);
		break;
	case ARM_HYPERCALL_SUBS_REL_ID0:
	case ARM_HYPERCALL_SUBS_REL_ID1:
		return arm_hypercall_subs_rel(id, inst, regs, vcpu);
		break;
	default:
		break;
	};
	return VMM_EFAIL;
}

/** Emulate 'ldrh (immediate)' instruction */
int arm_inst_ldrh_i(u32 inst, vmm_user_regs_t * regs, vmm_vcpu_t * vcpu)
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
		arm_unpredictable(regs, vcpu);
		return VMM_EFAIL;
	}
	imm32 = arm_zero_extend((imm4H << 4) | imm4L, 32);
	index = (P == 1) ? TRUE : FALSE;
	add = (U == 1) ? TRUE : FALSE;
	wback = ((P == 0) || (W == 1)) ? TRUE : FALSE;
	if ((Rt == 15) || (wback && (Rn == Rt))) {
		arm_unpredictable(regs, vcpu);
		return VMM_EFAIL;
	}
	if (arm_condition_passed(cond, regs)) {
		address = cpu_vcpu_reg_read(vcpu, regs, Rn);
		offset_addr = (add) ? (address + imm32) : 
					(address - imm32);
		address = (index) ? offset_addr : address;
		data = 0x0;
		if ((rc = cpu_vcpu_cp15_mem_read(vcpu, regs, address, 
						 &data, 2, FALSE))) {
			return rc;
		}
		if (wback) {
			cpu_vcpu_reg_write(vcpu, regs, Rn, offset_addr);
		}
		cpu_vcpu_reg_write(vcpu, regs, Rt, arm_zero_extend(data, 32));
	}
	regs->pc += 4;
	return VMM_OK;
}

/** Emulate 'ldrh (literal)' instruction */
int arm_inst_ldrh_l(u32 inst, vmm_user_regs_t * regs, vmm_vcpu_t * vcpu)
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
		arm_unpredictable(regs, vcpu);
		return VMM_EFAIL;
	}
	if (arm_condition_passed(cond, regs)) {
		address = arm_align(regs->pc, 4);
		address = (add) ? (address + imm32) : (address - imm32);
		data = 0x0;
		if ((rc = cpu_vcpu_cp15_mem_read(vcpu, regs, address, 
						 &data, 2, FALSE))) {
			return rc;
		}
		cpu_vcpu_reg_write(vcpu, regs, Rt, arm_zero_extend(data, 32));
	}
	regs->pc += 4;
	return VMM_OK;
}

/** Emulate 'ldrh (register)' instruction */
int arm_inst_ldrh_r(u32 inst, vmm_user_regs_t * regs, vmm_vcpu_t * vcpu)
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
		arm_unpredictable(regs, vcpu);
		return VMM_EFAIL;
	}
	index = (P == 1) ? TRUE : FALSE;
	add = (U == 1) ? TRUE : FALSE;
	wback = ((P == 0) || (W == 1)) ? TRUE : FALSE;
	shift_t = arm_shift_lsl;
	shift_n = 0;
	if ((Rt == 15) || (Rm == 15)) {
		arm_unpredictable(regs, vcpu);
		return VMM_EFAIL;
	}
	if (wback && (Rn == 15 || Rn == Rt)) {
		arm_unpredictable(regs, vcpu);
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
						 &data, 2, FALSE))) {
			return rc;
		}
		if (wback) {
			cpu_vcpu_reg_write(vcpu, regs, Rn, offset_addr);
		}
		cpu_vcpu_reg_write(vcpu, regs, Rt, arm_zero_extend(data, 32));
	}
	regs->pc += 4;
	return VMM_OK;
}

/** Emulate 'ldrht' intruction */
int arm_inst_ldrht(u32 inst, vmm_user_regs_t * regs, vmm_vcpu_t * vcpu)
{
	int rc;
	u32 cond, U, Rn, Rt, Rm, imm4H, imm4L;
	u32 imm32, offset, offset_addr, address; u16 data;
	bool regform, postindex, add;
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
			arm_unpredictable(regs, vcpu);
			return VMM_EFAIL;
		}
		imm32 = 0;
	} else {
		if ((Rt == 15) || (Rn == 15) || (Rn == Rt)) {
			arm_unpredictable(regs, vcpu);
			return VMM_EFAIL;
		}
		imm32 = arm_zero_extend((imm4H << 4) | imm4L, 32);
	}
	postindex = TRUE;
	add = (U == 1) ? TRUE : FALSE;
	if (arm_condition_passed(cond, regs)) {
		offset = (regform) ? cpu_vcpu_reg_read(vcpu, regs, Rm) : imm32;
		address = cpu_vcpu_reg_read(vcpu, regs, Rn);
		offset_addr = (add) ? (address + offset) : (address - offset);
		address = (postindex) ? address : offset_addr;
		data = 0x0;
		if ((rc = cpu_vcpu_cp15_mem_read(vcpu, regs, address, 
						 &data, 2, TRUE))) {
			return rc;
		}
		if (postindex) {
			cpu_vcpu_reg_write(vcpu, regs, Rn, offset_addr);
		}
		cpu_vcpu_reg_write(vcpu, regs, Rt, arm_zero_extend(data, 32));
	}
	regs->pc += 4;
	return VMM_OK;
}

/** Emulate 'strh (immediate)' instruction */
int arm_inst_strh_i(u32 inst, vmm_user_regs_t * regs, vmm_vcpu_t * vcpu)
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
		arm_unpredictable(regs, vcpu);
		return VMM_EFAIL;
	}
	imm32 = arm_zero_extend((imm4H << 4) | imm4L, 32);
	index = (P == 1) ? TRUE : FALSE;
	add = (U == 1) ? TRUE : FALSE;
	wback = ((P == 0) || (W == 1)) ? TRUE : FALSE;
	if ((Rt == 15) || (wback && ((Rn == 15) || (Rn == Rt)))) {
		arm_unpredictable(regs, vcpu);
		return VMM_EFAIL;
	}
	if (arm_condition_passed(cond, regs)) {
		address = cpu_vcpu_reg_read(vcpu, regs, Rn);
		offset_addr = (add) ? (address + imm32) : (address - imm32);
		address = (index) ? offset_addr : address;
		data = cpu_vcpu_reg_read(vcpu, regs, Rt) & 0xFFFF;
		if ((rc = cpu_vcpu_cp15_mem_write(vcpu, regs, address, 
						  &data, 2, FALSE))) {
			return rc;
		}
		if (wback) {
			cpu_vcpu_reg_write(vcpu, regs, Rn, offset_addr);
		}
	}
	regs->pc += 4;
	return VMM_OK;
}

/** Emulate 'strh (register)' instruction */
int arm_inst_strh_r(u32 inst, vmm_user_regs_t * regs, vmm_vcpu_t * vcpu)
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
		arm_unpredictable(regs, vcpu);
		return VMM_EFAIL;
	}
	index = (P == 1) ? TRUE : FALSE;
	add = (U == 1) ? TRUE : FALSE;
	wback = ((P == 0) || (W == 1)) ? TRUE : FALSE;
	shift_t = arm_shift_lsl;
	shift_n = 0;
	if ((Rt == 15) || (Rm == 15)) {
		arm_unpredictable(regs, vcpu);
		return VMM_EFAIL;
	}
	if (wback && (Rn == 15 || Rn == Rt)) {
		arm_unpredictable(regs, vcpu);
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
		data = cpu_vcpu_reg_read(vcpu, regs, Rt) & 0xFFFF;
		if ((rc = cpu_vcpu_cp15_mem_write(vcpu, regs, address, 
						  &data, 2, FALSE))) {
			return rc;
		}
		if (wback) {
			cpu_vcpu_reg_write(vcpu, regs, Rn, offset_addr);
		}
	}
	regs->pc += 4;
	return VMM_OK;
}

/** Emulate 'strht' intruction */
int arm_inst_strht(u32 inst, vmm_user_regs_t * regs, vmm_vcpu_t * vcpu)
{
	int rc;
	u32 cond, U, Rn, Rt, Rm, imm4H, imm4L;
	u32 imm32, offset, offset_addr, address; u16 data;
	bool regform, postindex, add;
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
			arm_unpredictable(regs, vcpu);
			return VMM_EFAIL;
		}
		imm32 = 0;
	} else {
		if ((Rt == 15) || (Rn == 15) || (Rn == Rt)) {
			arm_unpredictable(regs, vcpu);
			return VMM_EFAIL;
		}
		imm32 = arm_zero_extend((imm4H << 4) | imm4L, 32);
	}
	postindex = TRUE;
	add = (U == 1) ? TRUE : FALSE;
	if (arm_condition_passed(cond, regs)) {
		offset = (regform) ? cpu_vcpu_reg_read(vcpu, regs, Rm) : imm32;
		address = cpu_vcpu_reg_read(vcpu, regs, Rn);
		offset_addr = (add) ? (address + offset) : (address - offset);
		address = (postindex) ? address : offset_addr;
		data = cpu_vcpu_reg_read(vcpu, regs, Rt) & 0xFFFF;
		if ((rc = cpu_vcpu_cp15_mem_write(vcpu, regs, address, 
						  &data, 2, TRUE))) {
			return rc;
		}
		if (postindex) {
			cpu_vcpu_reg_write(vcpu, regs, Rn, offset_addr);
		}
	}
	regs->pc += 4;
	return VMM_OK;
}

/** Emulate 'ldrsh (immediate)' instruction */
int arm_inst_ldrsh_i(u32 inst, vmm_user_regs_t * regs, vmm_vcpu_t * vcpu)
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
		arm_unpredictable(regs, vcpu);
		return VMM_EFAIL;
	}
	imm32 = arm_zero_extend((imm4H << 4) | imm4L, 32);
	index = (P == 1) ? TRUE : FALSE;
	add = (U == 1) ? TRUE : FALSE;
	wback = ((P == 0) || (W == 1)) ? TRUE : FALSE;
	if ((Rt == 15) || (wback && (Rn == Rt))) {
		arm_unpredictable(regs, vcpu);
		return VMM_EFAIL;
	}
	if (arm_condition_passed(cond, regs)) {
		address = cpu_vcpu_reg_read(vcpu, regs, Rn);
		offset_addr = (add) ? (address + imm32) : 
					(address - imm32);
		address = (index) ? offset_addr : address;
		data = 0x0;
		if ((rc = cpu_vcpu_cp15_mem_read(vcpu, regs, address, 
						 &data, 2, FALSE))) {
			return rc;
		}
		if (wback) {
			cpu_vcpu_reg_write(vcpu, regs, Rn, offset_addr);
		}
		cpu_vcpu_reg_write(vcpu, regs, Rt, 
					arm_sign_extend(data, 16, 32));
	}
	regs->pc += 4;
	return VMM_OK;
}

/** Emulate 'ldrsh (literal)' instruction */
int arm_inst_ldrsh_l(u32 inst, vmm_user_regs_t * regs, vmm_vcpu_t * vcpu)
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
		arm_unpredictable(regs, vcpu);
		return VMM_EFAIL;
	}
	if (arm_condition_passed(cond, regs)) {
		address = arm_align(regs->pc, 4);
		address = (add) ? (address + imm32) : (address - imm32);
		data = 0x0;
		if ((rc = cpu_vcpu_cp15_mem_read(vcpu, regs, address, 
						 &data, 2, FALSE))) {
			return rc;
		}
		cpu_vcpu_reg_write(vcpu, regs, Rt, 
					arm_sign_extend(data, 16, 32));
	}
	regs->pc += 4;
	return VMM_OK;
}

/** Emulate 'ldrsh (register)' instruction */
int arm_inst_ldrsh_r(u32 inst, vmm_user_regs_t * regs, vmm_vcpu_t * vcpu)
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
		arm_unpredictable(regs, vcpu);
		return VMM_EFAIL;
	}
	index = (P == 1) ? TRUE : FALSE;
	add = (U == 1) ? TRUE : FALSE;
	wback = ((P == 0) || (W == 1)) ? TRUE : FALSE;
	shift_t = arm_shift_lsl;
	shift_n = 0;
	if ((Rt == 15) || (Rm == 15)) {
		arm_unpredictable(regs, vcpu);
		return VMM_EFAIL;
	}
	if (wback && (Rn == 15 || Rn == Rt)) {
		arm_unpredictable(regs, vcpu);
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
						 &data, 2, FALSE))) {
			return rc;
		}
		if (wback) {
			cpu_vcpu_reg_write(vcpu, regs, Rn, offset_addr);
		}
		cpu_vcpu_reg_write(vcpu, regs, Rt, 
					arm_sign_extend(data, 16, 32));
	}
	regs->pc += 4;
	return VMM_OK;
}

/** Emulate 'ldrsht' intruction */
int arm_inst_ldrsht(u32 inst, vmm_user_regs_t * regs, vmm_vcpu_t * vcpu)
{
	int rc;
	u32 cond, U, Rn, Rt, Rm, imm4H, imm4L;
	u32 imm32, offset, offset_addr, address; u16 data;
	bool regform, postindex, add;
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
			arm_unpredictable(regs, vcpu);
			return VMM_EFAIL;
		}
		imm32 = 0;
	} else {
		if ((Rt == 15) || (Rn == 15) || (Rn == Rt)) {
			arm_unpredictable(regs, vcpu);
			return VMM_EFAIL;
		}
		imm32 = arm_zero_extend((imm4H << 4) | imm4L, 32);
	}
	postindex = TRUE;
	add = (U == 1) ? TRUE : FALSE;
	if (arm_condition_passed(cond, regs)) {
		offset = (regform) ? cpu_vcpu_reg_read(vcpu, regs, Rm) : imm32;
		address = cpu_vcpu_reg_read(vcpu, regs, Rn);
		offset_addr = (add) ? (address + offset) : (address - offset);
		address = (postindex) ? address : offset_addr;
		data = 0x0;
		if ((rc = cpu_vcpu_cp15_mem_read(vcpu, regs, address, 
						 &data, 2, TRUE))) {
			return rc;
		}
		if (postindex) {
			cpu_vcpu_reg_write(vcpu, regs, Rn, offset_addr);
		}
		cpu_vcpu_reg_write(vcpu, regs, Rt, 
					arm_sign_extend(data, 16, 32));
	}
	regs->pc += 4;
	return VMM_OK;
}

/** Emulate 'ldrsb (immediate)' instruction */
int arm_inst_ldrsb_i(u32 inst, vmm_user_regs_t * regs, vmm_vcpu_t * vcpu)
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
		arm_unpredictable(regs, vcpu);
		return VMM_EFAIL;
	}
	imm32 = arm_zero_extend((imm4H << 4) | imm4L, 32);
	index = (P == 1) ? TRUE : FALSE;
	add = (U == 1) ? TRUE : FALSE;
	wback = ((P == 0) || (W == 1)) ? TRUE : FALSE;
	if ((Rt == 15) || (wback && (Rn == Rt))) {
		arm_unpredictable(regs, vcpu);
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
		if (wback) {
			cpu_vcpu_reg_write(vcpu, regs, Rn, offset_addr);
		}
		cpu_vcpu_reg_write(vcpu, regs, Rt, 
					arm_sign_extend(data, 8, 32));
	}
	regs->pc += 4;
	return VMM_OK;
}

/** Emulate 'ldrsb (literal)' instruction */
int arm_inst_ldrsb_l(u32 inst, vmm_user_regs_t * regs, vmm_vcpu_t * vcpu)
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
		arm_unpredictable(regs, vcpu);
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
		cpu_vcpu_reg_write(vcpu, regs, Rt, 
					arm_sign_extend(data, 8, 32));
	}
	regs->pc += 4;
	return VMM_OK;
}

/** Emulate 'ldrsb (register)' instruction */
int arm_inst_ldrsb_r(u32 inst, vmm_user_regs_t * regs, vmm_vcpu_t * vcpu)
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
		arm_unpredictable(regs, vcpu);
		return VMM_EFAIL;
	}
	index = (P == 1) ? TRUE : FALSE;
	add = (U == 1) ? TRUE : FALSE;
	wback = ((P == 0) || (W == 1)) ? TRUE : FALSE;
	shift_t = arm_shift_lsl;
	shift_n = 0;
	if ((Rt == 15) || (Rm == 15)) {
		arm_unpredictable(regs, vcpu);
		return VMM_EFAIL;
	}
	if (wback && (Rn == 15 || Rn == Rt)) {
		arm_unpredictable(regs, vcpu);
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
		if (wback) {
			cpu_vcpu_reg_write(vcpu, regs, Rn, offset_addr);
		}
		cpu_vcpu_reg_write(vcpu, regs, Rt, 
					arm_sign_extend(data, 8, 32));
	}
	regs->pc += 4;
	return VMM_OK;
}

/** Emulate 'ldrsbt' intruction */
int arm_inst_ldrsbt(u32 inst, vmm_user_regs_t * regs, vmm_vcpu_t * vcpu)
{
	int rc;
	u32 cond, U, Rn, Rt, Rm, imm4H, imm4L;
	u32 imm32, offset, offset_addr, address; u8 data;
	bool regform, postindex, add;
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
			arm_unpredictable(regs, vcpu);
			return VMM_EFAIL;
		}
		imm32 = 0;
	} else {
		if ((Rt == 15) || (Rn == 15) || (Rn == Rt)) {
			arm_unpredictable(regs, vcpu);
			return VMM_EFAIL;
		}
		imm32 = arm_zero_extend((imm4H << 4) | imm4L, 32);
	}
	postindex = TRUE;
	add = (U == 1) ? TRUE : FALSE;
	if (arm_condition_passed(cond, regs)) {
		offset = (regform) ? cpu_vcpu_reg_read(vcpu, regs, Rm) : imm32;
		address = cpu_vcpu_reg_read(vcpu, regs, Rn);
		offset_addr = (add) ? (address + offset) : (address - offset);
		address = (postindex) ? address : offset_addr;
		data = 0x0;
		if ((rc = cpu_vcpu_cp15_mem_read(vcpu, regs, address, 
						 &data, 1, TRUE))) {
			return rc;
		}
		cpu_vcpu_reg_write(vcpu, regs, Rt, 
					arm_sign_extend(data, 8, 32));
		if (postindex) {
			cpu_vcpu_reg_write(vcpu, regs, Rn, offset_addr);
		}
	}
	regs->pc += 4;
	return VMM_OK;
}

/** Emulate 'ldrd (immediate)' instruction */
int arm_inst_ldrd_i(u32 inst, vmm_user_regs_t * regs, vmm_vcpu_t * vcpu)
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
		arm_unpredictable(regs, vcpu);
		return VMM_EFAIL;
	}
	if ((P == 0) && (W == 1)) {
		arm_unpredictable(regs, vcpu);
		return VMM_EFAIL;
	}
	imm32 = arm_zero_extend((imm4H << 4) | imm4L, 32);
	index = (P == 1) ? TRUE : FALSE;
	add = (U == 1) ? TRUE : FALSE;
	wback = ((P == 0) || (W == 1)) ? TRUE : FALSE;
	if (wback && ((Rn == Rt) || (Rn == (Rt + 1)))) {
		arm_unpredictable(regs, vcpu);
		return VMM_EFAIL;
	}
	if (Rt == 14) {
		arm_unpredictable(regs, vcpu);
		return VMM_EFAIL;
	}
	if (arm_condition_passed(cond, regs)) {
		address = cpu_vcpu_reg_read(vcpu, regs, Rn);
		offset_addr = (add) ? (address + imm32) : 
					(address - imm32);
		address = (index) ? offset_addr : address;
		data = 0x0;
		if ((rc = cpu_vcpu_cp15_mem_read(vcpu, regs, address, 
						 &data, 4, FALSE))) {
			return rc;
		}
		cpu_vcpu_reg_write(vcpu, regs, Rt, data);
		data = 0x0;
		if ((rc = cpu_vcpu_cp15_mem_read(vcpu, regs, address + 4, 
						 &data, 4, FALSE))) {
			return rc;
		}
		cpu_vcpu_reg_write(vcpu, regs, Rt + 1, data);
		if (wback) {
			cpu_vcpu_reg_write(vcpu, regs, Rn, offset_addr);
		}
	}
	regs->pc += 4;
	return VMM_OK;
}

/** Emulate 'ldrd (literal)' instruction */
int arm_inst_ldrd_l(u32 inst, vmm_user_regs_t * regs, vmm_vcpu_t * vcpu)
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
		arm_unpredictable(regs, vcpu);
		return VMM_EFAIL;
	}
	imm32 = arm_zero_extend((imm4H << 4) | imm4L, 32);
	add = (U == 1) ? TRUE : FALSE;
	if (Rt == 14) {
		arm_unpredictable(regs, vcpu);
		return VMM_EFAIL;
	}
	if (arm_condition_passed(cond, regs)) {
		address = arm_align(regs->pc, 4);
		address = (add) ? (address + imm32) : (address - imm32);
		data = 0x0;
		if ((rc = cpu_vcpu_cp15_mem_read(vcpu, regs, address, 
						 &data, 4, FALSE))) {
			return rc;
		}
		cpu_vcpu_reg_write(vcpu, regs, Rt, data);
		data = 0x0;
		if ((rc = cpu_vcpu_cp15_mem_read(vcpu, regs, address + 4, 
						 &data, 4, FALSE))) {
			return rc;
		}
		cpu_vcpu_reg_write(vcpu, regs, Rt + 1, data);
	}
	regs->pc += 4;
	return VMM_OK;
}

/** Emulate 'ldrd (register)' instruction */
int arm_inst_ldrd_r(u32 inst, vmm_user_regs_t * regs, vmm_vcpu_t * vcpu)
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
		arm_unpredictable(regs, vcpu);
		return VMM_EFAIL;
	}
	if ((P == 0) && (W == 1)) {
		arm_unpredictable(regs, vcpu);
		return VMM_EFAIL;
	}
	index = (P == 1) ? TRUE : FALSE;
	add = (U == 1) ? TRUE : FALSE;
	wback = ((P == 0) || (W == 1)) ? TRUE : FALSE;
	if ((Rt == 14) || (Rm == 15) || (Rm == Rt) || (Rm == (Rt + 1))) {
		arm_unpredictable(regs, vcpu);
		return VMM_EFAIL;
	}
	if (wback && (Rn == 15 || (Rn == Rt) || (Rn == (Rt + 1)))) {
		arm_unpredictable(regs, vcpu);
		return VMM_EFAIL;
	}
	if (arm_condition_passed(cond, regs)) {
		offset = cpu_vcpu_reg_read(vcpu, regs, Rm);
		address = cpu_vcpu_reg_read(vcpu, regs, Rn);
		offset_addr = (add) ? (address + offset) : (address - offset);
		address = (index) ? offset_addr : address;
		data = 0x0;
		if ((rc = cpu_vcpu_cp15_mem_read(vcpu, regs, address, 
						 &data, 4, FALSE))) {
			return rc;
		}
		cpu_vcpu_reg_write(vcpu, regs, Rt, data);
		data = 0x0;
		if ((rc = cpu_vcpu_cp15_mem_read(vcpu, regs, address + 4, 
						 &data, 4, FALSE))) {
			return rc;
		}
		cpu_vcpu_reg_write(vcpu, regs, Rt + 1, data);
		if (wback) {
			cpu_vcpu_reg_write(vcpu, regs, Rn, offset_addr);
		}
	}
	regs->pc += 4;
	return VMM_OK;
}

/** Emulate 'strd (immediate)' instruction */
int arm_inst_strd_i(u32 inst, vmm_user_regs_t * regs, vmm_vcpu_t * vcpu)
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
		arm_unpredictable(regs, vcpu);
		return VMM_EFAIL;
	}
	if ((P == 0) && (W == 1)) {
		arm_unpredictable(regs, vcpu);
		return VMM_EFAIL;
	}
	imm32 = arm_zero_extend((imm4H << 4) | imm4L, 32);
	index = (P == 1) ? TRUE : FALSE;
	add = (U == 1) ? TRUE : FALSE;
	wback = ((P == 0) || (W == 1)) ? TRUE : FALSE;
	if (wback && ((Rn == 15) || (Rn == Rt) || (Rn == (Rt + 1)))) {
		arm_unpredictable(regs, vcpu);
		return VMM_EFAIL;
	}
	if (Rt == 14) {
		arm_unpredictable(regs, vcpu);
		return VMM_EFAIL;
	}
	if (arm_condition_passed(cond, regs)) {
		address = cpu_vcpu_reg_read(vcpu, regs, Rn);
		offset_addr = (add) ? (address + imm32) : 
					(address - imm32);
		address = (index) ? offset_addr : address;
		data = cpu_vcpu_reg_read(vcpu, regs, Rt);
		if ((rc = cpu_vcpu_cp15_mem_write(vcpu, regs, address, 
						  &data, 4, FALSE))) {
			return rc;
		}
		data = cpu_vcpu_reg_read(vcpu, regs, Rt + 1);
		if ((rc = cpu_vcpu_cp15_mem_write(vcpu, regs, address + 4, 
						  &data, 4, FALSE))) {
			return rc;
		}
		if (wback) {
			cpu_vcpu_reg_write(vcpu, regs, Rn, offset_addr);
		}
	}
	regs->pc += 4;
	return VMM_OK;
}

/** Emulate 'strd (register)' instruction */
int arm_inst_strd_r(u32 inst, vmm_user_regs_t * regs, vmm_vcpu_t * vcpu)
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
		arm_unpredictable(regs, vcpu);
		return VMM_EFAIL;
	}
	index = (P == 1) ? TRUE : FALSE;
	add = (U == 1) ? TRUE : FALSE;
	wback = ((P == 0) || (W == 1)) ? TRUE : FALSE;
	if ((Rt == 14) || (Rm == 15)) {
		arm_unpredictable(regs, vcpu);
		return VMM_EFAIL;
	}
	if (wback && (Rn == 15 || (Rn == Rt) || (Rn == (Rt + 1)))) {
		arm_unpredictable(regs, vcpu);
		return VMM_EFAIL;
	}
	if (arm_condition_passed(cond, regs)) {
		offset = cpu_vcpu_reg_read(vcpu, regs, Rm);
		address = cpu_vcpu_reg_read(vcpu, regs, Rn);
		offset_addr = (add) ? (address + offset) : 
					(address - offset);
		address = (index) ? offset_addr : address;
		data = cpu_vcpu_reg_read(vcpu, regs, Rt);
		if ((rc = cpu_vcpu_cp15_mem_write(vcpu, regs, address, 
						  &data, 4, FALSE))) {
			return rc;
		}
		data = cpu_vcpu_reg_read(vcpu, regs, Rt + 1);
		if ((rc = cpu_vcpu_cp15_mem_write(vcpu, regs, address + 4, 
						  &data, 4, FALSE))) {
			return rc;
		}
		if (wback) {
			cpu_vcpu_reg_write(vcpu, regs, Rn, offset_addr);
		}
	}
	regs->pc += 4;
	return VMM_OK;
}

/** Emulate data processing instructions */
int arm_instgrp_dataproc(u32 inst, vmm_user_regs_t * regs, vmm_vcpu_t * vcpu)
{
	u32 op, op1, Rn, op2;
	u32 is_op1_0xx1x, is_op1_xx0x0, is_op1_xx0x1, is_op1_xx1x0;
	u32 is_op1_xx1x1, is_op1_0xxxx;
	u32 is_op2_1011, is_op2_1101, is_op2_1111, is_op2_11x1;

	op = ARM_INST_DECODE(inst,
			     ARM_INST_DATAPROC_OP_MASK,
			     ARM_INST_DATAPROC_OP_SHIFT);
	op1 = ARM_INST_DECODE(inst,
			      ARM_INST_DATAPROC_OP1_MASK,
			      ARM_INST_DATAPROC_OP1_SHIFT);
	Rn = ARM_INST_DECODE(inst,
			     ARM_INST_DATAPROC_RN_MASK,
			     ARM_INST_DATAPROC_RN_SHIFT);
	op2 = ARM_INST_DECODE(inst,
			      ARM_INST_DATAPROC_OP2_MASK,
			      ARM_INST_DATAPROC_OP2_SHIFT);

	is_op1_0xxxx = !(op1 & 0x10);
	is_op1_0xx1x = !(op1 & 0x10) && (op1 & 0x2);
	is_op1_xx0x0 = !(op1 & 0x4) && !(op1 & 0x1);
	is_op1_xx0x1 = !(op1 & 0x4) && (op1 & 0x1);
	is_op1_xx1x0 = (op1 & 0x4) && !(op1 & 0x1);
	is_op1_xx1x1 = (op1 & 0x4) && (op1 & 0x1);
	is_op2_1011 = (op2 == 0xB);
	is_op2_1101 = (op2 == 0xD);
	is_op2_1111 = (op2 == 0xF);
	is_op2_11x1 = is_op2_1101 || is_op2_1111;

	if (!op && !is_op1_0xx1x && (is_op2_1011 || is_op2_11x1)){
		/* Extra load/store instructions */
		switch (op2) {
		case 0xB:
			if (is_op1_xx0x0) {
				/* STRH (register) */
				return arm_inst_strh_r(inst, regs, vcpu);
			} else if (is_op1_xx0x1) {
				/* LDRH (register) */
				return arm_inst_ldrh_r(inst, regs, vcpu);
			} else if (is_op1_xx1x0) {
				/* STRH (immediate, ARM) */
				return arm_inst_strh_i(inst, regs, vcpu);
			} else if (is_op1_xx1x1) {
				if (Rn == 0xF) {
					/* LDRH (literal) */
					return arm_inst_ldrh_l(inst, regs, vcpu);
				} else {
					/* LDRH (immediate, ARM) */
					return arm_inst_ldrh_i(inst, regs, vcpu);
				}
			}
			break;
		case 0xD:
			if (is_op1_xx0x0) {
				/* LDRD (register) */
				return arm_inst_ldrd_r(inst, regs, vcpu);
			} else if (is_op1_xx0x1) {
				/* LDRSB (register) */
				return arm_inst_ldrsb_r(inst, regs, vcpu);
			} else if (is_op1_xx1x0) {
				if (Rn == 0xF) {
					/* LDRD (literal) */
					return arm_inst_ldrd_l(inst, regs, vcpu);
				} else {
					/* LDRD (immediate) */
					return arm_inst_ldrd_i(inst, regs, vcpu);
				}
			} else if (is_op1_xx1x1) {
				if (Rn == 0xF) {
					/* LDRSB (literal) */
					return arm_inst_ldrsb_l(inst, regs, vcpu);
				} else {
					/* LDRSB (immediate) */
					return arm_inst_ldrsb_i(inst, regs, vcpu);
				}
			}
			break;
		case 0xF:
			if (is_op1_xx0x0) {
				/* STRD (register) */
				return arm_inst_strd_r(inst, regs, vcpu);
			} else if (is_op1_xx0x1) {
				/* LDRSH (register) */
				return arm_inst_ldrsh_r(inst, regs, vcpu);
			} else if (is_op1_xx1x0) {
				/* STRD (immediate) */
				return arm_inst_strd_i(inst, regs, vcpu);
			} else if (is_op1_xx1x1) {
				if (Rn == 0xF) {
					/* LDRSH (literal) */
					return arm_inst_ldrsh_l(inst, regs, vcpu);
				} else {
					/* LDRSH (immediate) */
					return arm_inst_ldrsh_i(inst, regs, vcpu);
				}
			}
			break;
		default:
			break;
		};
	} if (!op && is_op1_0xx1x && (is_op2_1011 || is_op2_11x1)) {
		/* Extra load/store instructions (unpriviledged) */
		if (is_op2_1011) {
			if (is_op1_0xxxx) {
				/* STRHT */
				return arm_inst_strht(inst, regs, vcpu);
			} else {
				/* LDRHT */
				return arm_inst_ldrht(inst, regs, vcpu);
			}
		} else if (is_op2_1101 && !is_op1_0xxxx) {
			/* LDRSBT */
				return arm_inst_ldrsbt(inst, regs, vcpu);
		} else if (is_op2_1111 && !is_op1_0xxxx) {
			/* LDRSHT */
				return arm_inst_ldrsht(inst, regs, vcpu);
		}
	}

	arm_unpredictable(regs, vcpu);
	return VMM_EFAIL;
}

/** Emulate 'str (immediate)' instruction */
int arm_inst_str_i(u32 inst, vmm_user_regs_t * regs, vmm_vcpu_t * vcpu)
{
	int rc;
	u32 cond, P, U, W, Rn, Rt, imm12;
	u32 imm32, address, offset_addr, data;
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
		arm_unpredictable(regs, vcpu);
		return VMM_EFAIL;
	}
	imm32 = arm_zero_extend(imm12, 32);
	index = (P == 1) ? TRUE : FALSE;
	add = (U == 1) ? TRUE : FALSE;
	wback = ((P == 0) || (W == 1)) ? TRUE : FALSE;
	if (wback && ((Rn == 15) || (Rn == Rt))) {
		arm_unpredictable(regs, vcpu);
		return VMM_EFAIL;
	}
	if (arm_condition_passed(cond, regs)) {
		address = cpu_vcpu_reg_read(vcpu, regs, Rn);
		offset_addr = (add) ? (address + imm32) : (address - imm32);
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
	return VMM_OK;
}

/** Emulate 'str (register)' instruction */
int arm_inst_str_r(u32 inst, vmm_user_regs_t * regs, vmm_vcpu_t * vcpu)
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
		arm_unpredictable(regs, vcpu);
		return VMM_EFAIL;
	}
	index = (P == 1) ? TRUE : FALSE;
	add = (U == 1) ? TRUE : FALSE;
	wback = ((P == 0) || (W == 1)) ? TRUE : FALSE;
	arm_decode_imm_shift(type, imm5, &shift_t, &shift_n);
	if (Rm == 15) {
		arm_unpredictable(regs, vcpu);
		return VMM_EFAIL;
	}
	if (wback && (Rn == 15 || Rn == Rt)) {
		arm_unpredictable(regs, vcpu);
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
	return VMM_OK;
}

/** Emulate 'strt' intruction */
int arm_inst_strt(u32 inst, vmm_user_regs_t * regs, vmm_vcpu_t * vcpu)
{
	int rc;
	u32 cond, U, Rn, Rt, imm12, imm32, imm5, type, Rm;
	u32 shift_t, shift_n, offset, offset_addr, address; u32 data;
	bool regform, postindex, add;
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
			arm_unpredictable(regs, vcpu);
			return VMM_EFAIL;
		}
		imm32 = 0;
		arm_decode_imm_shift(type, imm5, &shift_t, &shift_n);
	} else {
		if ((Rn == 15) || (Rn == Rt)) {
			arm_unpredictable(regs, vcpu);
			return VMM_EFAIL;
		}
		imm32 = arm_zero_extend(imm12, 32);
		shift_t = 0;
		shift_n = 0;
	}
	postindex = TRUE;
	add = (U == 1) ? TRUE : FALSE;
	if (arm_condition_passed(cond, regs)) {
		offset = (regform) ? 
			 arm_shift(cpu_vcpu_reg_read(vcpu, regs, Rm), 
				  shift_t, 
				  shift_n, 
				  (regs->cpsr >> CPSR_CARRY_SHIFT) & 0x1)
				  : imm32;
		address = cpu_vcpu_reg_read(vcpu, regs, Rn);
		offset_addr = (add) ? (address + offset) : 
					(address - offset);
		address = (postindex) ? address : offset_addr;
		data = cpu_vcpu_reg_read(vcpu, regs, Rt);
		if ((rc = cpu_vcpu_cp15_mem_write(vcpu, regs, address, 
						  &data, 4, TRUE))) {
			return rc;
		}
		if (postindex) {
			cpu_vcpu_reg_write(vcpu, regs, Rn, offset_addr);
		}
	}
	regs->pc += 4;
	return VMM_OK;
}

/** Emulate 'strb (immediate)' instruction */
int arm_inst_strb_i(u32 inst, vmm_user_regs_t * regs, vmm_vcpu_t * vcpu)
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
		arm_unpredictable(regs, vcpu);
		return VMM_EFAIL;
	}
	imm32 = arm_zero_extend(imm12, 32);
	index = (P == 1) ? TRUE : FALSE;
	add = (U == 1) ? TRUE : FALSE;
	wback = ((P == 0) || (W == 1)) ? TRUE : FALSE;
	if (Rt == 15) {
		arm_unpredictable(regs, vcpu);
		return VMM_EFAIL;
	}
	if (wback && ((Rn == 15) || (Rn == Rt))) {
		arm_unpredictable(regs, vcpu);
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
	return VMM_OK;
}

/** Emulate 'strb (register)' instruction */
int arm_inst_strb_r(u32 inst, vmm_user_regs_t * regs, vmm_vcpu_t * vcpu)
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
		arm_unpredictable(regs, vcpu);
		return VMM_EFAIL;
	}
	index = (P == 1) ? TRUE : FALSE;
	add = (U == 1) ? TRUE : FALSE;
	wback = ((P == 0) || (W == 1)) ? TRUE : FALSE;
	arm_decode_imm_shift(type, imm5, &shift_t, &shift_n);
	if ((Rt == 15) || (Rm == 15)) {
		arm_unpredictable(regs, vcpu);
		return VMM_EFAIL;
	}
	if (wback && (Rn == 15 || Rn == Rt)) {
		arm_unpredictable(regs, vcpu);
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
	return VMM_OK;
}

/** Emulate 'strbt' intruction */
int arm_inst_strbt(u32 inst, vmm_user_regs_t * regs, vmm_vcpu_t * vcpu)
{
	int rc;
	u32 cond, U, Rn, Rt, imm12, imm32, imm5, type, Rm;
	u32 shift_t, shift_n, offset, offset_addr, address; u8 data;
	bool regform, postindex, add;
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
			arm_unpredictable(regs, vcpu);
			return VMM_EFAIL;
		}
		imm32 = 0;
		arm_decode_imm_shift(type, imm5, &shift_t, &shift_n);
	} else {
		if ((Rt == 15) || (Rn == 15) || (Rn == Rt)) {
			arm_unpredictable(regs, vcpu);
			return VMM_EFAIL;
		}
		imm32 = arm_zero_extend(imm12, 32);
		shift_t = 0;
		shift_n = 0;
	}
	postindex = TRUE;
	add = (U == 1) ? TRUE : FALSE;
	if (arm_condition_passed(cond, regs)) {
		offset = (regform) ? 
			 arm_shift(cpu_vcpu_reg_read(vcpu, regs, Rm), 
				  shift_t, 
				  shift_n, 
				  (regs->cpsr >> CPSR_CARRY_SHIFT) & 0x1)
				  : imm32;
		address = cpu_vcpu_reg_read(vcpu, regs, Rn);
		offset_addr = (add) ? (address + offset) : 
					(address - offset);
		address = (postindex) ? address : offset_addr;
		data = cpu_vcpu_reg_read(vcpu, regs, Rt) & 0xFF;
		if ((rc = cpu_vcpu_cp15_mem_write(vcpu, regs, address, 
						  &data, 1, TRUE))) {
			return rc;
		}
		if (postindex) {
			cpu_vcpu_reg_write(vcpu, regs, Rn, offset_addr);
		}
	}
	regs->pc += 4;
	return VMM_OK;
}

/** Emulate 'ldr (immediate)' instruction */
int arm_inst_ldr_i(u32 inst, vmm_user_regs_t * regs, vmm_vcpu_t * vcpu)
{
	int rc;
	u32 cond, P, U, W, Rn, Rt, imm12;
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
	imm12 = ARM_INST_BITS(inst,
			      ARM_INST_LDRSTR_IMM12_END,
			      ARM_INST_LDRSTR_IMM12_START);
	if ((P == 0) && (W == 1)) {
		arm_unpredictable(regs, vcpu);
		return VMM_EFAIL;
	}
	imm32 = arm_zero_extend(imm12, 32);
	index = (P == 1) ? TRUE : FALSE;
	add = (U == 1) ? TRUE : FALSE;
	wback = ((P == 0) || (W == 1)) ? TRUE : FALSE;
	if (wback && (Rn == Rt)) {
		arm_unpredictable(regs, vcpu);
		return VMM_EFAIL;
	}
	if (arm_condition_passed(cond, regs)) {
		address = cpu_vcpu_reg_read(vcpu, regs, Rn);
		offset_addr = (add) ? (address + imm32) : 
					(address - imm32);
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
	return VMM_OK;
}

/** Emulate 'ldr (literal)' instruction */
int arm_inst_ldr_l(u32 inst, vmm_user_regs_t * regs, vmm_vcpu_t * vcpu)
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
	return VMM_OK;
}

/** Emulate 'ldr (register)' instruction */
int arm_inst_ldr_r(u32 inst, vmm_user_regs_t * regs, vmm_vcpu_t * vcpu)
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
		arm_unpredictable(regs, vcpu);
		return VMM_EFAIL;
	}
	index = (P == 1) ? TRUE : FALSE;
	add = (U == 1) ? TRUE : FALSE;
	wback = ((P == 0) || (W == 1)) ? TRUE : FALSE;
	arm_decode_imm_shift(type, imm5, &shift_t, &shift_n);
	if (Rm == 15) {
		arm_unpredictable(regs, vcpu);
		return VMM_EFAIL;
	}
	if (wback && (Rn == 15 || Rn == Rt)) {
		arm_unpredictable(regs, vcpu);
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
	return VMM_OK;
}

/** Emulate 'ldrt' intruction */
int arm_inst_ldrt(u32 inst, vmm_user_regs_t * regs, vmm_vcpu_t * vcpu)
{
	int rc;
	u32 cond, U, Rn, Rt, imm12, imm32, imm5, type, Rm;
	u32 shift_t, shift_n, offset, offset_addr, address; u32 data;
	bool regform, postindex, add;
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
			arm_unpredictable(regs, vcpu);
			return VMM_EFAIL;
		}
		imm32 = 0;
		arm_decode_imm_shift(type, imm5, &shift_t, &shift_n);
	} else {
		if ((Rt == 15) || (Rn == 15) || (Rn == Rt)) {
			arm_unpredictable(regs, vcpu);
			return VMM_EFAIL;
		}
		imm32 = arm_zero_extend(imm12, 32);
		shift_t = 0;
		shift_n = 0;
	}
	postindex = TRUE;
	add = (U == 1) ? TRUE : FALSE;
	if (arm_condition_passed(cond, regs)) {
		offset = (regform) ? 
			 arm_shift(cpu_vcpu_reg_read(vcpu, regs, Rm), 
				  shift_t, 
				  shift_n, 
				  (regs->cpsr >> CPSR_CARRY_SHIFT) & 0x1)
				  : imm32;
		address = cpu_vcpu_reg_read(vcpu, regs, Rn);
		offset_addr = (add) ? (address + offset) : 
					(address - offset);
		address = (postindex) ? address : offset_addr;
		if (postindex) {
			cpu_vcpu_reg_write(vcpu, regs, Rn, offset_addr);
		}
		data = 0x0;
		if ((rc = cpu_vcpu_cp15_mem_read(vcpu, regs, address, 
						 &data, 4, TRUE))) {
			return rc;
		}
		cpu_vcpu_reg_write(vcpu, regs, Rt, arm_zero_extend(data, 32));
	}
	regs->pc += 4;
	return VMM_OK;
}

/** Emulate 'ldrb (immediate)' instruction */
int arm_inst_ldrb_i(u32 inst, vmm_user_regs_t * regs, vmm_vcpu_t * vcpu)
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
		arm_unpredictable(regs, vcpu);
		return VMM_EFAIL;
	}
	imm32 = arm_zero_extend(imm12, 32);
	index = (P == 1) ? TRUE : FALSE;
	add = (U == 1) ? TRUE : FALSE;
	wback = ((P == 0) || (W == 1)) ? TRUE : FALSE;
	if (Rt == 15) {
		arm_unpredictable(regs, vcpu);
		return VMM_EFAIL;
	}
	if (wback && (Rn == Rt)) {
		arm_unpredictable(regs, vcpu);
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
	return VMM_OK;
}

/** Emulate 'ldrb (literal)' instruction */
int arm_inst_ldrb_l(u32 inst, vmm_user_regs_t * regs, vmm_vcpu_t * vcpu)
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
		arm_unpredictable(regs, vcpu);
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
	return VMM_OK;
}

/** Emulate 'ldrb (register)' instruction */
int arm_inst_ldrb_r(u32 inst, vmm_user_regs_t * regs, vmm_vcpu_t * vcpu)
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
		arm_unpredictable(regs, vcpu);
		return VMM_EFAIL;
	}
	index = (P == 1) ? TRUE : FALSE;
	add = (U == 1) ? TRUE : FALSE;
	wback = ((P == 0) || (W == 1)) ? TRUE : FALSE;
	arm_decode_imm_shift(type, imm5, &shift_t, &shift_n);
	if ((Rt == 15) || (Rm == 15)) {
		arm_unpredictable(regs, vcpu);
		return VMM_EFAIL;
	}
	if (wback && (Rn == 15 || Rn == Rt)) {
		arm_unpredictable(regs, vcpu);
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
	return VMM_OK;
}

/** Emulate 'ldrbt' intruction */
int arm_inst_ldrbt(u32 inst, vmm_user_regs_t * regs, vmm_vcpu_t * vcpu)
{
	int rc;
	u32 cond, U, Rn, Rt, imm12, imm32, imm5, type, Rm;
	u32 shift_t, shift_n, offset, offset_addr, address; u8 data;
	bool regform, postindex, add;
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
			arm_unpredictable(regs, vcpu);
			return VMM_EFAIL;
		}
		imm32 = 0;
		arm_decode_imm_shift(type, imm5, &shift_t, &shift_n);
	} else {
		if ((Rt == 15) || (Rn == 15) || (Rn == Rt)) {
			arm_unpredictable(regs, vcpu);
			return VMM_EFAIL;
		}
		imm32 = arm_zero_extend(imm12, 32);
		shift_t = 0;
		shift_n = 0;
	}
	postindex = TRUE;
	add = (U == 1) ? TRUE : FALSE;
	if (arm_condition_passed(cond, regs)) {
		offset = (regform) ? 
			 arm_shift(cpu_vcpu_reg_read(vcpu, regs, Rm), 
				  shift_t, 
				  shift_n, 
				  (regs->cpsr >> CPSR_CARRY_SHIFT) & 0x1)
				  : imm32;
		address = cpu_vcpu_reg_read(vcpu, regs, Rn);
		offset_addr = (add) ? (address + offset) : 
					(address - offset);
		address = (postindex) ? address : offset_addr;
		data = 0x0;
		if ((rc = cpu_vcpu_cp15_mem_read(vcpu, regs, address, 
						 &data, 1, TRUE))) {
			return rc;
		}
		cpu_vcpu_reg_write(vcpu, regs, Rt, arm_zero_extend(data, 32));
		if (postindex) {
			cpu_vcpu_reg_write(vcpu, regs, Rn, offset_addr);
		}
	}
	regs->pc += 4;
	return VMM_OK;
}

/** Emulate load/store instructions */
int arm_instgrp_ldrstr(u32 inst, vmm_user_regs_t * regs, vmm_vcpu_t * vcpu)
{
	u32 A, op1, rn, B;
	u32 is_xx0x0, is_0x010, is_xx0x1, is_0x011, is_xx1x0, is_0x110;
	u32 is_xx1x1, is_0x111;

	A = ARM_INST_DECODE(inst,
			    ARM_INST_LDRSTR_A_MASK,
			    ARM_INST_LDRSTR_A_SHIFT);
	op1 = ARM_INST_DECODE(inst,
			      ARM_INST_LDRSTR_OP1_MASK,
			      ARM_INST_LDRSTR_OP1_SHIFT);
	rn = ARM_INST_DECODE(inst,
			     ARM_INST_LDRSTR_RN_MASK, 
			     ARM_INST_LDRSTR_RN_SHIFT);
	B = ARM_INST_DECODE(inst,
			    ARM_INST_LDRSTR_B_MASK,
			    ARM_INST_LDRSTR_B_SHIFT);

	is_xx0x0 = !(op1 & 0x5);
	is_0x010 = is_xx0x0 && !(op1 & 0x10) && (op1 & 0x2);
	is_xx0x1 = !(op1 & 0x4) && (op1 & 0x1);
	is_0x011 = !(op1 & 0x14) && (op1 & 0x3);
	is_xx1x0 = (op1 & 0x4) && !(op1 & 0x1);
	is_0x110 = !(op1 & 0x11) && (op1 & 0x6);
	is_xx1x1 = (op1 & 0x5);
	is_0x111 = !(op1 & 0x10) && (op1 & 0x7);

	if (A) {
		if (is_xx0x0 && !is_0x010 && !B) {
			/* STR (register) */
			return arm_inst_str_r(inst, regs, vcpu);
		} else if (is_0x010 && !B) {
			/* STRT */
			return arm_inst_strt(inst, regs, vcpu);
		} else if (is_xx0x1 && !is_0x011) {
			/* LDR (register) */
			return arm_inst_ldr_r(inst, regs, vcpu);
		} else if (is_0x011 && !B) {
			/* LDRT */
			return arm_inst_ldrt(inst, regs, vcpu);
		} else if (is_xx1x0 && !is_0x110 && !B) {
			/* STRB (register) */
			return arm_inst_strb_r(inst, regs, vcpu);
		} else if (is_0x110 && !B) {
			/* STRBT */
			return arm_inst_strbt(inst, regs, vcpu);
		} else if (is_xx1x1 && !is_0x111 && !B) {
			/* LDRB (register) */
			return arm_inst_ldrb_r(inst, regs, vcpu);
		} else if (is_0x111 && !B) {
			/* LDRBT */
			return arm_inst_ldrbt(inst, regs, vcpu);
		}
	} else {
		if (is_xx0x0 && !is_0x010) {
			/* STR (immediate) */
			return arm_inst_str_i(inst, regs, vcpu);
		} else if (is_0x010) {
			/* STRT */
			return arm_inst_strt(inst, regs, vcpu);
		} else if (is_xx0x1 && !is_0x011) {
			if (rn == 0xF) {
				/* LDR (literal) */
				return arm_inst_ldr_l(inst, regs, vcpu);
			} else {
				/* LDR (immediate) */
				return arm_inst_ldr_i(inst, regs, vcpu);
			}
		} else if (is_0x011) {
			/* LDRT */
			return arm_inst_ldrt(inst, regs, vcpu);
		} else if (is_xx1x0 && !is_0x110) {
			/* STRB (immediate) */
			return arm_inst_strb_i(inst, regs, vcpu);
		} else if (is_0x110) {
			/* STRBT */
			return arm_inst_strbt(inst, regs, vcpu);
		} else if (is_xx1x1 && !is_0x111) {
			if (rn == 0xF) {
				/* LDRB (literal) */
				return arm_inst_ldrb_l(inst, regs, vcpu);
			} else {
				/* LDRB (immediate) */
				return arm_inst_ldrb_i(inst, regs, vcpu);
			}
		} else if (is_0x111) {
			/* LDRBT */
			return arm_inst_ldrbt(inst, regs, vcpu);
		}
	}
	arm_unpredictable(regs, vcpu);
	return VMM_EFAIL;
}

/** Emulate media instructions */
int arm_instgrp_media(u32 inst, vmm_user_regs_t * regs, vmm_vcpu_t * vcpu)
{
	arm_unpredictable(regs, vcpu);
	return VMM_EFAIL;
}

/** Emulate branch, branch with link, and block transfer instructions */
int arm_instgrp_brblk(u32 inst, vmm_user_regs_t * regs, vmm_vcpu_t * vcpu)
{
	arm_unpredictable(regs, vcpu);
	return VMM_EFAIL;
}

/** Emulate 'stc/stc2' instruction */
int arm_inst_stcx(u32 inst, vmm_user_regs_t * regs, vmm_vcpu_t * vcpu)
{
	int rc;
	u32 cond, P, U, D, W, Rn, CRd, coproc, imm8;
	u32 imm32, offset_addr, address, i, data;
	bool index, add, wback, uopt;
	cpu_vcpu_coproc_t *cp = NULL;
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
		arm_unpredictable(regs, vcpu);
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
				if ((rc = cpu_vcpu_cp15_mem_write(vcpu, regs,
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
	regs->pc += 4;
	return VMM_OK;
}

/** Emulate 'ldc_i/ldc2_i' instruction */
int arm_inst_ldcx_i(u32 inst, vmm_user_regs_t * regs, vmm_vcpu_t * vcpu)
{
	int rc;
	u32 cond, P, U, D, W, Rn, CRd, coproc, imm8;
	u32 imm32, offset_addr, address, i, data;
	bool index, add, wback, uopt;
	cpu_vcpu_coproc_t *cp = NULL;
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
		arm_unpredictable(regs, vcpu);
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
				if ((rc = cpu_vcpu_cp15_mem_read(vcpu, regs,
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
	regs->pc += 4;
	return VMM_OK;
}

/** Emulate 'ldc_l/ldc2_l' instruction */
int arm_inst_ldcx_l(u32 inst, vmm_user_regs_t * regs, vmm_vcpu_t * vcpu)
{
	int rc;
	u32 cond, P, U, D, W, CRd, coproc, imm8;
	u32 imm32, offset_addr, address, i, data;
	bool index, add, uopt;
	cpu_vcpu_coproc_t *cp = NULL;
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
		arm_unpredictable(regs, vcpu);
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
			offset_addr = (add) ? (arm_align(regs->pc, 4) + imm32) :
					      (arm_align(regs->pc, 4) - imm32);
			address = (index) ? offset_addr : arm_align(regs->pc, 4);
			i = 0;
			while (!cp->ldcstc_done(vcpu, regs, i, D, 
							CRd, uopt, imm8)) {
				data = 0x0;
				if ((rc = cpu_vcpu_cp15_mem_read(vcpu, regs,
						address, &data, 4, FALSE))) {
					return rc;
				}
				cp->ldcstc_write(vcpu, regs, i, D, 
							CRd, uopt, imm8, data);
				address += 4;
			}
		}
	}
	regs->pc += 4;
	return VMM_OK;
}

/** Emulate 'mcrr/mcrr2' instruction */
int arm_inst_mcrrx(u32 inst, vmm_user_regs_t * regs, vmm_vcpu_t * vcpu)
{
	u32 cond, Rt2, Rt, coproc, opc1, CRm;
	u32 data, data2;
	cpu_vcpu_coproc_t *cp = NULL;
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
		arm_unpredictable(regs, vcpu);
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
	regs->pc += 4;
	return VMM_OK;
}

/** Emulate 'mrrc/mrrc2' instruction */
int arm_inst_mrrcx(u32 inst, vmm_user_regs_t * regs, vmm_vcpu_t * vcpu)
{
	u32 cond, Rt2, Rt, coproc, opc1, CRm;
	u32 data, data2;
	cpu_vcpu_coproc_t *cp = NULL;
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
		arm_unpredictable(regs, vcpu);
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
	regs->pc += 4;
	return VMM_OK;
}

/** Emulate 'cdp/cdp2' instruction */
int arm_inst_cdpx(u32 inst, vmm_user_regs_t * regs, vmm_vcpu_t * vcpu)
{
	u32 cond, opc1, opc2, coproc, CRd, CRn, CRm;
	cpu_vcpu_coproc_t *cp = NULL;
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
	regs->pc += 4;
	return VMM_OK;
}

/** Emulate 'mcr/mcr2' instruction */
int arm_inst_mcrx(u32 inst, vmm_user_regs_t * regs, vmm_vcpu_t * vcpu)
{
	u32 cond, opc1, opc2, coproc, Rt, CRn, CRm;
	u32 data;
	cpu_vcpu_coproc_t *cp = NULL;
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
	regs->pc += 4;
	return VMM_OK;
}

/** Emulate 'mrc/mrc2' instruction */
int arm_inst_mrcx(u32 inst, vmm_user_regs_t * regs, vmm_vcpu_t * vcpu)
{
	u32 cond, opc1, opc2, coproc, Rt, CRn, CRm;
	u32 data;
	cpu_vcpu_coproc_t *cp = NULL;
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
		cpu_vcpu_reg_write(vcpu, regs, Rt, data);
	}
	regs->pc += 4;
	return VMM_OK;
}

int arm_instgrp_coproc(u32 inst, vmm_user_regs_t * regs, vmm_vcpu_t * vcpu)
{
	u32 op1, rn, cpro, op;
	u32 is_op1_0xxxxx, is_op1_0xxxx0, is_op1_0xxxx1, is_op1_00000x;
	u32 is_op1_00010x, is_op1_000100, is_op1_000101, is_op1_10xxxx;
	u32 is_op1_10xxx0, is_op1_10xxx1, is_op1_11xxxx, is_op1_000x0x;
	u32 is_rn_1111, is_cpro_101x, is_op;
	op1 = ARM_INST_DECODE(inst,
			      ARM_INST_COPROC_OP1_MASK,
			      ARM_INST_COPROC_OP1_SHIFT);
	rn = ARM_INST_DECODE(inst,
			     ARM_INST_COPROC_RN_MASK, ARM_INST_COPROC_RN_SHIFT);
	cpro = ARM_INST_DECODE(inst,
			       ARM_INST_COPROC_CPRO_MASK,
			       ARM_INST_COPROC_CPRO_SHIFT);
	op = ARM_INST_DECODE(inst,
			     ARM_INST_COPROC_OP_MASK, ARM_INST_COPROC_OP_SHIFT);
	is_op1_0xxxxx = !(op1 & 0x20);
	is_op1_0xxxx0 = is_op1_0xxxxx && !(op1 & 0x1);
	is_op1_0xxxx1 = is_op1_0xxxxx && (op1 & 0x1);
	is_op1_00000x = !(op1 & 0x3E);
	is_op1_00010x = !(op1 & 0x38) && !(op1 & 0x2) && (op1 & 0x4);
	is_op1_000100 = is_op1_00010x && !(op1 & 0x1);
	is_op1_000101 = is_op1_00010x && (op1 & 0x1);
	is_op1_10xxxx = !(op1 & 0x10) && (op1 & 0x20);
	is_op1_10xxx0 = is_op1_10xxxx && !(op1 & 0x1);
	is_op1_10xxx1 = is_op1_10xxxx && (op1 & 0x1);
	is_op1_11xxxx = (op1 & 0x30);
	is_op1_000x0x = !(op1 & 0x2);
	is_rn_1111 = (rn == 0xF);
	is_cpro_101x = (cpro == 0xA) || (cpro == 0xB);
	is_op = (op != 0x0);
	if (is_op1_0xxxxx && !is_op1_000x0x && is_cpro_101x) {
		/* Advanced SIMD, VFP Extension register 
		 * load/store instructions 
		 */
		arm_unpredictable(regs, vcpu);
		return VMM_EFAIL;
	} else if (is_op1_0xxxx0 && !is_op1_000x0x && !is_cpro_101x) {
		/* Store Coprocessor 
		 * STC, STC2 
		 */
		return arm_inst_stcx(inst, regs, vcpu);
	} else if (is_op1_0xxxx1 &&
		   !is_op1_000x0x && !is_cpro_101x && !is_rn_1111) {
		/* Load Coprocessor 
		 * LDC, LDC2 (immediate) 
		 */
		return arm_inst_ldcx_i(inst, regs, vcpu);
	} else if (is_op1_00000x) {
		/** Undefined Instruction Space */
		arm_unpredictable(regs, vcpu);
		return VMM_EFAIL;
	} else if (is_op1_0xxxx1 &&
		   !is_op1_000x0x && !is_cpro_101x && is_rn_1111) {
		/* Load Coprocessor 
		 * LDC, LDC2 (literal) 
		 */
		return arm_inst_ldcx_l(inst, regs, vcpu);
	} else if (is_op1_00010x && is_cpro_101x) {
		/* Advanced SIMD, VFP 64-bit transfers between 
		 * ARM core and extension registers 
		 */
		arm_unpredictable(regs, vcpu);
		return VMM_EFAIL;
	} else if (is_op1_000100 && !is_cpro_101x) {
		/* Move to Coprocessor from two ARM core registers
		 * MCRR, MCRR2 
		 */
		return arm_inst_mcrrx(inst, regs, vcpu);
	} else if (is_op1_000101 && !is_cpro_101x) {
		/* Move to two ARM core registers from Coprocessor
		 * MRRC, MRRC2 
		 */
		return arm_inst_mrrcx(inst, regs, vcpu);
	} else if (is_op1_10xxxx && !is_op && is_cpro_101x) {
		/* VFP data-processing instructions 
		 */
		arm_unpredictable(regs, vcpu);
		return VMM_EFAIL;
	} else if (is_op1_10xxxx && !is_op && !is_cpro_101x) {
		/* Coprocessor data operations 
		 * CDP, CDP2 
		 */
		return arm_inst_cdpx(inst, regs, vcpu);
	} else if (is_op1_10xxxx && is_op && is_cpro_101x) {
		/* Advanced SIMD, VFP 8, 16, and 32-bit transfer 
		 * between ARM core and extension registers 
		 */
		arm_unpredictable(regs, vcpu);
		return VMM_EFAIL;
	} else if (is_op1_10xxx0 && is_op && !is_cpro_101x) {
		/* Move to Coprocessor from ARM core register
		 * MCR, MCR2 
		 */
		return arm_inst_mcrx(inst, regs, vcpu);
	} else if (is_op1_10xxx1 && is_op && !is_cpro_101x) {
		/* Move to ARM core register from Coprocessor
		 * MRC, MRC2 
		 */
		return arm_inst_mrcx(inst, regs, vcpu);
	} else if (is_op1_11xxxx) {
		/* Supervisor Call SVC (previously SWI) */
		return arm_instgrp_hypercall(inst, regs, vcpu);
	}
	arm_unpredictable(regs, vcpu);
	return VMM_EFAIL;
}

int cpu_vcpu_emulate_arm_inst(vmm_vcpu_t *vcpu, 
				vmm_user_regs_t * regs, bool is_hypercall)
{
	u32 inst, op1, op;

	/* Sanity check */
	if (!vcpu) {
		return VMM_EFAIL;
	}
	if (!vcpu->is_normal) {
		return VMM_EFAIL;
	}

	/* Fetch the faulting instruction from vcpu address-space */
	inst = *((u32 *) regs->pc);

	/* If we know that we are in a hypercall 
	 * then skip unnecessary instruction decoding */
	if (is_hypercall) {
		return arm_instgrp_hypercall(inst, regs, vcpu);
	}

	op1 = ARM_INST_DECODE(inst,
			      ARM_INST_OP1_MASK, ARM_INST_OP1_SHIFT);
	op = ARM_INST_DECODE(inst, ARM_INST_OP_MASK, ARM_INST_OP_SHIFT);
	switch (op1 & 0x6) {
	case 0x0:
		/* Data-processing and 
		 * miscellaneous instructions */
		return arm_instgrp_dataproc(inst, regs, vcpu);
		break;
	case 0x2:
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

	arm_unpredictable(regs, vcpu);

	return VMM_EFAIL;
}
