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

#include <vmm_error.h>
#include <vmm_types.h>
#include <vmm_stdio.h>
#include <vmm_scheduler.h>
#include <vmm_regs.h>
#include <cpu_asm_macros.h>
#include <cpu_vcpu_mmu.h>
#include <cpu_vcpu_emulate.h>

static u32 load_store_emulated_reg(u8 sreg, u8 sel,
				   u32 *treg,
				   vmm_vcpu_t *vcpu, u8 do_load)
{
	u32 _err = VMM_OK;
	u32 *emulated_reg;

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
		emulated_reg = &vcpu->sregs.cp0_regs[sreg];
		break;
	case 12:
		switch(sel) {
		case 0:
			emulated_reg = &vcpu->sregs.cp0_regs[CP0_STATUS_IDX];
			break;
		case 1:
			emulated_reg = &vcpu->sregs.cp0_regs[CP0_INTCTL_IDX];
			break;
		case 2:
			emulated_reg = &vcpu->sregs.cp0_regs[CP0_SRSCTL_IDX];
			break;
		case 3:
			emulated_reg = &vcpu->sregs.cp0_regs[CP0_SRSMAP_IDX];
			break;
		}
	case 13: /* Cause register */
		emulated_reg = &vcpu->sregs.cp0_regs[CP0_CAUSE_IDX];
		break;
	case 14:
		emulated_reg = &vcpu->sregs.cp0_regs[CP0_EPC_IDX];
		break;
	case 15:
		switch (sel) {
		case 0:
			emulated_reg = &vcpu->sregs.cp0_regs[CP0_PRID_IDX];
			break;
		case 1:
			emulated_reg = &vcpu->sregs.cp0_regs[CP0_EBASE_IDX];
			break;
		}
		break;
	case 16:
		switch(sel) {
		case 0:
			emulated_reg = &vcpu->sregs.cp0_regs[CP0_CONFIG_IDX];
			break;
		case 1:
			emulated_reg = &vcpu->sregs.cp0_regs[CP0_CONFIG1_IDX];
			break;
		case 2:
			emulated_reg = &vcpu->sregs.cp0_regs[CP0_CONFIG2_IDX];
			break;
		case 3:
			emulated_reg = &vcpu->sregs.cp0_regs[CP0_CONFIG3_IDX];
			break;
		}
		break;
	case 17:
		emulated_reg = &vcpu->sregs.cp0_regs[CP0_LLADDR_IDX];
		break;
	case 18:
		emulated_reg = &vcpu->sregs.cp0_regs[CP0_WATCHLO_IDX];
		break;
	case 19:
		emulated_reg = &vcpu->sregs.cp0_regs[CP0_WATCHHI_IDX];
		break;
	case 23:
		emulated_reg = &vcpu->sregs.cp0_regs[CP0_DEBUG_IDX];
		break;
	case 24:
		emulated_reg = &vcpu->sregs.cp0_regs[CP0_DEPC_IDX];
		break;
	case 25:
		switch(sel) {
		case 0:
			emulated_reg = &vcpu->sregs.cp0_regs[CP0_PERFCTL_IDX];
			break;
		case 1:
			emulated_reg = &vcpu->sregs.cp0_regs[CP0_PERFCNT_IDX];
			break;
		}
		break;
	case 26:
		emulated_reg = &vcpu->sregs.cp0_regs[CP0_ECC_IDX];
		break;
	case 27:
		emulated_reg = &vcpu->sregs.cp0_regs[CP0_CACHEERR_IDX];
		break;
	case 28:
		switch(sel) {
		case 0:
			emulated_reg = &vcpu->sregs.cp0_regs[CP0_TAGLO_IDX];
			break;
		case 1:
			emulated_reg = &vcpu->sregs.cp0_regs[CP0_DATALO_IDX];
			break;
		}
		break;
	case 29:
		switch(sel) {
		case 0:
			emulated_reg = &vcpu->sregs.cp0_regs[CP0_TAGHI_IDX];
			break;
		case 1:
			emulated_reg = &vcpu->sregs.cp0_regs[CP0_DATAHI_IDX];
			break;
		}
		break;
	case 31:
		emulated_reg = &vcpu->sregs.cp0_regs[CP0_ERRORPC_IDX];
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

/* Co-processor un-usable exception */
u32 cpu_vcpu_emulate_cop_inst(vmm_vcpu_t *vcpu, u32 inst, vmm_user_regs_t *uregs)
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
				vmm_panic("Unable to load emulated register.\n");
			}

			/* handled. Need to start execution from next instruction. */
			uregs->cp0_epc += 4;
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
				vmm_panic("Unable to load emulated register.\n");
			}

			uregs->cp0_epc += 4;
			break;

		case MIPS32_OPC_CP0_DIEI:
			if (!MIPS32_OPC_CP0_SC(inst)) {
				rt = MIPS32_OPC_CP0_RT(inst);
				/* only when rt points to a non-zero register
				 * save current status there. */
				if (rt)
					uregs->regs[rt] =
						vcpu->sregs.cp0_regs[CP0_STATUS_IDX];

				/* Opcode says disable interrupts (for vcpu) */
				vcpu->sregs.cp0_regs[CP0_STATUS_IDX] &= ~0x1UL;
			} else {
				rt = MIPS32_OPC_CP0_RT(inst);
				/* only when rt points to a non-zero register
				 * save current status there. */
				if (rt)
					uregs->regs[rt] =
						vcpu->sregs.cp0_regs[CP0_STATUS_IDX];

				/* Opcode says enable interrupts (for vcpu) */
				vcpu->sregs.cp0_regs[CP0_STATUS_IDX] |= 0x01UL;
			}

			uregs->cp0_epc += 4;
			break;
		default:
			ehi._entryhi = read_c0_entryhi() & ~0xFF;
			ehi._s_entryhi.asid = (0x1 << ASID_SHIFT);
			write_c0_entryhi(ehi._entryhi);
			vmm_panic("Unknown MFC0 direction\n");
			break;
		}
		break;
	}

	return VMM_OK;
}
