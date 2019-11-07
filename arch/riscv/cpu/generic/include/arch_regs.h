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
 * @file arch_regs.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief common header file for CPU registers
 */
#ifndef _ARCH_REGS_H__
#define _ARCH_REGS_H__

#include <vmm_const.h>

#define RISCV_PRIV_FP_F_F0	_AC(0x000, UL)
#define RISCV_PRIV_FP_F_F1	_AC(0x004, UL)
#define RISCV_PRIV_FP_F_F2	_AC(0x008, UL)
#define RISCV_PRIV_FP_F_F3	_AC(0x00c, UL)
#define RISCV_PRIV_FP_F_F4	_AC(0x010, UL)
#define RISCV_PRIV_FP_F_F5	_AC(0x014, UL)
#define RISCV_PRIV_FP_F_F6	_AC(0x018, UL)
#define RISCV_PRIV_FP_F_F7	_AC(0x01c, UL)
#define RISCV_PRIV_FP_F_F8	_AC(0x020, UL)
#define RISCV_PRIV_FP_F_F9	_AC(0x024, UL)
#define RISCV_PRIV_FP_F_F10	_AC(0x028, UL)
#define RISCV_PRIV_FP_F_F11	_AC(0x02c, UL)
#define RISCV_PRIV_FP_F_F12	_AC(0x030, UL)
#define RISCV_PRIV_FP_F_F13	_AC(0x034, UL)
#define RISCV_PRIV_FP_F_F14	_AC(0x038, UL)
#define RISCV_PRIV_FP_F_F15	_AC(0x03c, UL)
#define RISCV_PRIV_FP_F_F16	_AC(0x040, UL)
#define RISCV_PRIV_FP_F_F17	_AC(0x044, UL)
#define RISCV_PRIV_FP_F_F18	_AC(0x048, UL)
#define RISCV_PRIV_FP_F_F19	_AC(0x04c, UL)
#define RISCV_PRIV_FP_F_F20	_AC(0x050, UL)
#define RISCV_PRIV_FP_F_F21	_AC(0x054, UL)
#define RISCV_PRIV_FP_F_F22	_AC(0x058, UL)
#define RISCV_PRIV_FP_F_F23	_AC(0x05c, UL)
#define RISCV_PRIV_FP_F_F24	_AC(0x060, UL)
#define RISCV_PRIV_FP_F_F25	_AC(0x064, UL)
#define RISCV_PRIV_FP_F_F26	_AC(0x068, UL)
#define RISCV_PRIV_FP_F_F27	_AC(0x070, UL)
#define RISCV_PRIV_FP_F_F28	_AC(0x074, UL)
#define RISCV_PRIV_FP_F_F29	_AC(0x078, UL)
#define RISCV_PRIV_FP_F_F30	_AC(0x07c, UL)
#define RISCV_PRIV_FP_F_F31	_AC(0x080, UL)
#define RISCV_PRIV_FP_F_FCSR	_AC(0x084, UL)

