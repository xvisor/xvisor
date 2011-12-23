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
 * @file cpu_macros.h
 * @version 0.1
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief CPU related macros.
 */

#ifndef __CPU_MACROS_H_
#define __CPU_MACROS_H_

#include <cpu_regs.h>

#ifdef __ASSEMBLY__ /* to be called only from assembly */

#define LEAF(fn)				\
        .globl fn;				\
        .ent fn;				\
fn:

#define END(fn)					\
        .size fn,.-fn;				\
        .end fn

#define tlbp_write_hazard 			\
        nop;					\
        nop;					\
        nop;					\
        nop;					\
        nop;					\
        nop;					\
        nop;					\
        nop;					\
        nop;

#define tlbp_read_hazard 			\
        nop;					\
        nop;					\
        nop;					\
        nop;					\
        nop;					\
        nop;					\
        nop;					\
        nop;					\
        nop;

#define tlbw_write_hazard 			\
        nop;					\
        nop;					\
        nop;					\
        nop;					\
        nop;					\
        nop;					\
        nop;					\
        nop;					\
        nop;

#define enable_global_interrupts		ei $0
#define disable_global_interrupts		di $0

#define EXCEPTION_VECTOR(_name, _offset, _where)\
	. = _offset;				\
	.set noreorder;				\
_name:						\
	b _where;				\
	nop;

