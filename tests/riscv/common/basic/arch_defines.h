/**
 * Copyright (c) 2019 Anup Patel.
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
 * @file arch_defines.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief register related macros & defines for RISC-V code
 */
#ifndef __ARCH_DEFINES_H__
#define __ARCH_DEFINES_H__

#ifdef __ASSEMBLY__
#define _AC(X,Y)	X
#define _AT(T,X)	X
#else
#define __AC(X,Y)	(X##Y)
#define _AC(X,Y)	__AC(X,Y)
#define _AT(T,X)	((T)(X))
#endif

/* Status register flags */
/* User Interrupt Enable */
#define SR_UIE				_AC(0x00000001, UL)
/* Supervisor Interrupt Enable */
#define SR_SIE				_AC(0x00000002, UL)
/* Previous User IE */
#define SR_UPIE				_AC(0x00000010, UL)
/* Previous Supervisor IE */
#define SR_SPIE				_AC(0x00000020, UL)
/* Previously Supervisor */
#define SR_SPP				_AC(0x00000100, UL)
/* Supervisor may access User Memory */
#define SR_SUM				_AC(0x00040000, UL)
/* Make executable readable */
#define SR_MXR				_AC(0x00080000, UL)

/* Floating-point Status */
#define SR_FS				_AC(0x00006000, UL)
#define SR_FS_OFF			_AC(0x00000000, UL)
#define SR_FS_INITIAL			_AC(0x00002000, UL)
#define SR_FS_CLEAN			_AC(0x00004000, UL)
#define SR_FS_DIRTY			_AC(0x00006000, UL)

/* Extension Status */
#define SR_XS				_AC(0x00018000, UL)
#define SR_XS_OFF			_AC(0x00000000, UL)
#define SR_XS_INITIAL			_AC(0x00008000, UL)
#define SR_XS_CLEAN			_AC(0x00010000, UL)
#define SR_XS_DIRTY			_AC(0x00018000, UL)

/* FS/XS dirty */
#if __riscv_xlen == 32
#define SR_SD				_AC(0x80000000, UL)
#else
#define SR_SD				_AC(0x8000000000000000, UL)
#endif

/* SATP flags */
#if __riscv_xlen == 32
#define SATP_PPN			_AC(0x003FFFFF, UL)
#define SATP_MODE_32			_AC(0x80000000, UL)
#define SATP_MODE			SATP_MODE_32
#else
#define SATP_PPN			_AC(0x00000FFFFFFFFFFF, UL)
#define SATP_MODE_39			_AC(0x8000000000000000, UL)
#define SATP_MODE			SATP_MODE_39
#endif

#define IRQ_S_SOFT			1
#define IRQ_H_SOFT			2
#define IRQ_M_SOFT			3
#define IRQ_S_TIMER			5
#define IRQ_H_TIMER			6
#define IRQ_M_TIMER			7
#define IRQ_S_EXT			9
#define IRQ_H_EXT			10
#define IRQ_M_EXT			11
#define IRQ_COP				12
#define IRQ_HOST			13

/* Interrupt Enable and Interrupt Pending flags */
/* Software Interrupt Enable */
#define SIE_SSIE			(1 << IRQ_S_SOFT)
/* Timer Interrupt Enable */
#define SIE_STIE			(1 << IRQ_S_TIMER)
/* External Interrupt Enable */
#define SIE_SEIE			(1 << IRQ_S_EXT)

/* SCAUSE */
#if __riscv_xlen == 32
#define SCAUSE_INTERRUPT_MASK   	_AC(0x80000000, UL)
#define SCAUSE_CAUSE_MASK		_AC(0x7FFFFFFF, UL)
#else
#define SCAUSE_INTERRUPT_MASK   	_AC(0x8000000000000000, UL)
#define SCAUSE_CAUSE_MASK		_AC(0x7FFFFFFFFFFFFFFF, UL)
#endif
#define CAUSE_INST_MISALIGNED		0
#define CAUSE_INST_ACCESS_FAULT		1
#define CAUSE_INST_ILLEGAL		2
#define CAUSE_BREAKPOINT		3
#define CAUSE_RESERVED1			4
#define CAUSE_LOAD_ACCESS_FAULT		5
#define CAUSE_AMO_MISALIGNED		6
#define CAUSE_STORE_AMO_ACCESS_FAULT	7
#define CAUSE_ECALL			8
#define CAUSE_RESERVED2			9
#define CAUSE_RESERVED3			10
#define CAUSE_RESERVED4			11
#define CAUSE_INST_PAGE_FAULT		12
#define CAUSE_LOAD_PAGE_FAULT     	13
#define CAUSE_RESERVED5			14
#define CAUSE_STORE_AMO_PAGE_FAULT    	15

#endif
