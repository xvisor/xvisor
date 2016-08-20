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
 * @author Anup Patel (anup@brainfault.org)
 * @brief source code to emulate ARM hypercall instructions
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_vcpu_irq.h>
#include <vmm_scheduler.h>
#include <arch_cpu.h>
#include <arch_regs.h>
#include <cpu_defines.h>
#include <cpu_vcpu_helper.h>
#include <cpu_vcpu_mem.h>
#include <cpu_vcpu_hypercall_arm.h>
#include <emulate_arm.h>
#include <emulate_psci.h>

/** Emulate 'cps' hypercall */
static int arm_hypercall_cps(u32 inst,
			     arch_regs_t *regs, struct vmm_vcpu *vcpu)
{
	register u32 cpsr, mask, imod, mode;
	imod = ARM_INST_BITS(inst,
			     ARM_HYPERCALL_CPS_IMOD_END,
			     ARM_HYPERCALL_CPS_IMOD_START);
	mode = ARM_INST_BITS(inst,
			     ARM_HYPERCALL_CPS_MODE_END,
			     ARM_HYPERCALL_CPS_MODE_START);
	cpsr = 0x0;
	mask = 0x0;
	if (ARM_INST_BIT(inst, ARM_HYPERCALL_CPS_M_START)) {
		cpsr |= mode;
		mask |= CPSR_MODE_MASK;
	}
	if (ARM_INST_BIT(inst, ARM_HYPERCALL_CPS_A_START)) {
		if (imod == 0x2) {
			cpsr &= ~CPSR_ASYNC_ABORT_DISABLED;
		} else if (imod == 0x3) {
			cpsr |= CPSR_ASYNC_ABORT_DISABLED;
		}
		mask |= CPSR_ASYNC_ABORT_DISABLED;
	}
	if (ARM_INST_BIT(inst, ARM_HYPERCALL_CPS_I_START)) {
		if (imod == 0x2) {
			cpsr &= ~CPSR_IRQ_DISABLED;
		} else if (imod == 0x3) {
			cpsr |= CPSR_IRQ_DISABLED;
		}
		mask |= CPSR_IRQ_DISABLED;
	}
	if (ARM_INST_BIT(inst, ARM_HYPERCALL_CPS_F_START)) {
		if (imod == 0x2) {
			cpsr &= ~CPSR_FIQ_DISABLED;
		} else if (imod == 0x3) {
			cpsr |= CPSR_FIQ_DISABLED;
		}
		mask |= CPSR_FIQ_DISABLED;
	}
	cpu_vcpu_cpsr_update(vcpu, regs, cpsr, mask);
	regs->pc += 4;
	return VMM_OK;
}

/** Emulate 'mrs' hypercall */
static int arm_hypercall_mrs(u32 inst,
			     arch_regs_t *regs, struct vmm_vcpu *vcpu)
{
	register u32 Rd;
	Rd = ARM_INST_BITS(inst,
			   ARM_HYPERCALL_MRS_RD_END,
			   ARM_HYPERCALL_MRS_RD_START);
	if (Rd == 15) {
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	}
	if (ARM_INST_BIT(inst, ARM_HYPERCALL_MRS_R_START)) {
		cpu_vcpu_reg_write(vcpu, regs, Rd,
				   cpu_vcpu_spsr_retrieve(vcpu));
	} else {
		cpu_vcpu_reg_write(vcpu, regs, Rd,
				   cpu_vcpu_cpsr_retrieve(vcpu, regs));
	}
	regs->pc += 4;
	return VMM_OK;
}

