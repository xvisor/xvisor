/**
 * Copyright (c) 2010-2011 Himanshu Chauhan.
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
 * @file cpu_vcpu_emulate.h
 * @version 0.01
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief Priviledged instruction emulation code.
 */
#ifndef __CPU_VCPU_EMULATE_H_
#define __CPU_VCPU_EMULATE_H_

#define CE_MASK				0x30000000UL
#define CE_SHIFT			28
#define UNUSABLE_COP_ID(__cause_reg)	((__cause_reg & CE_MASK) \
					 >> CE_SHIFT)

#define BD_MASK				0x80000000UL
#define BD_SHIFT			31
#define IS_BD_SET(__cause_reg)		((__cause_reg & BD_MASK) >> BD_SHIFT)

/* Opcode extraction macros */
#define MIPS32_OPC_SHIFT		26
#define MFC0_OPC_MASK			~(0x3f << MIPS32_OPC_SHIFT)
#define MIPS32_OPCODE(_i)		(_i >> MIPS32_OPC_SHIFT)

/* Co-processor instruction  decode macros */
#define MIPS32_OPC_CP0_DIR_SHIFT	21
#define MIPS32_OPC_CP0_DIR_MASK		~(0x1F << MIPS32_OPC_CP0_DIR_SHIFT)
#define MIPS32_OPC_CP0_DIR(_i)		((_i & ~MIPS32_OPC_CP0_DIR_MASK) >> \
					 MIPS32_OPC_CP0_DIR_SHIFT)

#define MIPS32_OPC_CP0_RT_SHIFT		16
#define MIPS32_OPC_CP0_RT_MASK		~(0x1F << MIPS32_OPC_CP0_RT_SHIFT)
#define MIPS32_OPC_CP0_RT(_i)		((_i & ~MIPS32_OPC_CP0_RT_MASK) >> \
					 MIPS32_OPC_CP0_RT_SHIFT)

#define MIPS32_OPC_CP0_RD_SHIFT		11
#define MIPS32_OPC_CP0_RD_MASK		~(0x1F << MIPS32_OPC_CP0_RD_SHIFT)
#define MIPS32_OPC_CP0_RD(_i)		((_i & ~MIPS32_OPC_CP0_RD_MASK) >> \
					 MIPS32_OPC_CP0_RD_SHIFT)

#define MIPS32_OPC_CP0_SEL(_i)		(_i & 0x7)

#define MIPS32_OPC_CP0_DIEI_SC_SHIFT	5
#define MIPS32_OPC_CP0_DIEI_SC_MASK	~(0x01UL << MIPS32_OPC_CP0_DIEI_SC_SHIFT)

#define MIPS32_OPC_CP0_SC(_i)		((_i & ~MIPS32_OPC_CP0_DIEI_SC_MASK) \
					 >> MIPS32_OPC_CP0_DIEI_SC_SHIFT)

#define MIPS32_OPC_CP0_ACSS		0x10
#define MIPS32_OPC_CP0_MF		0x00
#define MIPS32_OPC_CP0_MT		0x04
#define MIPS32_OPC_CP0_DIEI		0x0B

/* TLB Access intructions and opcode macros */
#define IS_TLB_ACCESS_INST(_i)		(_i & (0x01UL << 25))
#define MIPS32_OPC_TLB_ACCESS_OPCODE(_i) (_i & 0x3FUL)

#define MIPS32_OPC_TLB_OPCODE_TLBR	0x01
#define MIPS32_OPC_TLB_OPCODE_TLBP	0x08
#define MIPS32_OPC_TLB_OPCODE_TLBWI	0x02
#define MIPS32_OPC_TLB_OPCODE_TLBWR	0x06

/* BRANCH and JUMP instruction emulation macros and defines. */
#define MIPS32_OPC_BANDJ_OPCODE_SHIFT	26
#define MIPS32_OPC_BANDJ_OPCODE_MASK	(0x3FUL << \
					 MIPS32_OPC_BANDJ_OPCODE_SHIFT)
#define MIPS32_OPC_BANDJ_OPCODE(_i)	((_i & MIPS32_OPC_BANDJ_OPCODE_MASK) \
					 >> MIPS32_OPC_BANDJ_OPCODE_SHIFT)

#define MIPS32_OPC_BANDJ_REGIMM_OPCODE_SHIFT	16
#define MIPS32_OPC_BANDJ_REGIMM_OPCODE_MASK	(0x1FUL << \
						 MIPS32_OPC_BANDJ_REGIMM_OPCODE_SHIFT)
