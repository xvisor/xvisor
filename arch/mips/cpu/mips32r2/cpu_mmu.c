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
 * @file cpu_mmu.c
 * @version 0.01
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief MMU handling functions.
 */

#include <vmm_types.h>
#include <vmm_stdio.h>
#include <vmm_scheduler.h>
#include <cpu_mmu.h>
#include <vmm_regs.h>
#include <cpu_asm_macros.h>
#include <cpu_vcpu_mmu.h>
#include <vmm_error.h>

u32 do_tlbmiss(vmm_user_regs_t *uregs)
{
	mips32_entryhi_t ehi;
	ehi._entryhi = read_c0_entryhi();

	/* FIXME: Hardcoded 1 is the ASID of VMM. Move it to a macro */
	if (!is_vmm_asid(ehi._s_entryhi.asid)) {
		return do_vcpu_tlbmiss(uregs);
	} else {
		vmm_panic("ARGHHH!!! Cannot handle page fault in VMM!\n");
	}

	return 0;
}
