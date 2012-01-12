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
 * @file cpu_vcpu_emulate.c
 * @version 0.01
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief Priviledged instruction emulation code.
 */

#include <arch_regs.h>
#include <vmm_error.h>
#include <vmm_types.h>
#include <vmm_stdio.h>
#include <vmm_string.h>
#include <vmm_scheduler.h>
#include <cpu_asm_macros.h>
#include <cpu_vcpu_mmu.h>
#include <cpu_vcpu_emulate.h>

static u32 load_store_emulated_reg(u8 sreg, u8 sel,
				   u32 *treg,
				   struct vmm_vcpu *vcpu, u8 do_load)
{
	u32 _err = VMM_OK;
	u32 *emulated_reg = NULL;

	switch(sreg) {
	case 0: /* index register */
	case 1: /* Random register */
	case 2: /* entry lo0 */
	case 3: /* entry lo1 */
	case 4: /* context */
	case 5:
	case 6:
	case 7:
	case 8:
	case 9:
	case 10:
	case 11:
		emulated_reg = &mips_sregs(vcpu)->cp0_regs[sreg];
		break;
	case 12:
		switch(sel) {
		case 0:
			emulated_reg = &mips_sregs(vcpu)->cp0_regs[CP0_STATUS_IDX];
			break;
		case 1:
			emulated_reg = &mips_sregs(vcpu)->cp0_regs[CP0_INTCTL_IDX];
			break;
		case 2:
			emulated_reg = &mips_sregs(vcpu)->cp0_regs[CP0_SRSCTL_IDX];
			break;
		case 3:
			emulated_reg = &mips_sregs(vcpu)->cp0_regs[CP0_SRSMAP_IDX];
			break;
		}
	case 13: /* Cause register */
		emulated_reg = &mips_sregs(vcpu)->cp0_regs[CP0_CAUSE_IDX];
		break;
	case 14:
		emulated_reg = &mips_sregs(vcpu)->cp0_regs[CP0_EPC_IDX];
		break;
	case 15:
		switch (sel) {
		case 0:
			emulated_reg = &mips_sregs(vcpu)->cp0_regs[CP0_PRID_IDX];
			break;
		case 1:
			emulated_reg = &mips_sregs(vcpu)->cp0_regs[CP0_EBASE_IDX];
			break;
		}
		break;
	case 16:
		switch(sel) {
		case 0:
			emulated_reg = &mips_sregs(vcpu)->cp0_regs[CP0_CONFIG_IDX];
			break;
		case 1:
			emulated_reg = &mips_sregs(vcpu)->cp0_regs[CP0_CONFIG1_IDX];
			break;
		case 2:
			emulated_reg = &mips_sregs(vcpu)->cp0_regs[CP0_CONFIG2_IDX];
			break;
		case 3:
			emulated_reg = &mips_sregs(vcpu)->cp0_regs[CP0_CONFIG3_IDX];
			break;
		}
		break;
	case 17:
		emulated_reg = &mips_sregs(vcpu)->cp0_regs[CP0_LLADDR_IDX];
		break;
	case 18:
		emulated_reg = &mips_sregs(vcpu)->cp0_regs[CP0_WATCHLO_IDX];
		break;
	case 19:
		emulated_reg = &mips_sregs(vcpu)->cp0_regs[CP0_WATCHHI_IDX];
		break;
	case 23:
		emulated_reg = &mips_sregs(vcpu)->cp0_regs[CP0_DEBUG_IDX];
		break;
	case 24:
		emulated_reg = &mips_sregs(vcpu)->cp0_regs[CP0_DEPC_IDX];
		break;
	case 25:
		switch(sel) {
		case 0:
			emulated_reg = &mips_sregs(vcpu)->cp0_regs[CP0_PERFCTL_IDX];
			break;
		case 1:
			emulated_reg = &mips_sregs(vcpu)->cp0_regs[CP0_PERFCNT_IDX];
			break;
		}
		break;
	case 26:
		emulated_reg = &mips_sregs(vcpu)->cp0_regs[CP0_ECC_IDX];
		break;
	case 27:
		emulated_reg = &mips_sregs(vcpu)->cp0_regs[CP0_CACHEERR_IDX];
		break;
	case 28:
		switch(sel) {
		case 0:
			emulated_reg = &mips_sregs(vcpu)->cp0_regs[CP0_TAGLO_IDX];
			break;
		case 1:
			emulated_reg = &mips_sregs(vcpu)->cp0_regs[CP0_DATALO_IDX];
			break;
		}
		break;
	case 29:
		switch(sel) {
		case 0:
			emulated_reg = &mips_sregs(vcpu)->cp0_regs[CP0_TAGHI_IDX];
			break;
		case 1:
			emulated_reg = &mips_sregs(vcpu)->cp0_regs[CP0_DATAHI_IDX];
			break;
		}
		break;
	case 31:
		emulated_reg = &mips_sregs(vcpu)->cp0_regs[CP0_ERRORPC_IDX];
		break;
	default:
		_err = VMM_EFAIL;
		emulated_reg = NULL;
		break;
	}

