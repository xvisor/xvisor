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
 * @file cpu_vcpu_mmu.c
 * @version 0.01
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief MMU handling functions related to VCPU.
 */

#include <vmm_error.h>
#include <vmm_types.h>
#include <vmm_regs.h>
#include <vmm_stdio.h>
#include <vmm_scheduler.h>
#include <cpu_asm_macros.h>
#include <cpu_vcpu_mmu.h>

int do_vcpu_tlbmiss(vmm_user_regs_t *uregs)
{
	u32 badvaddr = read_c0_badvaddr();
	vmm_vcpu_t *current_vcpu;
	int counter = 0;

	current_vcpu = vmm_scheduler_current_vcpu();
	badvaddr >>= VPN2_SHIFT;
	for (counter = 0; counter < 2 * CPU_TLB_COUNT; counter++) {
		if (current_vcpu->sregs->shadow_tlb_entries[counter]
		    .entryhi._s_entryhi.vpn2 == badvaddr) {
			fill_tlb_entry(&current_vcpu->sregs->
				       shadow_tlb_entries[counter], 4);
			return 0;
		} else {
			vmm_panic("No TLB entry in shadow. Send fault to guest.\n");
		}
	}

	return VMM_EFAIL;
}