/** Emulate 'msr_i' hypercall */
static int arm_hypercall_msr_i(u32 inst,
			       arch_regs_t *regs, struct vmm_vcpu *vcpu)
{
	register u32 mask, imm12, psr;
	mask = ARM_INST_BITS(inst,
			     ARM_HYPERCALL_MSR_I_MASK_END,
			     ARM_HYPERCALL_MSR_I_MASK_START);
	imm12 = ARM_INST_BITS(inst,
			      ARM_HYPERCALL_MSR_I_IMM12_END,
			      ARM_HYPERCALL_MSR_I_IMM12_START);
	if (!mask) {
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	}
	mask |= (mask & 0x8) ? 0xFF000000 : 0x00;
	mask |= (mask & 0x4) ? 0xFF0000 : 0x00;
	mask |= (mask & 0x2) ? 0xFF00 : 0x00;
	mask |= (mask & 0x1) ? 0xFF : 0x00;
	psr = arm_expand_imm(regs, imm12);
	psr &= mask;
	if (ARM_INST_BIT(inst, ARM_HYPERCALL_MSR_I_R_START)) {
		cpu_vcpu_spsr_update(vcpu, psr, mask);
	} else {
		cpu_vcpu_cpsr_update(vcpu, regs, psr, mask);
	}
	regs->pc += 4;
	return VMM_OK;
}

/** Emulate 'msr_r' hypercall */
static int arm_hypercall_msr_r(u32 inst,
			       arch_regs_t *regs, struct vmm_vcpu *vcpu)
{
	register u32 mask, Rn, psr;
	mask = ARM_INST_BITS(inst,
			     ARM_HYPERCALL_MSR_R_MASK_END,
			     ARM_HYPERCALL_MSR_R_MASK_START);
	Rn = ARM_INST_BITS(inst,
			   ARM_HYPERCALL_MSR_R_RN_END,
			   ARM_HYPERCALL_MSR_R_RN_START);
	if (!mask || (Rn == 15)) {
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	}
	mask |= (mask & 0x8) ? 0xFF000000 : 0x00;
	mask |= (mask & 0x4) ? 0xFF0000 : 0x00;
	mask |= (mask & 0x2) ? 0xFF00 : 0x00;
	mask |= (mask & 0x1) ? 0xFF : 0x00;
	psr = cpu_vcpu_reg_read(vcpu, regs, Rn);
	psr &= mask;
	if (ARM_INST_BIT(inst, ARM_HYPERCALL_MSR_R_R_START)) {
		cpu_vcpu_spsr_update(vcpu, psr, mask);
	} else {
		cpu_vcpu_cpsr_update(vcpu, regs, psr, mask);
	}
	regs->pc += 4;
	return VMM_OK;
}

/** Emulate 'rfe' hypercall */
static int arm_hypercall_rfe(u32 inst,
			     arch_regs_t *regs, struct vmm_vcpu *vcpu)
{
	u32 data;
	register int rc;
	register u32 Rn, P, U, W;
	register u32 address;
	Rn = ARM_INST_BITS(inst,
			   ARM_HYPERCALL_RFE_RN_END,
			   ARM_HYPERCALL_RFE_RN_START);
	if (Rn == 15) {
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	}
	P = ARM_INST_BIT(inst, ARM_HYPERCALL_RFE_P_START);
	U = ARM_INST_BIT(inst, ARM_HYPERCALL_RFE_U_START);
	W = ARM_INST_BIT(inst, ARM_HYPERCALL_RFE_W_START);
	switch (arm_priv(vcpu)->cpsr & CPSR_MODE_MASK) {
	case CPSR_MODE_FIQ:
		vmm_vcpu_irq_deassert(vcpu, CPU_EXTERNAL_FIQ);
		break;
	case CPSR_MODE_IRQ:
		vmm_vcpu_irq_deassert(vcpu, CPU_EXTERNAL_IRQ);
		break;
	case CPSR_MODE_SYSTEM: /* fall-through */
	case CPSR_MODE_SUPERVISOR:
		vmm_vcpu_irq_deassert(vcpu, CPU_SOFT_IRQ);
		break;
	case CPSR_MODE_ABORT:
		vmm_vcpu_irq_deassert(vcpu, CPU_PREFETCH_ABORT_IRQ);
		vmm_vcpu_irq_deassert(vcpu, CPU_DATA_ABORT_IRQ);
		break;
	case CPSR_MODE_UNDEFINED:
		vmm_vcpu_irq_deassert(vcpu, CPU_UNDEF_INST_IRQ);
		break;
	default:
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	};
	address = cpu_vcpu_reg_read(vcpu, regs, Rn);
	address = (U == 1) ? address : (address - 8);
	address = (P == U) ? (address + 4) : address;
	data = 0x0;
	if ((rc = cpu_vcpu_mem_read(vcpu, regs, address + 4,
					 &data, 4, FALSE))) {
		return rc;
	}
	cpu_vcpu_cpsr_update(vcpu, regs, data, CPSR_ALLBITS_MASK);
	data = 0x0;
	if ((rc = cpu_vcpu_mem_read(vcpu, regs, address,
					 &data, 4, FALSE))) {
		return rc;
	}
	regs->pc = data;
	if (W == 1) {
		address = cpu_vcpu_reg_read(vcpu, regs, Rn);
		address = (U == 1) ? (address + 8) : (address - 8);
		cpu_vcpu_reg_write(vcpu, regs, Rn, address);
	}
	return VMM_OK;
}

