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
 * @file cpu_genx.c
 * @version 0.01
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief CPU General Exception Handler.
 */

#include <vmm_error.h>
#include <vmm_types.h>
#include <vmm_stdio.h>
#include <vmm_scheduler.h>
#include <vmm_regs.h>
#include <cpu_asm_macros.h>
#include <cpu_vcpu_mmu.h>
#include <cpu_genex.h>
#include <cpu_vcpu_emulate.h>

u32 do_general_exception(vmm_user_regs_t *uregs)
{
	u32 cp0_cause = read_c0_cause();
	u32 cp0_status = read_c0_status();
	mips32_entryhi_t ehi;
	u32 victim_asid;
	u32 victim_inst;
	vmm_vcpu_t *c_vcpu;

	ehi._entryhi = read_c0_entryhi();
	victim_asid = ehi._s_entryhi.asid >> ASID_SHIFT;
	c_vcpu = vmm_scheduler_current_vcpu();

	switch (EXCEPTION_CAUSE(cp0_cause)) {
	case EXEC_CODE_COPU:
		victim_inst = *((u32 *)uregs->cp0_epc);
		cpu_vcpu_emulate_cop_inst(c_vcpu, victim_inst, uregs);
		break;

	case EXEC_CODE_TLBL:
		if (CPU_IN_USER_MODE(cp0_status) && is_vmm_asid(ehi._s_entryhi.asid)) {
			ehi._s_entryhi.asid = (0x1 << ASID_SHIFT);
			write_c0_entryhi(ehi._entryhi);
			vmm_panic("CPU is in user mode and ASID is pointing to VMM!!\n");
		}
		break;
	}

	return VMM_OK;
}
