/**
 * Copyright (c) 2012-2015 Himanshu Chauhan.
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
 * @file arch_regs.h
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief common header file for CPU registers
 */
#ifndef _ARCH_REGS_H__
#define _ARCH_REGS_H__

#include <vmm_types.h>
#include <cpu_mmu.h>

/*
 * Stack State at the entry of exception.
 *
 *      |               |
 *      |               |
 *      +---------------+
 *      |      SS       | +40
 *      +---------------+
 *      |     RSP       | +32
 *      +---------------+
 *      |    RFLAGS     | +24
 *      +---------------+
 *      |      CS       | +16
 *      +---------------+
 *      |     RIP       | +08
 *      +---------------+
 *      |  HW Err Code  | +00
 *      +---------------+
 *      |               |
 *      |               |
 */
struct arch_regs {
	/*
	 * x86_64_todo: With VT enabled, CPU saves the
	 * context of the guest. There is a section
	 * of particular format that needs to be defined
	 * here for CPU to save context on a vm_exit.
	 */
	u64 rax;
	u64 rbx;
	u64 rcx;
	u64 rdx;
	u64 rdi;
	u64 rsi;
	u64 rbp;
	u64 r8;
	u64 r9;
	u64 r10;
	u64 r11;
	u64 r12;
	u64 r13;
	u64 r14;
	u64 r15;
	u64 hw_err_code;
	u64 rip;
	u64 cs;
	u64 rflags;
	u64 rsp;
	u64 ss;
} __packed;

typedef struct arch_regs arch_regs_t;

#endif