/** Emulate 'srs' hypercall */
static int arm_hypercall_srs(u32 inst,
			     arch_regs_t *regs, struct vmm_vcpu *vcpu)
{
	u32 data;
	register int rc;
	register u32 P, U, W, mode;
	register u32 cpsr, base, address;
	P = ARM_INST_BIT(inst, ARM_HYPERCALL_SRS_P_START);
	U = ARM_INST_BIT(inst, ARM_HYPERCALL_SRS_U_START);
	W = ARM_INST_BIT(inst, ARM_HYPERCALL_SRS_W_START);
	mode = ARM_INST_BITS(inst,
		     ARM_HYPERCALL_SRS_MODE_END,
		     ARM_HYPERCALL_SRS_MODE_START);
	cpsr = arm_priv(vcpu)->cpsr & CPSR_MODE_MASK;
	if ((cpsr == CPSR_MODE_USER) ||
	    (cpsr == CPSR_MODE_SYSTEM)) {
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	}
	base = cpu_vcpu_regmode_read(vcpu, regs, mode, 13);
	address = (U == 1) ? base : (base - 8);
	address = (P == U) ? (address + 4) : address;
	data = regs->lr;
	if ((rc = cpu_vcpu_mem_write(vcpu, regs, address,
					  &data, 4, FALSE))) {
		return rc;
	}
	address += 4;
	data = cpu_vcpu_spsr_retrieve(vcpu);
	if ((rc = cpu_vcpu_mem_write(vcpu, regs, address,
					  &data, 4, FALSE))) {
		return rc;
	}
	if (W == 1) {
		address = (U == 1) ? (base + 8) : (base - 8);
		cpu_vcpu_regmode_write(vcpu, regs, mode, 13, address);
	}
	regs->pc += 4;
	return VMM_OK;
}

/** Emulate 'wfi' hypercall */
static int arm_hypercall_wfi(u32 inst,
			     arch_regs_t *regs, struct vmm_vcpu *vcpu)
{
	/* Wait for irq on this vcpu */
	vmm_vcpu_irq_wait(vcpu);
	regs->pc += 4;
	return VMM_OK;
}

/** Emulate 'wfe' hypercall */
static int arm_hypercall_wfe(u32 inst,
			     arch_regs_t *regs, struct vmm_vcpu *vcpu)
{
	/* give up the cpu */
	vmm_scheduler_yield();
	regs->pc += 4;
	return VMM_OK;
}

/** Emulate 'yield' hypercall */
static int arm_hypercall_yield(u32 inst,
			       arch_regs_t *regs, struct vmm_vcpu *vcpu)
{
	/* give up the cpu */
	vmm_scheduler_yield();
	regs->pc += 4;
	return VMM_OK;
}

static int arm_hypercall_unused(u32 inst,
				arch_regs_t *regs, struct vmm_vcpu *vcpu)
{
	return VMM_EFAIL;
}

static int (* const wfx_funcs[]) (u32 inst,
				arch_regs_t *regs, struct vmm_vcpu *vcpu) =
{
	arm_hypercall_wfi,	/* ARM_HYPERCALL_WFI_SUBID */
	arm_hypercall_wfe,	/* ARM_HYPERCALL_WFE_SUBID */
	arm_hypercall_yield,	/* ARM_HYPERCALL_YIELD_SUBID */
	arm_hypercall_unused	/* not used yet */
};