#define MIPS32_OPC_BANDJ_REGIMM_OPCODE(_i)	((_i & MIPS32_OPC_BANDJ_REGIMM_OPCODE_MASK) \
						 >> MIPS32_OPC_BANDJ_REGIMM_OPCODE_SHIFT)

#define MIPS32_OPC_BANDJ_REGIMM_RS_SHIFT	21
#define MIPS32_OPC_BANDJ_REGIMM_RS_MASK		0x1FUL
#define MIPS32_OPC_BANDJ_REGIMM_RS(_i)		((_i & MIPS32_OPC_BANDJ_REGIMM_RS_MASK) \
						 >> MIPS32_OPC_BANDJ_REGIMM_RS_SHIFT)

#define MIPS32_OPC_BANDJ_REGIMM_OFFSET_SHIFT	0
#define MIPS32_OPC_BANDJ_REGIMM_OFFSET_MASK	0xFFFFUL
#define MIPS32_OPC_BANDJ_REGIMM_OFFSET(_i)	(_i & MIPS32_OPC_BANDJ_REGIMM_OFFSET_MASK)

#define MIPS32_OPC_BANDJ_SPECIAL_OPCODE_SHIFT	0
#define MIPS32_OPC_BANDJ_SPECIAL_OPCODE_MASK	(0x3FUL)
#define MIPS32_OPC_BANDJ_SPECIAL_OPCODE(_i)	((_i & MIPS32_OPC_BANDJ_SPECIAL_OPCODE_MASK) \
						 >> MIPS32_OPC_BANDJ_SPECIAL_OPCODE_SHIFT)

#define MIPS32_OPC_BANDJ_OPCODE_SPECIAL		0x00
#define MIPS32_OPC_BANDJ_OPCODE_REGIMM		0x01
#define MIPS32_OPC_BANDJ_OPCODE_J		0x02
#define MIPS32_OPC_BANDJ_OPCODE_JAL		0x03
#define MIPS32_OPC_BANDJ_OPCODE_BEQ		0x04
#define MIPS32_OPC_BANDJ_OPCODE_BNE		0x05
#define MIPS32_OPC_BANDJ_OPCODE_BLEZ		0x06
#define MIPS32_OPC_BANDJ_OPCODE_BGTZ		0x07
#define MIPS32_OPC_BANDJ_OPCODE_BEQL		0x14
#define MIPS32_OPC_BANDJ_OPCODE_BNEL		0x15
#define MIPS32_OPC_BANDJ_OPCODE_BLEZL		0x16
#define MIPS32_OPC_BANDJ_OPCODE_BGTZL		0x17


/* REGIMM Branch instruction opcodes */
#define MIPS32_OPC_BANDJ_REGIMM_OPCODE_BLTZ	0x00
#define MIPS32_OPC_BANDJ_REGIMM_OPCODE_BGEZ	0x01
#define MIPS32_OPC_BANDJ_REGIMM_OPCODE_BLTZL	0x02
#define MIPS32_OPC_BANDJ_REGIMM_OPCODE_BGEZL	0x03
#define MIPS32_OPC_BANDJ_REGIMM_OPCODE_BLTZAL	0x10
#define MIPS32_OPC_BANDJ_REGIMM_OPCODE_BGEZAL	0x11
#define MIPS32_OPC_BANDJ_REGIMM_OPCODE_BLTZALL	0x12
#define MIPS32_OPC_BANDJ_REGIMM_OPCODE_BGEZALL	0x13

/* SPECIAL JUMP instruction opcodes */
#define MIPS32_OPC_BANDJ_SPECIAL_OPCODE_JR	0x08
#define MIPS32_OPC_BANDJ_SPECIAL_OPCODE_JRHB	0x08
#define MIPS32_OPC_BANDJ_SPECIAL_OPCODE_JALR	0x09
#define MIPS32_OPC_BANDJ_SPECIAL_OPCODE_JALRHB	0x09

u32 cpu_vcpu_emulate_cop_inst(struct vmm_vcpu *vcpu, u32 inst, arch_regs_t *uregs);
u32 cpu_vcpu_emulate_branch_and_jump_inst(struct vmm_vcpu *vcpu, u32 inst, arch_regs_t *uregs);

#endif /* __CPU_VCPU_EMULATE_H_ */
