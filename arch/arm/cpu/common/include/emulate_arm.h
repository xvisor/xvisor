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
 * @file emulate_arm.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file to emulate ARM instructions
 */
#ifndef _EMULATE_ARM_H__
#define _EMULATE_ARM_H__

#include <vmm_types.h>

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

#define ARM_INST_DATAPROC_OP_MASK		0x02000000
#define ARM_INST_DATAPROC_OP_SHIFT		25
#define ARM_INST_DATAPROC_OP1_MASK		0x01F00000
#define ARM_INST_DATAPROC_OP1_SHIFT		20
#define ARM_INST_DATAPROC_RN_MASK		0x000F0000
#define ARM_INST_DATAPROC_RN_SHIFT		16
#define ARM_INST_DATAPROC_OP2_MASK		0x000000F0
#define ARM_INST_DATAPROC_OP2_SHIFT		4

#define ARM_INST_LDRSTR_A_MASK			0x02000000
#define ARM_INST_LDRSTR_A_SHIFT			25
#define ARM_INST_LDRSTR_OP1_MASK		0x01F00000
#define ARM_INST_LDRSTR_OP1_SHIFT		20
#define ARM_INST_LDRSTR_RN_MASK			0x000F0000
#define ARM_INST_LDRSTR_RN_SHIFT		16
#define ARM_INST_LDRSTR_B_MASK			0x00000010
#define ARM_INST_LDRSTR_B_SHIFT			4

#define ARM_INST_LDMSTM_W_END			21
#define ARM_INST_LDMSTM_W_START			21
#define ARM_INST_LDMSTM_RN_END			19
#define ARM_INST_LDMSTM_RN_START		16
#define ARM_INST_LDMSTM_REGLIST_END		15
#define ARM_INST_LDMSTM_REGLIST_START		0

#define ARM_INST_MOVW_I_IMM4_END		19
#define ARM_INST_MOVW_I_IMM4_START		16
#define ARM_INST_MOVW_I_RD_END			15
#define ARM_INST_MOVW_I_RD_START		12
#define ARM_INST_MOVW_I_IMM12_END		11
#define ARM_INST_MOVW_I_IMM12_START		0

#define ARM_INST_BRBLK_OP_END			25
#define ARM_INST_BRBLK_OP_START			20

#define ARM_INST_STCX_P_END			24
#define ARM_INST_STCX_P_START			24
#define ARM_INST_STCX_U_END			23
#define ARM_INST_STCX_U_START			23
#define ARM_INST_STCX_D_END			22
#define ARM_INST_STCX_D_START			22
#define ARM_INST_STCX_W_END			21
#define ARM_INST_STCX_W_START			21
#define ARM_INST_STCX_RN_END			19
#define ARM_INST_STCX_RN_START			16
#define ARM_INST_STCX_CRD_END			15
#define ARM_INST_STCX_CRD_START			12
#define ARM_INST_STCX_COPROC_END		11
#define ARM_INST_STCX_COPROC_START		8
#define ARM_INST_STCX_IMM8_END			7
#define ARM_INST_STCX_IMM8_START		0

#define ARM_INST_LDCX_I_P_END			24
#define ARM_INST_LDCX_I_P_START			24
#define ARM_INST_LDCX_I_U_END			23
#define ARM_INST_LDCX_I_U_START			23
#define ARM_INST_LDCX_I_D_END			22
#define ARM_INST_LDCX_I_D_START			22
#define ARM_INST_LDCX_I_W_END			21
#define ARM_INST_LDCX_I_W_START			21
#define ARM_INST_LDCX_I_RN_END			19
#define ARM_INST_LDCX_I_RN_START		16
#define ARM_INST_LDCX_I_CRD_END			15
#define ARM_INST_LDCX_I_CRD_START		12
#define ARM_INST_LDCX_I_COPROC_END		11
#define ARM_INST_LDCX_I_COPROC_START		8
#define ARM_INST_LDCX_I_IMM8_END		7
#define ARM_INST_LDCX_I_IMM8_START		0

#define ARM_INST_LDCX_L_P_END			24
#define ARM_INST_LDCX_L_P_START			24
#define ARM_INST_LDCX_L_U_END			23
#define ARM_INST_LDCX_L_U_START			23
#define ARM_INST_LDCX_L_D_END			22
#define ARM_INST_LDCX_L_D_START			22
#define ARM_INST_LDCX_L_W_END			21
#define ARM_INST_LDCX_L_W_START			21
#define ARM_INST_LDCX_L_CRD_END			15
#define ARM_INST_LDCX_L_CRD_START		12
#define ARM_INST_LDCX_L_COPROC_END		11
#define ARM_INST_LDCX_L_COPROC_START		8
#define ARM_INST_LDCX_L_IMM8_END		7
#define ARM_INST_LDCX_L_IMM8_START		0