/** Emulate 'wfi', 'wfe', 'sev', 'yield' hypercall */
static int arm_hypercall_wfx(u32 inst,
			     arch_regs_t *regs, struct vmm_vcpu *vcpu)
{
	u32 subid = ARM_INST_DECODE(inst,
			ARM_INST_HYPERCALL_WFX_MASK,
			ARM_INST_HYPERCALL_WFX_SHIFT);

	return wfx_funcs[subid] (inst, regs, vcpu);
}

/** Emulate 'smc' hypercall */
static int arm_hypercall_smc(u32 inst,
			     arch_regs_t *regs, struct vmm_vcpu *vcpu)
{
	int rc;

	/* Treat this as PSCI call and emulate it */
	rc = emulate_psci_call(vcpu, regs, TRUE);
	if (rc) {
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	} else {
		regs->pc += 4;
	}

	return VMM_OK;
}

/** Emulate 'ldm_ue' hypercall */
static int arm_hypercall_ldm_ue(u32 id, u32 inst,
				arch_regs_t *regs, struct vmm_vcpu *vcpu)
{
	u32 ndata[16];
	register int rc;
	register u32 Rn, U, P, W, reg_list;
	register u32 address, i, pos, length;
	Rn = ARM_INST_BITS(inst,
			   ARM_HYPERCALL_LDM_UE_RN_END,
			   ARM_HYPERCALL_LDM_UE_RN_START);
	P = ((id - ARM_HYPERCALL_LDM_UE_ID0) & 0x4) >> 2;
	U = ((id - ARM_HYPERCALL_LDM_UE_ID0) & 0x2) >> 1;
	W = ((id - ARM_HYPERCALL_LDM_UE_ID0) & 0x1);
	reg_list = ARM_INST_BITS(inst,
				 ARM_HYPERCALL_LDM_UE_REGLIST_END,
				 ARM_HYPERCALL_LDM_UE_REGLIST_START);
	if (Rn == 15) {
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	}
	if (reg_list & 0x8000) {
		/* LDM (Exception Return) */
		if ((W == 1) && (reg_list & (0x1 << Rn))) {
			arm_unpredictable(regs, vcpu, inst, __func__);
			return VMM_EFAIL;
		}
		switch (arm_priv(vcpu)->cpsr & CPSR_MODE_MASK) {
		case CPSR_MODE_FIQ:
			vmm_vcpu_irq_deassert(vcpu, CPU_EXTERNAL_FIQ);
			break;
		case CPSR_MODE_IRQ:
			vmm_vcpu_irq_deassert(vcpu, CPU_EXTERNAL_IRQ);
			break;
		case CPSR_MODE_SUPERVISOR:
			vmm_vcpu_irq_deassert(vcpu, CPU_SOFT_IRQ);
			break;
		case CPSR_MODE_ABORT:
			vmm_vcpu_irq_deassert(vcpu, CPU_PREFETCH_ABORT_IRQ);
			vmm_vcpu_irq_deassert(vcpu, CPU_DATA_ABORT_IRQ);
			break;
		case CPSR_MODE_UNDEFINED:
			vmm_vcpu_irq_deassert(vcpu, CPU_UNDEF_INST_IRQ);
			break;
		default:
			arm_unpredictable(regs, vcpu, inst, __func__);
			return VMM_EFAIL;
		};
		length = 4;
		for (i = 0; i < 15; i++) {
			if (reg_list & (0x1 << i)) {
				length += 4;
			}
		}
		address = cpu_vcpu_reg_read(vcpu, regs, Rn);
		address = (U == 1) ? address : address - length;
		address = (P == U) ? (address + 4) : address;
		if (((address + length - 4) & ~TTBL_MIN_PAGE_MASK) !=
			(address & ~TTBL_MIN_PAGE_MASK)) {
			pos = TTBL_MIN_PAGE_SIZE -
					(address & TTBL_MIN_PAGE_MASK);
			if ((rc = cpu_vcpu_mem_read(vcpu, regs,
				address, &ndata, pos, FALSE))) {
				return rc;
			}
			if ((rc = cpu_vcpu_mem_read(vcpu, regs,
				address + pos, &ndata[pos >> 2],
				length - pos, FALSE))) {
				return rc;
			}
		} else {
			if ((rc = cpu_vcpu_mem_read(vcpu, regs,
				address, &ndata, length, FALSE))) {
				return rc;
			}
		}
		address = address + length - 4;
		pos = 0;
		for (i = 0; i < 15; i++) {
			if (reg_list & (0x1 << i)) {
				cpu_vcpu_reg_write(vcpu, regs,
						   i, ndata[pos]);
				pos++;
			}
		}
		if ((W == 1) && !(reg_list & (0x1 << Rn))) {
			address = cpu_vcpu_reg_read(vcpu, regs, Rn);
			address = (U == 1) ? address + length :
						address - length;
			cpu_vcpu_reg_write(vcpu, regs, Rn, address);
		}
		cpu_vcpu_cpsr_update(vcpu, regs,
				     cpu_vcpu_spsr_retrieve(vcpu),
				     CPSR_ALLBITS_MASK);
		regs->pc = ndata[pos];
	} else {
		/* LDM (User Registers) */
		if ((W == 1) || !reg_list) {
			arm_unpredictable(regs, vcpu, inst, __func__);
			return VMM_EFAIL;
		}
		i = arm_priv(vcpu)->cpsr & CPSR_MODE_MASK;
		if ((i == CPSR_MODE_USER) || (i == CPSR_MODE_SYSTEM)) {
			arm_unpredictable(regs, vcpu, inst, __func__);
			return VMM_EFAIL;
		}
		length = 0;
		for (i = 0; i < 15; i++) {
			if (reg_list & (0x1 << i)) {
				length += 4;
			}
		}
		address = cpu_vcpu_reg_read(vcpu, regs, Rn);
		address = (U == 1) ? address : address - length;
		address = (P == U) ? (address + 4) : address;
		if (((address + length - 4) & ~TTBL_MIN_PAGE_MASK) !=
			(address & ~TTBL_MIN_PAGE_MASK)) {
			pos = TTBL_MIN_PAGE_SIZE -
					(address & TTBL_MIN_PAGE_MASK);
			if ((rc = cpu_vcpu_mem_read(vcpu, regs,
				address, &ndata, pos, FALSE))) {
				return rc;
			}
			if ((rc = cpu_vcpu_mem_read(vcpu, regs,
				address + pos, &ndata[pos >> 2],
				length - pos, FALSE))) {
				return rc;
			}
		} else {
			if ((rc = cpu_vcpu_mem_read(vcpu, regs,
				address, &ndata, length, FALSE))) {
				return rc;
			}
		}
		pos = 0;
		for (i = 0; i < 15; i++) {
			if (reg_list & (0x1 << i)) {
				cpu_vcpu_regmode_write(vcpu, regs,
							CPSR_MODE_USER,
							i, ndata[pos]);
				pos++;
			}
		}
		regs->pc += 4;
	}
	return VMM_OK;
}

