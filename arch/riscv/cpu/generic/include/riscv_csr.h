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
 * @file riscv_csr.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief CSR macros & defines for shared by all C & Assembly code
 *
 * The source has been largely adapted from Linux 4.x or higher:
 * arch/riscv/include/asm/csr.h
 *
 * Copyright (C) 2015 Regents of the University of California
 *
 * The original code is licensed under the GPL.
 */
#ifndef __RISCV_CSR_H__
#define __RISCV_CSR_H__

#include <riscv_asm.h>
#include <vmm_const.h>

/* Status register flags */
#define SR_UIE	_AC(0x00000001, UL) /* User Interrupt Enable */
#define SR_SIE	_AC(0x00000002, UL) /* Supervisor Interrupt Enable */
#define SR_UPIE	_AC(0x00000010, UL) /* Previous User IE */
#define SR_SPIE	_AC(0x00000020, UL) /* Previous Supervisor IE */
#define SR_SPP	_AC(0x00000100, UL) /* Previously Supervisor */
#define SR_SUM	_AC(0x00040000, UL) /* Supervisor may access User Memory */
#define SR_MXR	_AC(0x00080000, UL) /* Make executable readable */

#define SR_FS           _AC(0x00006000, UL) /* Floating-point Status */
#define SR_FS_OFF       _AC(0x00000000, UL)
#define SR_FS_INITIAL   _AC(0x00002000, UL)
#define SR_FS_CLEAN     _AC(0x00004000, UL)
#define SR_FS_DIRTY     _AC(0x00006000, UL)

#define SR_XS           _AC(0x00018000, UL) /* Extension Status */
#define SR_XS_OFF       _AC(0x00000000, UL)
#define SR_XS_INITIAL   _AC(0x00008000, UL)
#define SR_XS_CLEAN     _AC(0x00010000, UL)
#define SR_XS_DIRTY     _AC(0x00018000, UL)

#ifndef CONFIG_64BIT
#define SR_SD   _AC(0x80000000, UL) /* FS/XS dirty */
#else
#define SR_SD   _AC(0x8000000000000000, UL) /* FS/XS dirty */
#endif

/* SATP flags */
#if __riscv_xlen == 32
#define SATP_PPN     _AC(0x003FFFFF, UL)
#define SATP_MODE_32 _AC(0x80000000, UL)
#define SATP_MODE    SATP_MODE_32
#else
#define SATP_PPN     _AC(0x00000FFFFFFFFFFF, UL)
#define SATP_MODE_39 _AC(0x8000000000000000, UL)
#define SATP_MODE    SATP_MODE_39
#endif

/* Interrupt Enable and Interrupt Pending flags */
#define SIE_SSIE _AC(0x00000002, UL) /* Software Interrupt Enable */
#define SIE_STIE _AC(0x00000020, UL) /* Timer Interrupt Enable */
#define SIE_SEIE _AC(0x000000200, UL) /* External Interrupt Enable */

/* SCAUSE */
#if __riscv_xlen == 32
#define SCAUSE_INTERRUPT_MASK   _AC(0x80000000, UL)
#define SCAUSE_EXC_MASK		_AC(0x7FFFFFFF, UL)
#else
#define SCAUSE_INTERRUPT_MASK   _AC(0x8000000000000000, UL)
#define SCAUSE_EXC_MASK		_AC(0x7FFFFFFFFFFFFFFF, UL)
#endif
#define EXC_INST_MISALIGNED		0
#define EXC_INST_ACCESS_FAULT		1
#define EXC_INST_ILLEGAL		2
#define EXC_BREAKPOINT			3
#define EXC_RESERVED1			4
#define EXC_LOAD_ACCESS_FAULT		5
#define EXC_AMO_MISALIGNED		6
#define EXC_STORE_AMO_ACCESS_FAULT	7
#define EXC_ECALL			8
#define EXC_RESERVED2			9
#define EXC_RESERVED3			10
#define EXC_RESERVED4			11
#define EXC_INST_PAGE_FAULT		12
#define EXC_LOAD_PAGE_FAULT     	13
#define EXC_RESERVED5			14
#define EXC_STORE_AMO_PAGE_FAULT    	15

#ifndef __ASSEMBLY__

#define csr_swap(csr, val)					\
({								\
	unsigned long __v = (unsigned long)(val);		\
	__asm__ __volatile__ ("csrrw %0, " __ASM_STR(csr) ", %1"\
			      : "=r" (__v) : "rK" (__v)		\
			      : "memory");			\
	__v;							\
})

#define csr_read(csr)						\
({								\
	register unsigned long __v;				\
	__asm__ __volatile__ ("csrr %0, " __ASM_STR(csr)	\
			      : "=r" (__v) :			\
			      : "memory");			\
	__v;							\
})

#define csr_write(csr, val)					\
({								\
	unsigned long __v = (unsigned long)(val);		\
	__asm__ __volatile__ ("csrw " __ASM_STR(csr) ", %0"	\
			      : : "rK" (__v)			\
			      : "memory");			\
})

#define csr_read_set(csr, val)					\
({								\
	unsigned long __v = (unsigned long)(val);		\
	__asm__ __volatile__ ("csrrs %0, " __ASM_STR(csr) ", %1"\
			      : "=r" (__v) : "rK" (__v)		\
			      : "memory");			\
	__v;							\
})

#define csr_set(csr, val)					\
({								\
	unsigned long __v = (unsigned long)(val);		\
	__asm__ __volatile__ ("csrs " __ASM_STR(csr) ", %0"	\
			      : : "rK" (__v)			\
			      : "memory");			\
})

#define csr_read_clear(csr, val)				\
({								\
	unsigned long __v = (unsigned long)(val);		\
	__asm__ __volatile__ ("csrrc %0, " __ASM_STR(csr) ", %1"\
			      : "=r" (__v) : "rK" (__v)		\
			      : "memory");			\
	__v;							\
})

#define csr_clear(csr, val)					\
({								\
	unsigned long __v = (unsigned long)(val);		\
	__asm__ __volatile__ ("csrc " __ASM_STR(csr) ", %0"	\
			      : : "rK" (__v)			\
			      : "memory");			\
})

#endif /* __ASSEMBLY__ */

#endif
