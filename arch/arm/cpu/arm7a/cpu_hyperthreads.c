/**
 * Copyright (c) 2011 Anup Patel.
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
 * @file cpu_hyperthreads.c
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief C code for hyperthread related functions
 */

#include <vmm_error.h>
#include <vmm_cpu.h>
#include <vmm_string.h>

int vmm_hyperthread_regs_init(vmm_hyperthread_t * tinfo, void *udata)
{
	vmm_memset(&tinfo->tregs, 0, sizeof(vmm_user_regs_t));
	tinfo->tregs.pc = (u32) tinfo->tfn;
	tinfo->tregs.lr = (u32) tinfo->tfn;
	tinfo->tregs.sp =
	    ((u32) tinfo + sizeof(vmm_hyperthread_info_t) - 0x100);
	tinfo->tregs.gpr[0] = (u32) udata;

	return VMM_OK;
}

void vmm_hyperthread_regs_switch(vmm_hyperthread_t * tthread,
				 vmm_hyperthread_t * thread,
				 vmm_user_regs_t * regs)
{
	u32 ite;
	if (tthread) {
		tthread->tregs.pc = regs->pc;
		tthread->tregs.lr = regs->lr;
		tthread->tregs.sp = regs->sp;
		for (ite = 0; ite < CPU_GPR_COUNT; ite++)
			tthread->tregs.gpr[ite] = regs->gpr[ite];
	}
	regs->pc = thread->tregs.pc;
	regs->lr = thread->tregs.lr;
	regs->sp = thread->tregs.sp;
	for (ite = 0; ite < CPU_GPR_COUNT; ite++)
		regs->gpr[ite] = thread->tregs.gpr[ite];
}

vmm_hyperthread_t *vmm_hyperthread_uregs2thread(vmm_user_regs_t * tregs)
{
	return (vmm_hyperthread_t *) (((u32) tregs->sp) &
				      ~(sizeof(vmm_hyperthread_info_t) - 1));
}

vmm_hyperthread_t *vmm_hyperthread_context2thread(void)
{
	u32 dummy;
	return (vmm_hyperthread_t *) (((u32) & dummy) &
				      ~(sizeof(vmm_hyperthread_info_t) - 1));;
}