/** Emulate 'stm_u' hypercall */
static int arm_hypercall_stm_u(u32 id, u32 inst,
			       arch_regs_t *regs, struct vmm_vcpu *vcpu)
{
	u32 ndata[16];
	register int rc;
	register u32 Rn, P, U, reg_list;
	register u32 i, pos, length, address;
	Rn = ARM_INST_BITS(inst,
			   ARM_HYPERCALL_STM_U_RN_END,
			   ARM_HYPERCALL_STM_U_RN_START);
	reg_list = ARM_INST_BITS(inst,
				 ARM_HYPERCALL_STM_U_REGLIST_END,
				 ARM_HYPERCALL_STM_U_REGLIST_START);
	if ((Rn == 15) || !reg_list) {
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	}
	P = ((id - ARM_HYPERCALL_STM_U_ID0) & 0x2) >> 1;
	U = ((id - ARM_HYPERCALL_STM_U_ID0) & 0x1);
	i = arm_priv(vcpu)->cpsr & CPSR_MODE_MASK;
	if ((i == CPSR_MODE_USER) || (i == CPSR_MODE_SYSTEM)) {
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	}
	length = 0;
	for (i = 0; i < 16; i++) {
		if (reg_list & (0x1 << i)) {
			length += 4;
		}
	}
	address = cpu_vcpu_reg_read(vcpu, regs, Rn);
	address = (U == 1) ? address : address - length;
	address = (P == U) ? address + 4 : address;
	pos = 0;
	for (i = 0; i < 16; i++) {
		if (reg_list & (0x1 << i)) {
			ndata[pos] = cpu_vcpu_regmode_read(vcpu, regs,
							CPSR_MODE_USER, i);
			pos++;
		}
	}
	if (((address + length - 4) & ~TTBL_MIN_PAGE_MASK) !=
				(address & ~TTBL_MIN_PAGE_MASK)) {
		pos = TTBL_MIN_PAGE_SIZE -
				(address & TTBL_MIN_PAGE_MASK);
		if ((rc = cpu_vcpu_mem_write(vcpu, regs,
			address, &ndata, pos, FALSE))) {
			return rc;
		}
		if ((rc = cpu_vcpu_mem_write(vcpu, regs,
			address + pos, &ndata[pos >> 2],
			length - pos, FALSE))) {
			return rc;
		}
	} else {
		if ((rc = cpu_vcpu_mem_write(vcpu, regs,
			address, &ndata, length, FALSE))) {
			return rc;
		}
	}
	regs->pc += 4;
	return VMM_OK;
}