#define SAVE_REG(reg, treg)			\
        sw reg, ((reg ## _IDX) * 4)(treg)

#define LOAD_REG(reg, treg)			\
        lw reg, ((reg ## _IDX) * 4)(treg)

#define SAVE_INT_CONTEXT(_int_sp)			\
	move K0, SP;					\
	la SP, _int_sp;					\
	addiu SP, SP, -((CPU_UREG_COUNT) * 4);		\
        mfc0 K1, CP0_EPC;				\
	SAVE_REG(V0,SP);				\
	SAVE_REG(V1,SP);				\
	SAVE_REG(A0,SP);				\
	SAVE_REG(A1,SP);				\
	SAVE_REG(A2,SP);				\
	SAVE_REG(A3,SP);				\
	SAVE_REG(T0,SP);				\
	SAVE_REG(T1,SP);				\
	SAVE_REG(T2,SP);				\
        sw K1, (U_CP0_EPC_IDX * 4)(SP);			\
	SAVE_REG(T3,SP);				\
	SAVE_REG(T4,SP);				\
	SAVE_REG(T5,SP);				\
	mfc0 K1, CP0_STATUS;				\
	SAVE_REG(T6,SP);				\
	SAVE_REG(T7,SP);				\
	SAVE_REG(S0,SP);				\
	SAVE_REG(S1,SP);				\
	SAVE_REG(S2,SP);				\
	sw K1, (U_CP0_STATUS_IDX * 4)(SP);		\
	SAVE_REG(S3,SP);				\
	SAVE_REG(S4,SP);				\
	SAVE_REG(S5,SP);				\
	mfc0 K1, CP0_ENTRYHI;				\
	SAVE_REG(S6,SP);				\
	SAVE_REG(S7,SP);				\
	SAVE_REG(T8,SP);				\
	SAVE_REG(T9,SP);				\
	sw K1, (U_CP0_ENTRYHI_IDX * 4)(SP);		\
	SAVE_REG(GP,SP);				\
	SAVE_REG(S8,SP);				\
	SAVE_REG(RA,SP);				\
        sw K0, (SP_IDX * 4)(SP)

#define RESTORE_INT_CONTEXT(treg)			\
        lw K1, (U_CP0_EPC_IDX * 4)(treg);		\
        mtc0 K1, CP0_EPC;				\
	ehb;						\
	LOAD_REG(V0,treg);				\
	LOAD_REG(V1,treg);				\
	LOAD_REG(A0,treg);				\
	LOAD_REG(A1,treg);				\
	LOAD_REG(A2,treg);				\
	LOAD_REG(A3,treg);				\
	LOAD_REG(T0,treg);				\
	LOAD_REG(T1,treg);				\
	LOAD_REG(T2,treg);				\
	LOAD_REG(T3,treg);				\
	LOAD_REG(T4,treg);				\
	lw K1, (U_CP0_STATUS_IDX * 4)(treg);		\
	mtc0 K1, CP0_STATUS;				\
	ehb;						\
	LOAD_REG(T5,treg);				\
	LOAD_REG(T6,treg);				\
	LOAD_REG(T7,treg);				\
	LOAD_REG(S0,treg);				\
	LOAD_REG(S1,treg);				\
	LOAD_REG(S2,treg);				\
	LOAD_REG(S3,treg);				\
	lw K1, (U_CP0_ENTRYHI_IDX * 4)(treg);		\
	mtc0 K1, CP0_ENTRYHI;				\
	ehb;						\
	LOAD_REG(S4,treg);				\
	LOAD_REG(S5,treg);				\
	LOAD_REG(S6,treg);				\
	LOAD_REG(S7,treg);				\
	LOAD_REG(T8,treg);				\
	LOAD_REG(T9,treg);				\
	LOAD_REG(GP,treg);				\
	LOAD_REG(RA,treg);				\
	LOAD_REG(S8,treg);				\
	lw SP, (SP_IDX * 4)(treg)

#endif /* __ASSEMBLY__ */

#define num_to_string(s)	to_string(s)
#define to_string(s)		#s

#define IASM_SAVE_REG(reg, here)				\
	"sw " to_string(reg) " , " num_to_string(reg ## _IDX)	\
			     " * 4(" num_to_string(here)" )\n\t"

#define IASM_LOAD_REG(reg, here)				\
	"lw " to_string(reg) " , " num_to_string(reg ## _IDX)	\
			     " * 4(" num_to_string(here)" )\n\t"

#define LOAD_WORD(to, from)			\
	"lw " to_string(to) " , " to_string(from) "\n\t"
#define STORE_WORD(from, to)			\
	"sw " to_string(from) ", " to_string(to) "\n\t"

#define LOAD_WORD_FROM_CP0(to, from_cp)		\
	"mfc0 " to_string(to) ", " to_string(from_cp) "\n\t"

#define STORE_WORD_TO_CP0(from, to_cp)		\
	"mtc0 " to_string(from) ", " to_string(to_cp) "\n\t"

#define NOP_HAZARD()				\
	asm volatile("nop\n\t"			\
	"nop\n\t"				\
	"nop\n\t"				\
	"nop\n\t"				\
	"nop\n\t"				\
	"nop\n\t"				\
	"nop\n\t"				\
	"nop\n\t"				\
	"nop\n\t"

#define EHB()					\
	asm volatile("ehb\n\t")

#define TLB_WRITE_RANDOM()			\
	asm volatile("tlbwr\n\t")

#define TLB_WRITE_INDEXED()			\
	asm volatile("tlbwi\n\t")

/*
 * Macros to be used with C code.
 */
#define __read_32bit_c0_register(source, sel)				\
({ int __res;								\
	if (sel == 0)							\
		__asm__ __volatile__(					\
			"mfc0\t%0, " #source "\n\t"			\
			: "=r" (__res));				\
	else								\
		__asm__ __volatile__(					\
			".set\tmips32\n\t"				\
			"mfc0\t%0, " #source ", " #sel "\n\t"		\
			".set\tmips0\n\t"				\
			: "=r" (__res));				\
	__res;								\
})

#define __write_32bit_c0_register(register, sel, value)			\
do {									\
	if (sel == 0)							\
		__asm__ __volatile__(					\
			"mtc0\t%z0, " #register "\n\t"			\
			: : "Jr" ((unsigned int)(value)));		\
	else								\
		__asm__ __volatile__(					\
			".set\tmips32\n\t"				\
			"mtc0\t%z0, " #register ", " #sel "\n\t"	\
			".set\tmips0"					\
			: : "Jr" ((unsigned int)(value)));		\
} while (0)

#define __read_ulong_c0_register(reg, sel)				\
	(unsigned long) __read_32bit_c0_register(reg, sel)

#define __write_ulong_c0_register(reg, sel, val)			\
do {									\
		__write_32bit_c0_register(reg, sel, val);		\
} while (0)

#define read_c0_index()		__read_32bit_c0_register($0, 0)
#define write_c0_index(val)	__write_32bit_c0_register($0, 0, val)

#define read_c0_entrylo0()	__read_ulong_c0_register($2, 0)
#define write_c0_entrylo0(val)	__write_ulong_c0_register($2, 0, val)

#define read_c0_entrylo1()	__read_ulong_c0_register($3, 0)
#define write_c0_entrylo1(val)	__write_ulong_c0_register($3, 0, val)

#define read_c0_conf()		__read_32bit_c0_register($3, 0)
#define write_c0_conf(val)	__write_32bit_c0_register($3, 0, val)

#define read_c0_context()	__read_ulong_c0_register($4, 0)
#define write_c0_context(val)	__write_ulong_c0_register($4, 0, val)

#define read_c0_userlocal()	__read_ulong_c0_register($4, 2)
#define write_c0_userlocal(val)	__write_ulong_c0_register($4, 2, val)

#define read_c0_pagemask()	__read_32bit_c0_register($5, 0)
#define write_c0_pagemask(val)	__write_32bit_c0_register($5, 0, val)

#define read_c0_wired()		__read_32bit_c0_register($6, 0)
#define write_c0_wired(val)	__write_32bit_c0_register($6, 0, val)

#define read_c0_info()		__read_32bit_c0_register($7, 0)

#define read_c0_badvaddr()	__read_ulong_c0_register($8, 0)
#define write_c0_badvaddr(val)	__write_ulong_c0_register($8, 0, val)

#define read_c0_count()		__read_32bit_c0_register($9, 0)
#define write_c0_count(val)	__write_32bit_c0_register($9, 0, val)

#define read_c0_entryhi()	__read_ulong_c0_register($10, 0)
#define write_c0_entryhi(val)	__write_ulong_c0_register($10, 0, val)

#define read_c0_compare()	__read_32bit_c0_register($11, 0)
#define write_c0_compare(val)	__write_32bit_c0_register($11, 0, val)

#define read_c0_status()	__read_32bit_c0_register($12, 0)
#define write_c0_status(val)	__write_32bit_c0_register($12, 0, val)

#define read_c0_cause()		__read_32bit_c0_register($13, 0)
#define write_c0_cause(val)	__write_32bit_c0_register($13, 0, val)

#define read_c0_epc()		__read_ulong_c0_register($14, 0)
#define write_c0_epc(val)	__write_ulong_c0_register($14, 0, val)

#define read_c0_prid()		__read_32bit_c0_register($15, 0)

#define read_c0_config()	__read_32bit_c0_register($16, 0)
#define read_c0_config1()	__read_32bit_c0_register($16, 1)
#define read_c0_config2()	__read_32bit_c0_register($16, 2)
#define write_c0_config(val)	__write_32bit_c0_register($16, 0, val)
#define write_c0_config1(val)	__write_32bit_c0_register($16, 1, val)
#define write_c0_config2(val)	__write_32bit_c0_register($16, 2, val)

#define read_c0_xcontext()	__read_ulong_c0_register($20, 0)
#define write_c0_xcontext(val)	__write_ulong_c0_register($20, 0, val)

#define read_c0_intcontrol()	__read_32bit_c0_ctrl_register($20)
#define write_c0_intcontrol(val) __write_32bit_c0_ctrl_register($20, val)

#define read_c0_framemask()	__read_32bit_c0_register($21, 0)
#define write_c0_framemask(val)	__write_32bit_c0_register($21, 0, val)

/*
 * MIPS32 / MIPS64 performance counters
 */
#define read_c0_cacheerr()	__read_32bit_c0_register($27, 0)

#define read_c0_taglo()		__read_32bit_c0_register($28, 0)
#define write_c0_taglo(val)	__write_32bit_c0_register($28, 0, val)

#define read_c0_dtaglo()	__read_32bit_c0_register($28, 2)
#define write_c0_dtaglo(val)	__write_32bit_c0_register($28, 2, val)

#define read_c0_taghi()		__read_32bit_c0_register($29, 0)
#define write_c0_taghi(val)	__write_32bit_c0_register($29, 0, val)

#define read_c0_errorepc()	__read_ulong_c0_register($30, 0)
#define write_c0_errorepc(val)	__write_ulong_c0_register($30, 0, val)

/* MIPSR2 */
#define read_c0_hwrena()	__read_32bit_c0_register($7, 0)
#define write_c0_hwrena(val)	__write_32bit_c0_register($7, 0, val)

#define read_c0_intctl()	__read_32bit_c0_register($12, 1)
#define write_c0_intctl(val)	__write_32bit_c0_register($12, 1, val)

#define read_c0_srsctl()	__read_32bit_c0_register($12, 2)
#define write_c0_srsctl(val)	__write_32bit_c0_register($12, 2, val)

#define read_c0_srsmap()	__read_32bit_c0_register($12, 3)
#define write_c0_srsmap(val)	__write_32bit_c0_register($12, 3, val)

#define read_c0_ebase()		__read_32bit_c0_register($15, 1)
#define write_c0_ebase(val)	__write_32bit_c0_register($15, 1, val)

#define CP0_STATUS_UM_MASK	0x10UL
#define CP0_STATUS_UM_SHIFT	4
#define CPU_IN_USER_MODE(__status)	(__status & CP0_STATUS_UM_MASK)

#endif /* __CPU_MACROS_H_ */
