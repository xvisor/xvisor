/**
 * Copyright (c) 2011 Pranav Sawargaonkar.
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
 * @file vmm_regs.h
 * @version 1.0
 * @author Pranav Sawargaonkar (pranav.sawargaonkar@gmail.com)
 * @author Anup Patel (anup@brainfault.org)
 * @brief common header file for CPU registers
 */
#ifndef _VMM_REGS_H__
#define _VMM_REGS_H__

#include <vmm_types.h>
#include <cpu_defines.h>
#include <cpu_mmu.h>

enum arm_features {
    ARM_FEATURE_VFP,
    ARM_FEATURE_AUXCR, /* ARM1026 Auxiliary control register. */
    ARM_FEATURE_XSCALE, /* Intel XScale extensions. */
    ARM_FEATURE_IWMMXT, /* Intel iwMMXt extension. */
    ARM_FEATURE_V6,
    ARM_FEATURE_V6K,
    ARM_FEATURE_V7,
    ARM_FEATURE_THUMB2,
    ARM_FEATURE_MPU, /* Only has Memory Protection Unit, not full MMU. */
    ARM_FEATURE_VFP3,
    ARM_FEATURE_VFP_FP16,
    ARM_FEATURE_NEON,
    ARM_FEATURE_DIV,
    ARM_FEATURE_M, /* Microcontroller profile. */
    ARM_FEATURE_OMAPCP, /* OMAP specific CP15 ops handling. */
    ARM_FEATURE_THUMB2EE
};

struct vmm_user_regs {
	u32 cpsr; /* CPSR */
	u32 gpr[CPU_GPR_COUNT];	/* R0 - R12 */
	u32 sp;	/* Stack Pointer */
	u32 lr;	/* Link Register */
	u32 pc;	/* Program Counter */
} __attribute((packed));

typedef struct vmm_user_regs vmm_user_regs_t;

struct vmm_super_regs {
	/* Priviledged CPSR */
	u32 cpsr;
	/* Banked Registers */
	u32 gpr_usr[CPU_FIQ_GPR_COUNT];	/* User Mode */
	u32 sp_usr;
	u32 lr_usr;
	u32 sp_svc; /* Supervisor Mode */
	u32 lr_svc;
	u32 spsr_svc;
	u32 sp_mon; /* Monitor Mode */
	u32 lr_mon;
	u32 spsr_mon;
	u32 sp_abt; /* Abort Mode */
	u32 lr_abt;
	u32 spsr_abt;
	u32 sp_und; /* Undefined Mode */
	u32 lr_und;
	u32 spsr_und;
	u32 sp_irq; /* IRQ Mode */
	u32 lr_irq;
	u32 spsr_irq;
	u32 gpr_fiq[CPU_FIQ_GPR_COUNT];	/* FIQ Mode */
	u32 sp_fiq;
	u32 lr_fiq;
	u32 spsr_fiq;
	/* System control coprocessor (cp15) */
	struct {
		/* Shadow L1 */
		cpu_l1tbl_t *l1;
		/* Shadow DACR */
		u32 dacr;
		/* Virtual TLB */
		struct {
			u8 *valid;
			u8 *asid;
			cpu_page_t *page;
			u32 victim;
			u32 count;
		} vtlb;
		/* Overlapping vectors */
		u32 ovect[CPU_IRQ_NR * 2];
		u32 ovect_base;
		/* Coprocessor Registers */
		u32 c0_cpuid;
		u32 c0_cachetype;
		u32 c0_ccsid[16]; /* Cache size. */
		u32 c0_clid; /* Cache level. */
		u32 c0_cssel; /* Cache size selection. */
		u32 c0_c1[8]; /* Feature registers. */
		u32 c0_c2[8]; /* Instruction set registers. */
		u32 c1_sctlr; /* System control register. */
		u32 c1_coproc; /* Coprocessor access register.  */
		u32 c1_xscaleauxcr; /* XScale auxiliary control register. */
		u32 c2_base0; /* MMU translation table base 0. */
		u32 c2_base1; /* MMU translation table base 1. */
		u32 c2_control; /* MMU translation table base control. */
		u32 c2_mask; /* MMU translation table base selection mask. */
		u32 c2_base_mask; /* MMU translation table base 0 mask. */
		u32 c2_data; /* MPU data cachable bits. */
		u32 c2_insn; /* MPU instruction cachable bits. */
		u32 c3; /* MMU domain access control register
				MPU write buffer control. */
		u32 c5_insn; /* Fault status registers. */
		u32 c5_data;
		u32 c6_region[8]; /* MPU base/size registers. */
		u32 c6_insn; /* Fault address registers. */
		u32 c6_data;
		u32 c9_insn; /* Cache lockdown registers. */
		u32 c9_data;
		u32 c13_fcse; /* FCSE PID. */
		u32 c13_context; /* Context ID. */
		u32 c13_tls1; /* User RW Thread register. */
		u32 c13_tls2; /* User RO Thread register. */
		u32 c13_tls3; /* Privileged Thread register. */
		u32 c15_cpar; /* XScale Coprocessor Access Register */
		u32 c15_ticonfig; /* TI925T configuration byte. */
		u32 c15_i_max; /* Maximum D-cache dirty line index. */
		u32 c15_i_min; /* Minimum D-cache dirty line index. */
		u32 c15_threadid; /* TI debugger thread-ID. */
	} cp15;
	/* Internal CPU feature flags. */
	u32 features;
} __attribute((packed));

typedef struct vmm_super_regs vmm_super_regs_t;

#endif
