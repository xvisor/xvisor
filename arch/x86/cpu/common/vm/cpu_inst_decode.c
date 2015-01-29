/**
 * Copyright (c) 2014 Himanshu Chauhan.
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
 * @file cpu_inst_decode.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief x86 Common instruction decode definitions.
 */

#include <vmm_stdio.h>
#include <vmm_types.h>
#include <vmm_host_aspace.h>
#include <vmm_error.h>
#include <vmm_manager.h>
#include <libs/stringlib.h>
#include <cpu_mmu.h>
#include <cpu_vm.h>
#include <cpu_features.h>
#include <cpu_pgtbl_helper.h>
#include <arch_guest_helper.h>
#include <cpu_inst_decode.h>

int x86_decode_inst(struct vcpu_hw_context *context, x86_inst inst,
		    x86_decoded_inst_t *dinst)
{
	u8 opcode;
	u8 opsize = 4, is_rex = 0;
	u8 *cinst;
	mod32_rm_t rm;

	memset(dinst, 0, sizeof(x86_decoded_inst_t));

	cinst = &inst[0];

	if (*cinst == OP_SIZE_REX_PREF || *cinst == 0x41) {
		opsize = 8;
		is_rex = 1;
		cinst++;
	}

	if (*inst == OPC_ESC_OPCODE) {
		cinst++;
	}

	opcode = *cinst;
	cinst++;

	switch(opcode) {
	case OPC_MOV_CR_TO_R:
		dinst->inst_type = INST_TYPE_MOV_CR;
		dinst->inst_size = 3;
		if (is_rex) dinst->inst_size++;
		rm.byte = *cinst;
		dinst->inst.crn_mov.op_size = opsize;
		dinst->inst.crn_mov.src_reg = rm.f.dst + RM_REG_CR0;
		dinst->inst.crn_mov.dst_reg = rm.f.src;
		break;

	case OPC_MOV_R_TO_CR:
		dinst->inst_type = INST_TYPE_MOV_CR;
		dinst->inst_size = 3;
		if (is_rex) dinst->inst_size++;
		rm.byte = *cinst;
		dinst->inst.crn_mov.op_size = opsize;
		dinst->inst.crn_mov.src_reg = rm.f.src;
		dinst->inst.crn_mov.dst_reg = rm.f.dst + RM_REG_CR0;
		break;

	case OPC_MOVLQ_MM_AX:
		dinst->inst_type = INST_TYPE_MOV;
		dinst->inst_size = 5;
		dinst->inst.gen_mov.op_size = opsize;
		dinst->inst.gen_mov.src_type = OP_TYPE_MEM;
		dinst->inst.gen_mov.dst_type = OP_TYPE_REG;
		dinst->inst.gen_mov.dst_addr = RM_REG_AX;
		memcpy((void *)&dinst->inst.gen_mov.src_addr, (void *)cinst, opsize);
		break;

	case OPC_MOVLQ_AX_MM:
		dinst->inst_type = INST_TYPE_MOV;
		dinst->inst_size = 5;
		dinst->inst.gen_mov.op_size = opsize;
		dinst->inst.gen_mov.src_type = OP_TYPE_REG;
		dinst->inst.gen_mov.dst_type = OP_TYPE_MEM;
		dinst->inst.gen_mov.src_addr = RM_REG_AX;
		memcpy((void *)&dinst->inst.gen_mov.dst_addr, (void *)cinst, opsize);
		break;

	case OPC_MOVWLQ_IMM_RM_WLQ:
		rm.byte = *cinst;
		cinst++;

		/* destination is a displacement specified in instruction */
		if (rm.byte == 0x05) {
			dinst->inst_type = INST_TYPE_MOV;
			dinst->inst_size = 10;
			dinst->inst.gen_mov.op_size = opsize;
			dinst->inst.gen_mov.src_type = OP_TYPE_IMM;
			dinst->inst.gen_mov.dst_type = OP_TYPE_MEM;
			memcpy((void *)&dinst->inst.gen_mov.dst_addr, (void *)cinst, opsize);
			cinst += opsize;
			memcpy((void *)&dinst->inst.gen_mov.src_addr, (void *)cinst, opsize);
		} else {
			return VMM_EFAIL;
		}
		break;

	case OPC_MOVL_MMRR_RR:
		rm.byte = *cinst;
		dinst->inst_type = INST_TYPE_MOV;
		dinst->inst_size = 2;
		dinst->inst.gen_mov.op_size = 4;
		dinst->inst.gen_mov.src_type = OP_TYPE_MEM;
		dinst->inst.gen_mov.dst_type = OP_TYPE_REG;
		dinst->inst.gen_mov.src_addr = context->g_regs[rm.f.src];
		dinst->inst.gen_mov.dst_addr = rm.f.dst;
		break;

	case OPC_INVLPG:
		rm.byte = *cinst;
		cinst++;
		dinst->inst_type = INST_TYPE_CACHE;
		dinst->inst_size = 3;
		dinst->inst.src_reg = rm.f.src;
		break;

	case OPC_CLTS:
		dinst->inst_type = INST_TYPE_CLR_CR;
		dinst->inst_size = 2;
		dinst->inst.crn_mov.dst_reg = RM_REG_CR0;
		dinst->inst.crn_mov.src_reg = X86_CR0_TS;
		break;

	default:
		return VMM_EFAIL;
	}

	return VMM_OK;
}
