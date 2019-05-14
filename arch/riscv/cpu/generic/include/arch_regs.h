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

struct riscv_priv {
	unsigned long hedeleg;
	unsigned long hideleg;
	unsigned long bsstatus;
	unsigned long bsie;
	unsigned long bstvec;
	unsigned long bsscratch;
	unsigned long bsepc;
	unsigned long bscause;
	unsigned long bstval;
	unsigned long bsip;
	unsigned long bsatp;
	/* Opaque pointer to timer data */
	void *timer_priv;
};

struct riscv_guest_priv {
	/* Time offset */
	u64 time_offset;
	/* Stage2 pagetable */
	struct cpu_pgtbl *pgtbl;
};

#define riscv_timer_priv(vcpu)	(riscv_priv(vcpu)->timer_priv)
#define riscv_regs(vcpu)	(&((vcpu)->regs))
#define riscv_priv(vcpu)	((struct riscv_priv *)((vcpu)->arch_priv))
#define riscv_guest_priv(guest)	((struct riscv_guest_priv *)((guest)->arch_priv))

#endif