	if (emulated_reg && _err == VMM_OK) {
		if (do_load)
			*treg = *emulated_reg;
		else
			*emulated_reg = *treg;
	}

	return _err;
}

u32 cpu_vcpu_emulate_tlb_inst(struct vmm_vcpu *vcpu, u32 inst,
			      arch_regs_t *uregs)
{
	switch(MIPS32_OPC_TLB_ACCESS_OPCODE(inst)) {
	case MIPS32_OPC_TLB_OPCODE_TLBP:
		return mips_probe_vcpu_tlb(vcpu, uregs);
	case MIPS32_OPC_TLB_OPCODE_TLBR:
		return mips_read_vcpu_tlb(vcpu, uregs);
	case MIPS32_OPC_TLB_OPCODE_TLBWI:
		return mips_write_vcpu_tlbi(vcpu, uregs);
	case MIPS32_OPC_TLB_OPCODE_TLBWR:
		return mips_write_vcpu_tlbr(vcpu, uregs);
	}

	return VMM_EFAIL;
}

/* Co-processor un-usable exception */
u32 cpu_vcpu_emulate_cop_inst(struct vmm_vcpu *vcpu, u32 inst,
			      arch_regs_t *uregs)
{
	u32 cp0_cause = read_c0_cause();
	u8 rt, rd, sel;
	mips32_entryhi_t ehi;

	u32 cop_id = UNUSABLE_COP_ID(cp0_cause);
	if (cop_id != 0) {
		ehi._entryhi = (read_c0_entryhi() & ~0xFF);
		ehi._s_entryhi.asid = (0x1 << ASID_SHIFT);
		write_c0_entryhi(ehi._entryhi);
		vmm_panic("COP%d unusable exeption!\n", cop_id);
	}

	switch(MIPS32_OPCODE(inst)) {
	case MIPS32_OPC_CP0_ACSS:
		switch(MIPS32_OPC_CP0_DIR(inst)) {
		case MIPS32_OPC_CP0_MF:
			rt = MIPS32_OPC_CP0_RT(inst);
			rd = MIPS32_OPC_CP0_RD(inst);
			sel = MIPS32_OPC_CP0_SEL(inst);
			if (load_store_emulated_reg(rd, sel,
						    &uregs->regs[rt],
						    vcpu, 1)) {
				ehi._entryhi = read_c0_entryhi() & ~0xFF;
				ehi._s_entryhi.asid = (0x1 << ASID_SHIFT);
				write_c0_entryhi(ehi._entryhi);
				vmm_panic("Can't load emulated register.\n");
			}

			break;
		case MIPS32_OPC_CP0_MT:
			rt = MIPS32_OPC_CP0_RT(inst);
			rd = MIPS32_OPC_CP0_RD(inst);
			sel = MIPS32_OPC_CP0_SEL(inst);
			if (load_store_emulated_reg(rd, sel,
						    &uregs->regs[rt],
						    vcpu, 0)) {
				ehi._entryhi = read_c0_entryhi() & ~0xFF;
				ehi._s_entryhi.asid = (0x1 << ASID_SHIFT);
				write_c0_entryhi(ehi._entryhi);
				vmm_panic("Can't load emulated register.\n");
			}

			break;

		case MIPS32_OPC_CP0_DIEI:
			if (!MIPS32_OPC_CP0_SC(inst)) {
				rt = MIPS32_OPC_CP0_RT(inst);
				/* only when rt points to a non-zero register
				 * save current status there. */
				if (rt)
					uregs->regs[rt] =
						mips_sregs(vcpu)->cp0_regs[CP0_STATUS_IDX];

				/* Opcode says disable interrupts (for vcpu) */
				mips_sregs(vcpu)->cp0_regs[CP0_STATUS_IDX] &= ~0x1UL;
			} else {
				rt = MIPS32_OPC_CP0_RT(inst);
				/* only when rt points to a non-zero register
				 * save current status there. */
				if (rt)
					uregs->regs[rt] =
						mips_sregs(vcpu)->cp0_regs[CP0_STATUS_IDX];

				/* Opcode says enable interrupts (for vcpu) */
				mips_sregs(vcpu)->cp0_regs[CP0_STATUS_IDX] |= 0x01UL;
			}

			break;
		default:
			if (IS_TLB_ACCESS_INST(inst)) {
				return cpu_vcpu_emulate_tlb_inst(vcpu,
								 inst, uregs);
			}
			break;
		}
		break;
	}

	return VMM_OK;
}