/** Emulate 'subs_rel' hypercall */
static int arm_hypercall_subs_rel(u32 id, u32 inst,
				  arch_regs_t *regs, struct vmm_vcpu *vcpu)
{
	u32 shift_t;
	register u32 opcode, Rn, imm, type, Rm;
	register u32 operand2, result;
	opcode = ARM_INST_BITS(inst,
				ARM_HYPERCALL_SUBS_REL_OPCODE_END,
				ARM_HYPERCALL_SUBS_REL_OPCODE_START);
	Rn = ARM_INST_BITS(inst,
			   ARM_HYPERCALL_SUBS_REL_RN_END,
			   ARM_HYPERCALL_SUBS_REL_RN_START);
	switch (arm_priv(vcpu)->cpsr & CPSR_MODE_MASK) {
	case CPSR_MODE_FIQ:
		vmm_vcpu_irq_deassert(vcpu, CPU_EXTERNAL_FIQ);
		break;
	case CPSR_MODE_IRQ:
		vmm_vcpu_irq_deassert(vcpu, CPU_EXTERNAL_IRQ);
		break;
	case CPSR_MODE_SYSTEM: /* fall-through */
	case CPSR_MODE_SUPERVISOR:
		vmm_vcpu_irq_deassert(vcpu, CPU_SOFT_IRQ);
		break;
	case CPSR_MODE_ABORT:
		vmm_vcpu_irq_deassert(vcpu, CPU_PREFETCH_ABORT_IRQ);
		vmm_vcpu_irq_deassert(vcpu, CPU_DATA_ABORT_IRQ);
		break;
	case CPSR_MODE_UNDEFINED:
		vmm_vcpu_irq_deassert(vcpu, CPU_UNDEF_INST_IRQ);
		break;
	default:
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
	};
	if (id == ARM_HYPERCALL_SUBS_REL_ID0) {
		/* Register form */
		imm = ARM_INST_BITS(inst,
				    ARM_HYPERCALL_SUBS_REL_IMM5_END,
				    ARM_HYPERCALL_SUBS_REL_IMM5_START);
		type = ARM_INST_BITS(inst,
				     ARM_HYPERCALL_SUBS_REL_TYPE_END,
				     ARM_HYPERCALL_SUBS_REL_TYPE_START);
		Rm = ARM_INST_BITS(inst,
				   ARM_HYPERCALL_SUBS_REL_RM_END,
				   ARM_HYPERCALL_SUBS_REL_RM_START);
		type = arm_decode_imm_shift(type, imm, &shift_t);
		operand2 = cpu_vcpu_reg_read(vcpu, regs, Rm);
		operand2 = arm_shift(operand2, shift_t, type,
				 (regs->cpsr & CPSR_CARRY_MASK) >>
				 CPSR_CARRY_SHIFT);
	} else {
		/* Immediate value */
		imm = ARM_INST_BITS(inst,
		      ARM_HYPERCALL_SUBS_REL_IMM12_END,
		      ARM_HYPERCALL_SUBS_REL_IMM12_START);
		operand2 = arm_expand_imm(regs, imm);
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
		arm_unpredictable(regs, vcpu, inst, __func__);
		return VMM_EFAIL;
		break;
	};
	cpu_vcpu_cpsr_update(vcpu, regs,
			     cpu_vcpu_spsr_retrieve(vcpu),
			     CPSR_ALLBITS_MASK);
	regs->pc = result;
	return VMM_OK;
}

