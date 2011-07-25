/**
 * Copyright (c) 2010 Himanshu Chauhan.
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
 * @file vmm_hyperthreads.c
 * @version 0.01
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief Source file for hyperthreads. These run on top of mterm vcpus.
 */

#include <vmm_stdio.h>
#include <vmm_error.h>
#include <vmm_cpu.h>
#include <vmm_string.h>
#include <cpu_asm_macros.h>

void vmm_hyperthread_regs_switch(vmm_hyperthread_t *tthread,
				 vmm_hyperthread_t *thread,
				 vmm_user_regs_t *regs)
{
	if (tthread) {
		vmm_memcpy((void *)&tthread->tregs, (void *)regs, sizeof(vmm_user_regs_t));
	}
	if (thread) {
		vmm_memcpy((void *)regs, (void *)&thread->tregs, sizeof(vmm_user_regs_t));
	}

	/*
	 * Hyperthreads should always be in kernel mode. As per design, switching
	 * routines are always called in interrupt context. So CP0_Status says we are
	 * in kernel mode. Reading and storing should do the trick.
	 */
	regs->cp0_status = read_c0_status();
}

s32 vmm_hyperthread_regs_init(vmm_hyperthread_t *tinfo, void *udata)
{
	vmm_memset(&tinfo->tregs, 0, sizeof(vmm_user_regs_t));
	tinfo->tregs.regs[A0_IDX] = (u32)udata;
	tinfo->tregs.regs[RA_IDX] = (u32)tinfo->tfn;
	tinfo->tregs.cp0_epc = (u32)tinfo->tfn;
	tinfo->tregs.regs[SP_IDX] = ((u32)tinfo + (sizeof(vmm_hyperthread_info_t)));
	tinfo->tregs.regs[S8_IDX] = tinfo->tregs.regs[SP_IDX];

	return VMM_OK;
}

vmm_hyperthread_t *vmm_hyperthread_uregs2thread(vmm_user_regs_t *tregs)
{
	return (vmm_hyperthread_t *)(((u32)tregs->regs[SP_IDX]) & 0xFFFFF000);
}

vmm_hyperthread_t *vmm_hyperthread_context2thread(void)
{
	vmm_hyperthread_t *current;
	u32 temp __unused;

	__asm__ __volatile__("add %0, $0, "num_to_string(SP)"\n\t"
			     "lui %1, 0xFFFF\n\t"
			     "ori %1, %1, 0xF000\n\t"
			     "and %0, %0, %1\n\t"
			     :"=r"(current), "=r"(temp));

	return current;
}
