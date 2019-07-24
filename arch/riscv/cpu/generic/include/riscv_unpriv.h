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
 * @file riscv_unpriv.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief RISC-V hypervisor unprivileged access routines
 *
 * The source has been largely adapted from OpenSBI 0.3 or higher:
 * includ/sbi/riscv_unpriv.h
 *
 * Copyright (c) 2019 Western Digital Corporation or its affiliates.
 *
 * Authors:
 *   Anup Patel <anup.patel@wdc.com>
 *
 * The original code is licensed under the BSD 2-clause license.
 */

#ifndef __RISCV_UNPRIV_H__
#define __RISCV_UNPRIV_H__

#include <vmm_types.h>
#include <vmm_compiler.h>
#include <libs/bitops.h>

#include <riscv_encoding.h>

#define STR(x) XSTR(x)
#define XSTR(x) #x

#define DECLARE_UNPRIVILEGED_LOAD_FUNCTION(type, insn)			\
static inline type load_##type(const type *addr)			\
{									\
	register ulong __hstatus asm ("a2");				\
	type val;							\
	asm ("csrrs %0, "STR(CSR_HSTATUS)", %3\n"			\
		#insn " %1, %2\n"					\
		"csrw "STR(CSR_HSTATUS)", %0"				\
	: "+&r" (__hstatus), "=&r" (val)				\
	: "m" (*addr), "r" (HSTATUS_SPRV));			\
	return val;							\
}

#define DECLARE_UNPRIVILEGED_STORE_FUNCTION(type, insn)			\
static inline void store_##type(type *addr, type val)			\
{									\
	register ulong __hstatus asm ("a2");				\
	asm volatile ("csrrs %0, "STR(CSR_HSTATUS)", %3\n"		\
		#insn " %1, %2\n"					\
		"csrw "STR(CSR_HSTATUS)", %0"				\
	: "+&r" (__hstatus)						\
	: "r" (val), "m" (*addr), "r" (HSTATUS_SPRV));	\
}

DECLARE_UNPRIVILEGED_LOAD_FUNCTION(u8, lbu)
DECLARE_UNPRIVILEGED_LOAD_FUNCTION(u16, lhu)
DECLARE_UNPRIVILEGED_LOAD_FUNCTION(s8, lb)
DECLARE_UNPRIVILEGED_LOAD_FUNCTION(s16, lh)
DECLARE_UNPRIVILEGED_LOAD_FUNCTION(s32, lw)
DECLARE_UNPRIVILEGED_STORE_FUNCTION(u8, sb)
DECLARE_UNPRIVILEGED_STORE_FUNCTION(u16, sh)
DECLARE_UNPRIVILEGED_STORE_FUNCTION(u32, sw)
#ifdef CONFIG_64BIT
DECLARE_UNPRIVILEGED_LOAD_FUNCTION(u32, lwu)
DECLARE_UNPRIVILEGED_LOAD_FUNCTION(u64, ld)
DECLARE_UNPRIVILEGED_STORE_FUNCTION(u64, sd)
DECLARE_UNPRIVILEGED_LOAD_FUNCTION(ulong, ld)
#else
DECLARE_UNPRIVILEGED_LOAD_FUNCTION(u32, lw)
DECLARE_UNPRIVILEGED_LOAD_FUNCTION(ulong, lw)

static inline u64 load_u64(const u64 *addr)
{
	return load_u32((u32 *)addr)
		+ ((u64)load_u32((u32 *)addr + 1) << 32);
}

static inline void store_u64(u64 *addr, u64 val)
{
	store_u32((u32 *)addr, val);
	store_u32((u32 *)addr + 1, val >> 32);
}
#endif

static inline ulong get_insn(ulong sepc, ulong *hstatus, ulong *vsstatus)
{
	register ulong __sepc asm ("a2") = sepc;
	register ulong __hstatus asm ("a3");
	register ulong __sstatus asm ("a4");
	register ulong __vsstatus asm ("a5");
	ulong val;
#ifndef __riscv_compressed
	asm ("csrrs %[hstatus], "STR(CSR_HSTATUS)", %[hprv]\n"
		"csrrs %[sstatus], "STR(CSR_SSTATUS)", %[smxr]\n"
		"csrrs %[vsstatus], "STR(CSR_VSSTATUS)", %[smxr]\n"
#ifdef CONFIG_64BIT
		STR(LWU) " %[insn], (%[addr])\n"
#else
		STR(LW) " %[insn], (%[addr])\n"
#endif
		"csrw "STR(CSR_VSSTATUS)", %[vsstatus]"
		"csrw "STR(CSR_SSTATUS)", %[sstatus]"
		"csrw "STR(CSR_HSTATUS)", %[hstatus]"
		: [hstatus] "+&r" (__hstatus), [sstatus] "+&r" (__sstatus),
		  [vsstatus] "+&r" (__vsstatus), [insn] "=&r" (val)
		: [hprv] "r" (HSTATUS_SPRV), [smxr] "r" (SSTATUS_MXR),
		  [addr] "r" (__sepc));
#else
	ulong rvc_mask = 3, tmp;
	asm ("csrrs %[hstatus], "STR(CSR_HSTATUS)", %[hprv]\n"
		"csrrs %[sstatus], "STR(CSR_SSTATUS)", %[smxr]\n"
		"csrrs %[vsstatus], "STR(CSR_VSSTATUS)", %[smxr]\n"
		"and %[tmp], %[addr], 2\n"
		"bnez %[tmp], 1f\n"
#ifdef CONFIG_64BIT
		STR(LWU) " %[insn], (%[addr])\n"
#else
		STR(LW) " %[insn], (%[addr])\n"
#endif
		"and %[tmp], %[insn], %[rvc_mask]\n"
		"beq %[tmp], %[rvc_mask], 2f\n"
		"sll %[insn], %[insn], %[xlen_minus_16]\n"
		"srl %[insn], %[insn], %[xlen_minus_16]\n"
		"j 2f\n"
		"1:\n"
		"lhu %[insn], (%[addr])\n"
		"and %[tmp], %[insn], %[rvc_mask]\n"
		"bne %[tmp], %[rvc_mask], 2f\n"
		"lhu %[tmp], 2(%[addr])\n"
		"sll %[tmp], %[tmp], 16\n"
		"add %[insn], %[insn], %[tmp]\n"
		"2: csrw "STR(CSR_VSSTATUS)", %[vsstatus]\n"
		"csrw "STR(CSR_SSTATUS)", %[sstatus]\n"
		"csrw "STR(CSR_HSTATUS)", %[hstatus]"
	: [hstatus] "+&r" (__hstatus), [sstatus] "+&r" (__sstatus),
	  [vsstatus] "+&r" (__vsstatus), [insn] "=&r" (val),
	  [tmp] "=&r" (tmp)
	: [hprv] "r" (HSTATUS_SPRV), [smxr] "r" (SSTATUS_MXR),
	  [addr] "r" (__sepc), [rvc_mask] "r" (rvc_mask),
	  [xlen_minus_16] "i" (__riscv_xlen - 16));
#endif
	if (hstatus)
		*hstatus = __hstatus;
	if (vsstatus)
		*vsstatus = __vsstatus;
	return val;
}

#endif