static int (* const cps_and_co_funcs[]) (u32 inst,
				arch_regs_t *regs, struct vmm_vcpu *vcpu) =
{
	arm_hypercall_cps,	/* ARM_HYPERCALL_CPS_SUBID */
	arm_hypercall_mrs,	/* ARM_HYPERCALL_MRS_SUBID */
	arm_hypercall_msr_i,	/* ARM_HYPERCALL_MSR_I_SUBID */
	arm_hypercall_msr_r,	/* ARM_HYPERCALL_MSR_R_SUBID */
	arm_hypercall_rfe,	/* ARM_HYPERCALL_RFE_SUBID */
	arm_hypercall_srs,	/* ARM_HYPERCALL_SRS_SUBID */
	arm_hypercall_wfx,	/* ARM_HYPERCALL_WFI_SUBID */
	arm_hypercall_smc	/* ARM_HYPERCALL_SMC_SUBID */
};

static int arm_hypercall_cps_and_co(u32 id, u32 inst,
				    arch_regs_t *regs, struct vmm_vcpu *vcpu)
{
	u32 subid = ARM_INST_DECODE(inst,
			ARM_INST_HYPERCALL_SUBID_MASK,
			ARM_INST_HYPERCALL_SUBID_SHIFT);

	return cps_and_co_funcs[subid] (inst, regs, vcpu);
}

static int arm_hypercall_id(u32 id, u32 inst,
			    arch_regs_t *regs, struct vmm_vcpu *vcpu)
{
	return VMM_EFAIL;
}

static int (* const hcall_funcs[]) (u32 id, u32 inst,
				arch_regs_t *regs, struct vmm_vcpu *vcpu) =
{
	arm_hypercall_cps_and_co,	/* ARM_HYPERCALL_CPS_ID */
	arm_hypercall_ldm_ue,		/* ARM_HYPERCALL_LDM_UE_ID0 */
	arm_hypercall_ldm_ue,		/* ARM_HYPERCALL_LDM_UE_ID1 */
	arm_hypercall_ldm_ue,		/* ARM_HYPERCALL_LDM_UE_ID2 */
	arm_hypercall_ldm_ue,		/* ARM_HYPERCALL_LDM_UE_ID3 */
	arm_hypercall_ldm_ue,		/* ARM_HYPERCALL_LDM_UE_ID4 */
	arm_hypercall_ldm_ue,		/* ARM_HYPERCALL_LDM_UE_ID5 */
	arm_hypercall_ldm_ue,		/* ARM_HYPERCALL_LDM_UE_ID6 */
	arm_hypercall_ldm_ue,		/* ARM_HYPERCALL_LDM_UE_ID7 */
	arm_hypercall_stm_u,		/* ARM_HYPERCALL_STM_U_ID0 */
	arm_hypercall_stm_u,		/* ARM_HYPERCALL_STM_U_ID1 */
	arm_hypercall_stm_u,		/* ARM_HYPERCALL_STM_U_ID2 */
	arm_hypercall_stm_u,		/* ARM_HYPERCALL_STM_U_ID3 */
	arm_hypercall_subs_rel,		/* ARM_HYPERCALL_SUBS_REL_ID0 */
	arm_hypercall_subs_rel,		/* ARM_HYPERCALL_SUBS_REL_ID1 */
	arm_hypercall_id		/* not used yet */
};

int cpu_vcpu_hypercall_arm(struct vmm_vcpu *vcpu,
			   arch_regs_t *regs, u32 inst)
{
	u32 id = ARM_INST_DECODE(inst,
			     ARM_INST_HYPERCALL_ID_MASK,
			     ARM_INST_HYPERCALL_ID_SHIFT);

	return hcall_funcs[id] (id, inst, regs, vcpu);
}

