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


u32 cpu_vcpu_emulate_cop_inst(vmm_vcpu_t *vcpu, u32 inst, vmm_user_regs_t *uregs);

#endif /* __CPU_VCPU_EMULATE_H_ */
