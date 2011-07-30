/*
 * Copyright (c) 2010-2020 Himanshu Chauhan.
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
 * @author: Himanshu Chauhan <hschauhan@nulltrace.org>
 * @brief: General exception handling stuff.
 */

#ifndef __CPU_GENEX_H_
#define __CPU_GENEX_H_

#include <vmm_types.h>
#include <vmm_regs.h>

#define EXEC_CODE_MASK			0x7CUL
#define EXEC_CODE_SHIFT			2

#define EXCEPTION_CAUSE(__cause_reg)	((__cause_reg & EXEC_CODE_MASK) \
					 >> EXEC_CODE_SHIFT)

typedef enum {
	EXEC_CODE_INT, /* Interrupt */
	EXEC_CODE_TLB_MOD, /* TLB Modification Exception */
	EXEC_CODE_TLBL, /* TLB Load Exception */
	EXEC_CODE_TLBS, /* TLB Store Exception */
	EXEC_CODE_ADEL, /* Address error (load or instruction fetch) */
	EXEC_CODE_ADES, /* Address error (store) */
	EXEC_CODE_IBE, /* Bus Error (instruction load) */
	EXEC_CODE_DBE, /* Bus Error (data fetch store) */
	EXEC_CODE_SYS, /* System call */
	EXEC_CODE_BP, /* Break point */
	EXEC_CODE_RI, /* Reserved instruction */
	EXEC_CODE_COPU, /* Unusable coprocessor */
	EXEC_CODE_OV, /* Arithmatic Overflow */
	EXEC_CODE_TRAP, /* Trap */
	EXEC_CODE_RESERVED1, /* reserved */
	EXEC_CODE_FPE, /* Floating point exception */
	EXEC_CODE_IMPL_DEP, /* Implementation dependent */
	EXEC_CODE_IMPL_DEP1, /* Ditto */
	EXEC_CODE_C2E, /* Reserved for precise COP2 Exec */
	EXEC_CODE_RESERVED2,
	EXEC_CODE_RESERVED3,
	EXEC_CODE_RESERVED4,
	EXEC_CODE_MDMX_UNUSABLE,
	EXEC_CODE_WATCH_ACCESS, /* reference to watch hi/lo */
	EXEC_CODE_MCHECK, /* Machine check */
	EXEC_CODE_RESERVED5,
	EXEC_CODE_RESERVED6,
	EXEC_CODE_RESERVED7,
	EXEC_CODE_RESERVED8,
	EXEC_CODE_RESERVED9,
	EXEC_CODE_CACHE_ERROR,
	EXEC_CODE_RESERVED10
} exec_code_t;

u32 do_general_exception(vmm_user_regs_t *uregs);

#endif /* __CPU_GENEX_H_ */
