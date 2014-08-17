/**
 * Copyright (c) 2013 Himanshu Chauhan.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * @file cpu_inst_emulate.h
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief x86 Common instruction decoder declarations.
 */

#ifndef __CPU_INST_DECODE_H_
#define __CPU_INST_DECODE_H_

#define X86_MAX_INST_LEN	14

typedef unsigned char x86_inst[X86_MAX_INST_LEN];

typedef enum {
	INST_TYPE_MOV,
	INST_TYPE_MOV_CR,
} inst_type;

/* Operand type in instruction */
typedef enum {
	OP_TYPE_REG,
	OP_TYPE_MEM,
	OP_TYPE_IMM,
} op_type;

/* Applies to 16,32, and 64 bit instructions */
typedef enum {
	RM_REG_AX   = 0,
	RM_REG_CX   = 1,
	RM_REG_DX   = 2,
	RM_REG_BX   = 3,
	RM_REG_SP   = 4,
	RM_REG_BP   = 5,
	RM_REG_SI   = 6,
	RM_REG_DI   = 7,
	RM_REG_R8   = 8,
	RM_REG_R9   = 9,
	RM_REG_R10  = 10,
	RM_REG_R11  = 11,
	RM_REG_R12  = 12,
	RM_REG_R13  = 13,
	RM_REG_R14  = 14,
	RM_REG_R15  = 15,
	RM_REG_RIP  = 16,
	RM_REG_CR0  = 17,
	RM_REG_CR1  = 18,
	RM_REG_CR2  = 19,
	RM_REG_CR3  = 20,
	RM_REG_CR4  = 21,
	RM_REG_GDTR = 22,
	RM_REG_LDTR = 23,
	RM_REG_TR   = 24,
	RM_REG_IDTR = 25,

	RM_REG_MAX
} rm_reg_t;

typedef union mod_rm {
	u8 byte;
	struct {
		u32 dst:3; /* dst reg */
		u32 src:3; /* src reg */
		u32 mod:2;
	} f;
} mod32_rm_t;

/******************************
 * Addressing modes in opcode *
 ******************************/
/* Indirect addressing. [EAX] */
#define OPC_ADD_MOD_DISP0	0x00
/* Base pointer + 8bit offset, [EAX]+disp8 */
#define OPC_ADD_MOD_DISP8	0x01
/* Base pointer + 32-bit displacement, [EAX]+disp32 */
#define OPC_ADD_MOD_DISP32	0x02
/* Register to Register */
#define OPC_ADD_MOD_REG		0x03

/******************************
 *          OPCODES           *
 ******************************/
/* move reg to reg */
#define OPC_MOVL_RR		0x89
/* move byte from seg:off to register */
#define OPC_MOVB_MM_AX		0xa0
/* move word/double word from seg:off to AX */
#define OPC_MOVLQ_MM_AX		0xa1
/* move word/double world from AX to seg:off */
#define OPC_MOVLQ_AX_MM		0xa3
/* move imm to register/memory */
#define OPC_MOVWLQ_IMM_RM_WLQ	0xc7
/* move crN to Reg */
#define OPC_MOV_CR_TO_R		0x20
/* move Reg to crN */
#define OPC_MOV_R_TO_CR		0x22

#define OPC_ESC_OPCODE		0x0f
#define OP_SIZE_REX_PREF	0x48

typedef struct {
	u64 inst_type;
	u64 inst_size;
	union {
		struct {
			u32 op_size;  /* size of operation */
			u32 src_type; /* type of source operand (mem or register) */
			u32 dst_type;
			u64 src_addr;
			u64 dst_addr;
		} gen_mov;

		struct {
			u32 op_size;
			u64 src_reg;
			u64 dst_reg;
		} crn_mov;
	} inst;
} x86_decoded_inst_t;


int x86_decode_inst(x86_inst inst, x86_decoded_inst_t *dinst);

#endif /* __CPU_INST_DECODE_H_ */
