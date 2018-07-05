/**
 * Copyright (c) 2018 Anup Patel.
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
 * @file cpu_proc.S
 * @author Anup Patel (anup@brainfault.org)
 * @brief Low-level implementation of CPU specific quirky functions
 */

#include <riscv_asm.h>

#ifdef CONFIG_SMP
	/* 
	 * Retrive SMP ID of current processor
	 */
	.globl arch_smp_id
arch_smp_id:
	addi	sp, sp, -(__SIZEOF_POINTER__)
	REG_S	tp, (sp)
	csrr	tp, sscratch
	REG_L	a0, RISCV_SCRATCH_SMP_ID_OFFSET(tp)
	REG_L	tp, (sp)
	addi	sp, sp, __SIZEOF_POINTER__
	ret

	/*
	 * Setup SMP ID of current processor
	 */
	.globl proc_setup_smp_id
proc_setup_smp_id:
	csrr	tp, sscratch
	REG_S	a0, RISCV_SCRATCH_SMP_ID_OFFSET(tp)
	ret
#endif

	.globl arch_vcpu_preempt_orphan
arch_vcpu_preempt_orphan:
	REG_S	zero, (zero)
	ret