static u32 mips_emulate_branch_jump(struct vmm_vcpu *vcpu, u32 inst,
				    arch_regs_t *uregs)
{
	return 1;
}

static u32 mips_emulate_jump_special(struct vmm_vcpu *vcpu, u32 inst,
				     arch_regs_t *uregs)
{
	return 1;
}

static u32 mips_emulate_branch_regimm(struct vmm_vcpu *vcpu, u32 inst,
				      arch_regs_t *uregs)
{
	u8 rs = MIPS32_OPC_BANDJ_REGIMM_RS(inst);
	s32 target_offset = MIPS32_OPC_BANDJ_REGIMM_OFFSET(inst);
	u32 target_pc;
	u32 usual_epc = uregs->cp0_epc + 8;
	u32 should_execute_delay_slot = 1;

	/* 16 to 18 bit before adding to address of faulting instruction. */
	target_offset <<= 2;
	target_pc = (uregs->cp0_epc + 4) + target_offset;

	switch(MIPS32_OPC_BANDJ_REGIMM_OPCODE(inst)) {
	/* branch is taken or not, delay slot is executed in either case */
	case MIPS32_OPC_BANDJ_REGIMM_OPCODE_BLTZ:
		if ((s32)uregs->regs[rs] < 0)
			uregs->cp0_epc = target_pc;
		else
			uregs->cp0_epc += 8;
		break;

	/* same as BLTZ but delay slot is executed only if branch is taken */
	case MIPS32_OPC_BANDJ_REGIMM_OPCODE_BLTZL:
		if (uregs->regs[rs] < 0)
			uregs->cp0_epc = target_pc;
		else {
			/* No branch, no delay slot execution. */
			uregs->cp0_epc += 8;
			should_execute_delay_slot = 0;
		}
		break;

	case MIPS32_OPC_BANDJ_REGIMM_OPCODE_BGEZ:
		if (uregs->regs[rs] >= 0)
			uregs->cp0_epc = target_pc;
		else
			uregs->cp0_epc += 8;
		break;

	case MIPS32_OPC_BANDJ_REGIMM_OPCODE_BGEZL:
		if (uregs->regs[rs] >= 0)
			uregs->cp0_epc = target_pc;
		else {
			uregs->cp0_epc += 8;
			should_execute_delay_slot = 0;
		}
		break;

	case MIPS32_OPC_BANDJ_REGIMM_OPCODE_BGEZAL:
		if (uregs->regs[rs] >= 0) {
			uregs->cp0_epc = target_pc;
			uregs->regs[RA_IDX] = usual_epc;
		} else {
			uregs->cp0_epc += 8;
		}
		break;

	case MIPS32_OPC_BANDJ_REGIMM_OPCODE_BGEZALL:
		if (uregs->regs[rs] >= 0) {
			uregs->cp0_epc = target_pc;
			uregs->regs[RA_IDX] = usual_epc;
		} else {
			uregs->cp0_epc += 8;
			should_execute_delay_slot = 0;
		}
		break;

	case MIPS32_OPC_BANDJ_REGIMM_OPCODE_BLTZAL:
		if ((s32)uregs->regs[rs] < 0) {
			uregs->regs[RA_IDX] = usual_epc;
			uregs->cp0_epc = target_pc;
		} else {
			uregs->cp0_epc += 8;
		}
		break;

	case MIPS32_OPC_BANDJ_REGIMM_OPCODE_BLTZALL:
		if ((s32)uregs->regs[rs] < 0) {
			uregs->regs[RA_IDX] = usual_epc;
			uregs->cp0_epc = target_pc;
		} else {
			uregs->cp0_epc += 8;
			should_execute_delay_slot = 0;
		}
		break;
	}

	return should_execute_delay_slot;
}

/*
 * NOTE: This emulation will only happen when there was a fault or
 * there was a priviledged instruction in the delay slot. This condition
 * should be rare though.
 */
u32 cpu_vcpu_emulate_branch_and_jump_inst(struct vmm_vcpu *vcpu, u32 inst,
					  arch_regs_t *uregs)
{
	int _err = VMM_EFAIL;

	switch(MIPS32_OPC_BANDJ_OPCODE(inst)) {
	case MIPS32_OPC_BANDJ_OPCODE_SPECIAL:
		return mips_emulate_jump_special(vcpu, inst, uregs);
	case MIPS32_OPC_BANDJ_OPCODE_REGIMM:
		return mips_emulate_branch_regimm(vcpu, inst, uregs);
	default:
		return mips_emulate_branch_jump(vcpu, inst, uregs);
	}

	return _err;
}