#define RISCV_PRIV_FP_D_F0	_AC(0x000, UL)
#define RISCV_PRIV_FP_D_F1	_AC(0x008, UL)
#define RISCV_PRIV_FP_D_F2	_AC(0x010, UL)
#define RISCV_PRIV_FP_D_F3	_AC(0x018, UL)
#define RISCV_PRIV_FP_D_F4	_AC(0x020, UL)
#define RISCV_PRIV_FP_D_F5	_AC(0x028, UL)
#define RISCV_PRIV_FP_D_F6	_AC(0x030, UL)
#define RISCV_PRIV_FP_D_F7	_AC(0x038, UL)
#define RISCV_PRIV_FP_D_F8	_AC(0x040, UL)
#define RISCV_PRIV_FP_D_F9	_AC(0x048, UL)
#define RISCV_PRIV_FP_D_F10	_AC(0x050, UL)
#define RISCV_PRIV_FP_D_F11	_AC(0x058, UL)
#define RISCV_PRIV_FP_D_F12	_AC(0x060, UL)
#define RISCV_PRIV_FP_D_F13	_AC(0x068, UL)
#define RISCV_PRIV_FP_D_F14	_AC(0x070, UL)
#define RISCV_PRIV_FP_D_F15	_AC(0x078, UL)
#define RISCV_PRIV_FP_D_F16	_AC(0x080, UL)
#define RISCV_PRIV_FP_D_F17	_AC(0x088, UL)
#define RISCV_PRIV_FP_D_F18	_AC(0x090, UL)
#define RISCV_PRIV_FP_D_F19	_AC(0x098, UL)
#define RISCV_PRIV_FP_D_F20	_AC(0x0a0, UL)
#define RISCV_PRIV_FP_D_F21	_AC(0x0a8, UL)
#define RISCV_PRIV_FP_D_F22	_AC(0x0b0, UL)
#define RISCV_PRIV_FP_D_F23	_AC(0x0b8, UL)
#define RISCV_PRIV_FP_D_F24	_AC(0x0c0, UL)
#define RISCV_PRIV_FP_D_F25	_AC(0x0c8, UL)
#define RISCV_PRIV_FP_D_F26	_AC(0x0d0, UL)
#define RISCV_PRIV_FP_D_F27	_AC(0x0d8, UL)
#define RISCV_PRIV_FP_D_F28	_AC(0x0e0, UL)
#define RISCV_PRIV_FP_D_F29	_AC(0x0e8, UL)
#define RISCV_PRIV_FP_D_F30	_AC(0x0f0, UL)
#define RISCV_PRIV_FP_D_F31	_AC(0x0f8, UL)
#define RISCV_PRIV_FP_D_FCSR	_AC(0x100, UL)

#ifndef __ASSEMBLY__

#include <vmm_types.h>
#include <vmm_compiler.h>

struct arch_regs {
	unsigned long zero;
	unsigned long ra;
	unsigned long sp;
	unsigned long gp;
	unsigned long tp;
	unsigned long t0;
	unsigned long t1;
	unsigned long t2;
	unsigned long s0;
	unsigned long s1;
	unsigned long a0;
	unsigned long a1;
	unsigned long a2;
	unsigned long a3;
	unsigned long a4;
	unsigned long a5;
	unsigned long a6;
	unsigned long a7;
	unsigned long s2;
	unsigned long s3;
	unsigned long s4;
	unsigned long s5;
	unsigned long s6;
	unsigned long s7;
	unsigned long s8;
	unsigned long s9;
	unsigned long s10;
	unsigned long s11;
	unsigned long t3;
	unsigned long t4;
	unsigned long t5;
	unsigned long t6;
	unsigned long sepc;
	unsigned long sstatus;
	unsigned long hstatus;
	unsigned long sp_exec;
} __packed;

typedef struct arch_regs arch_regs_t;

struct riscv_priv_fp_f {
	u32 f[32];
	u32 fcsr;
} __packed;

struct riscv_priv_fp_d {
	u64 f[32];
	u32 fcsr;
} __packed;

union riscv_priv_fp {
	struct riscv_priv_fp_f f;
	struct riscv_priv_fp_d d;
};

struct riscv_priv {
	/* Register width */
	unsigned long xlen;
	/* ISA feature bitmap */
	unsigned long *isa;
	/* CSR state */
	unsigned long hie;
	unsigned long hip;
	unsigned long vsstatus;
	unsigned long vstvec;
	unsigned long vsscratch;
	unsigned long vsepc;
	unsigned long vscause;
	unsigned long vstval;
	unsigned long vsatp;
	/* FP state */
	union riscv_priv_fp fp;
	/* Opaque pointer to timer data */
	void *timer_priv;
};

struct riscv_guest_priv {
	/* Time delta */
	u64 time_delta;
	/* Stage2 pagetable */
	struct cpu_pgtbl *pgtbl;
	/* Opaque pointer to vserial data */
	void *guest_serial;
};

#define riscv_timer_priv(vcpu)	(riscv_priv(vcpu)->timer_priv)
#define riscv_regs(vcpu)	(&((vcpu)->regs))
#define riscv_priv(vcpu)	((struct riscv_priv *)((vcpu)->arch_priv))
#define riscv_guest_priv(guest)	((struct riscv_guest_priv *)((guest)->arch_priv))
#define riscv_guest_serial(guest)	(riscv_guest_priv(guest)->guest_serial)

#endif

#endif