#define ARM_INST_MCRRX_RT2_END			19
#define ARM_INST_MCRRX_RT2_START		16
#define ARM_INST_MCRRX_RT_END			15
#define ARM_INST_MCRRX_RT_START			12
#define ARM_INST_MCRRX_COPROC_END		11
#define ARM_INST_MCRRX_COPROC_START		8
#define ARM_INST_MCRRX_OPC1_END			7
#define ARM_INST_MCRRX_OPC1_START		4
#define ARM_INST_MCRRX_CRM_END			3
#define ARM_INST_MCRRX_CRM_START		0

#define ARM_INST_MRRCX_RT2_END			19
#define ARM_INST_MRRCX_RT2_START		16
#define ARM_INST_MRRCX_RT_END			15
#define ARM_INST_MRRCX_RT_START			12
#define ARM_INST_MRRCX_COPROC_END		11
#define ARM_INST_MRRCX_COPROC_START		8
#define ARM_INST_MRRCX_OPC1_END			7
#define ARM_INST_MRRCX_OPC1_START		4
#define ARM_INST_MRRCX_CRM_END			3
#define ARM_INST_MRRCX_CRM_START		0

#define ARM_INST_CDPX_OPC1_END			23
#define ARM_INST_CDPX_OPC1_START		20
#define ARM_INST_CDPX_CRN_END			19
#define ARM_INST_CDPX_CRN_START			16
#define ARM_INST_CDPX_CRD_END			15
#define ARM_INST_CDPX_CRD_START			12
#define ARM_INST_CDPX_COPROC_END		11
#define ARM_INST_CDPX_COPROC_START		8
#define ARM_INST_CDPX_OPC2_END			7
#define ARM_INST_CDPX_OPC2_START		5
#define ARM_INST_CDPX_CRM_END			3
#define ARM_INST_CDPX_CRM_START			0

#define ARM_INST_MCRX_OPC1_END			23
#define ARM_INST_MCRX_OPC1_START		21
#define ARM_INST_MCRX_CRN_END			19
#define ARM_INST_MCRX_CRN_START			16
#define ARM_INST_MCRX_RT_END			15
#define ARM_INST_MCRX_RT_START			12
#define ARM_INST_MCRX_COPROC_END		11
#define ARM_INST_MCRX_COPROC_START		8
#define ARM_INST_MCRX_OPC2_END			7
#define ARM_INST_MCRX_OPC2_START		5
#define ARM_INST_MCRX_CRM_END			3
#define ARM_INST_MCRX_CRM_START			0

#define ARM_INST_MRCX_OPC1_END			23
#define ARM_INST_MRCX_OPC1_START		21
#define ARM_INST_MRCX_CRN_END			19
#define ARM_INST_MRCX_CRN_START			16
#define ARM_INST_MRCX_RT_END			15
#define ARM_INST_MRCX_RT_START			12
#define ARM_INST_MRCX_COPROC_END		11
#define ARM_INST_MRCX_COPROC_START		8
#define ARM_INST_MRCX_OPC2_END			7
#define ARM_INST_MRCX_OPC2_START		5
#define ARM_INST_MRCX_CRM_END			3
#define ARM_INST_MRCX_CRM_START			0

#define ARM_INST_COPROC_OP1_MASK		0x03F00000
#define ARM_INST_COPROC_OP1_SHIFT		20
#define ARM_INST_COPROC_RN_MASK			0x000F0000
#define ARM_INST_COPROC_RN_SHIFT		16
#define ARM_INST_COPROC_CPRO_MASK		0x00000F00
#define ARM_INST_COPROC_CPRO_SHIFT		8
#define ARM_INST_COPROC_OP_MASK			0x00000010
#define ARM_INST_COPROC_OP_SHIFT		4

#define ARM_INST_COND_MASK			0xF0000000
#define ARM_INST_COND_SHIFT			28
#define ARM_INST_OP1_MASK			0x0E000000
#define ARM_INST_OP1_SHIFT			25
#define ARM_INST_OP_MASK			0x00000010
#define ARM_INST_OP_SHIFT			4

#define arm_zero_extend(imm, bits)		((u32)(imm))
#define arm_align(addr, nbytes)			((addr) - ((addr) % (nbytes)))

void arm_unpredictable(arch_regs_t *regs, struct vmm_vcpu *vcpu, 
			u32 inst, const char *reason);

u32 arm_sign_extend(u32 imm, u32 len, u32 bits);

bool arm_condition_check(u32 cond, arch_regs_t *regs);

#define arm_condition_passed(cond, regs) (((cond) == 0xE) ? \
					 TRUE : \
					 arm_condition_check((cond), (regs)))

u32 arm_decode_imm_shift(u32 type, u32 imm5, u32 *shift_t);

u32 arm_shift_c(u32 val, u32 shift_t, u32 shift_n, u32 cin, u32 *cout);

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

static inline u32 arm_expand_imm(arch_regs_t *regs, u32 imm12)
{
	return arm_expand_imm_c(imm12, 
				(arm_cpsr(regs) >> CPSR_CARRY_SHIFT) & 0x1,
				NULL);
}

u32 arm_add_with_carry(u32 x, u32 y, u32 cin, u32 *cout, u32 *oout);

/** Emulate ARM instructions */
int emulate_arm_inst(struct vmm_vcpu *vcpu, arch_regs_t *regs, u32 inst);

#endif
