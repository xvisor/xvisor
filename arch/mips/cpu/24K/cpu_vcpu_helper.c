/**
 * Copyright (c) Himanshu Chauhan
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
 * @file cpu_vcpu_helper.c
 * @version 1.0
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief source of VCPU helper functions
 */

#include <vmm_error.h>
#include <vmm_string.h>
#include <vmm_guest.h>
#include <cpu_asm_macros.h>
#include <vmm_cpu.h>

extern char _stack_start;

void vmm_vcpu_regs_init(vmm_vcpu_t *vcpu)
{
	vmm_memset(&vcpu->uregs, 0, sizeof(vmm_user_regs_t));

        if (vcpu->guest == NULL) {
		/* For hypercore */
                vcpu->uregs.cp0_epc = vcpu->start_pc;
                vcpu->uregs.regs[SP_IDX] = (virtual_addr_t)&_stack_start;
		vcpu->uregs.regs[S8_IDX] = vcpu->uregs.regs[SP_IDX];
		vcpu->uregs.cp0_status = read_c0_status();
        } else {
		/* For vcpu running guests */
		vcpu->sregs.cp0_regs[CP0_CAUSE_IDX] = 0x400;
		vcpu->sregs.cp0_regs[CP0_STATUS_IDX] = 0x40004;
		vcpu->uregs.cp0_status = read_c0_status() | (0x01UL << CP0_STATUS_UM_SHIFT);
		/* All guest run from 0 and fault */
		vcpu->sregs.cp0_regs[CP0_EPC_IDX] = 0x00000000;
		/* Give guest the same CPU cap as we have */
		vcpu->sregs.cp0_regs[CP0_PRID_IDX] = read_c0_prid();
	}
}

void vmm_vcpu_regs_switch(vmm_vcpu_t *tvcpu, vmm_vcpu_t *vcpu,
			  vmm_user_regs_t *regs)
{
	if (tvcpu) {
		if (tvcpu->guest == NULL) {
			vmm_memcpy(&tvcpu->uregs, regs, sizeof(vmm_user_regs_t));
		}
	}

	if (vcpu) {
		if (vcpu->guest == NULL) {
			vcpu->uregs.cp0_status = read_c0_status() & ~(0x01UL << CP0_STATUS_UM_SHIFT);
		} else {
			vcpu->uregs.cp0_status = read_c0_status() | (0x01UL << CP0_STATUS_UM_SHIFT);
		}
		vmm_memcpy(regs, &vcpu->uregs, sizeof(vmm_user_regs_t));
	}
}

void vmm_vcpu_regs_dump(vmm_vcpu_t *vcpu) 
{
}
