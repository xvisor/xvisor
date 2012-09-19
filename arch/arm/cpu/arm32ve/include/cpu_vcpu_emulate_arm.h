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
 * @file cpu_vcpu_emulate_arm.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Inferace for non-hardware assisted ARM instruction emulation
 */
#ifndef _CPU_VCPU_EMULATE_ARM_H__
#define _CPU_VCPU_EMULATE_ARM_H__

#include <vmm_types.h>
#include <vmm_manager.h>

enum arm_shift_type {
	arm_shift_lsl,
	arm_shift_lsr,
	arm_shift_asr,
	arm_shift_ror,
	arm_shift_rrx
};

#define ARM_INST_BIT(inst, start)		(((inst) >> (start)) & 0x1)
#define ARM_INST_BITS(inst, end, start)		(((inst) << (31-(end))) >> (31+(start)-(end)))
#define ARM_INST_DECODE(inst, mask, shift)	(((inst) & (mask)) >> (shift))
#define ARM_INST_ENCODE(val, mask, shift)	(((val) << (shift)) & (mask))

#define ARM_INST_LDRSTR_REGFORM2_END		25
#define ARM_INST_LDRSTR_REGFORM2_START		25
#define ARM_INST_LDRSTR_P_END			24
#define ARM_INST_LDRSTR_P_START			24
#define ARM_INST_LDRSTR_U_END			23
#define ARM_INST_LDRSTR_U_START			23
#define ARM_INST_LDRSTR_REGFORM1_END		22
#define ARM_INST_LDRSTR_REGFORM1_START		22
#define ARM_INST_LDRSTR_W_END			21
#define ARM_INST_LDRSTR_W_START			21
#define ARM_INST_LDRSTR_RN_END			19
#define ARM_INST_LDRSTR_RN_START		16
#define ARM_INST_LDRSTR_RT_END			15
#define ARM_INST_LDRSTR_RT_START		12
#define ARM_INST_LDRSTR_IMM12_END		11
#define ARM_INST_LDRSTR_IMM12_START		0
#define ARM_INST_LDRSTR_IMM5_END		11
#define ARM_INST_LDRSTR_IMM5_START		7
#define ARM_INST_LDRSTR_TYPE_END		6
#define ARM_INST_LDRSTR_TYPE_START		5
#define ARM_INST_LDRSTR_IMM4H_END		11
#define ARM_INST_LDRSTR_IMM4H_START		8
#define ARM_INST_LDRSTR_IMM4L_END		3
#define ARM_INST_LDRSTR_IMM4L_START		0
#define ARM_INST_LDRSTR_RM_END			3
#define ARM_INST_LDRSTR_RM_START		0

#define ARM_INST_LDRSTR_A_MASK			0x02000000
#define ARM_INST_LDRSTR_A_SHIFT			25
#define ARM_INST_LDRSTR_OP1_MASK		0x01F00000
#define ARM_INST_LDRSTR_OP1_SHIFT		20
#define ARM_INST_LDRSTR_RN_MASK			0x000F0000
#define ARM_INST_LDRSTR_RN_SHIFT		16
#define ARM_INST_LDRSTR_B_MASK			0x00000010
#define ARM_INST_LDRSTR_B_SHIFT			4

#define ARM_INST_COND_MASK			0xF0000000
#define ARM_INST_COND_SHIFT			28
#define ARM_INST_OP1_MASK			0x0E000000
#define ARM_INST_OP1_SHIFT			25
#define ARM_INST_OP_MASK			0x00000010
#define ARM_INST_OP_SHIFT			4

/** Emulate ARM instruction (no HW help) */
int cpu_vcpu_emulate_arm_inst(struct vmm_vcpu *vcpu, 
			      arch_regs_t *regs, bool is_hypercall);

#endif
