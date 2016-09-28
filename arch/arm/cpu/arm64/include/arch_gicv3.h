/**
 * Copyright (c) 2016 Anup Patel.
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
 * @file arch_gicv3.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief architecure specific GICv3 interface
 *
 * The source has been largely adapted from Linux 4.x or higher:
 *
 * arch/arm64/include/asm/arch_gicv3.h
 *
 * Copyright (C) 2015 ARM Ltd.
 *
 * The original code is licensed under the GPL.
 */
#ifndef __ARCH_GICV3_H__
#define __ARCH_GICV3_H__

#include <vmm_compiler.h>
#include <vmm_host_io.h>
#include <arch_barrier.h>
#include <cpu_inline_asm.h>

#define ICC_EOIR1_EL1			sys_reg(3, 0, 12, 12, 1)
#define ICC_DIR_EL1			sys_reg(3, 0, 12, 11, 1)
#define ICC_IAR1_EL1			sys_reg(3, 0, 12, 12, 0)
#define ICC_SGI1R_EL1			sys_reg(3, 0, 12, 11, 5)
#define ICC_PMR_EL1			sys_reg(3, 0, 4, 6, 0)
#define ICC_CTLR_EL1			sys_reg(3, 0, 12, 12, 4)
#define ICC_SRE_EL1			sys_reg(3, 0, 12, 12, 5)
#define ICC_GRPEN1_EL1			sys_reg(3, 0, 12, 12, 7)

#define ICC_SRE_EL2			sys_reg(3, 4, 12, 9, 5)

/*
 * System register definitions
 */
#define ICH_VSEIR_EL2			sys_reg(3, 4, 12, 9, 4)
#define ICH_HCR_EL2			sys_reg(3, 4, 12, 11, 0)
#define ICH_VTR_EL2			sys_reg(3, 4, 12, 11, 1)
#define ICH_MISR_EL2			sys_reg(3, 4, 12, 11, 2)
#define ICH_EISR_EL2			sys_reg(3, 4, 12, 11, 3)
#define ICH_ELSR_EL2			sys_reg(3, 4, 12, 11, 5)
#define ICH_VMCR_EL2			sys_reg(3, 4, 12, 11, 7)

#define __LR0_EL2(x)			sys_reg(3, 4, 12, 12, x)
#define __LR8_EL2(x)			sys_reg(3, 4, 12, 13, x)

#define ICH_LR0_EL2			__LR0_EL2(0)
#define ICH_LR1_EL2			__LR0_EL2(1)
#define ICH_LR2_EL2			__LR0_EL2(2)
#define ICH_LR3_EL2			__LR0_EL2(3)
#define ICH_LR4_EL2			__LR0_EL2(4)
#define ICH_LR5_EL2			__LR0_EL2(5)
#define ICH_LR6_EL2			__LR0_EL2(6)
#define ICH_LR7_EL2			__LR0_EL2(7)
#define ICH_LR8_EL2			__LR8_EL2(0)
#define ICH_LR9_EL2			__LR8_EL2(1)
#define ICH_LR10_EL2			__LR8_EL2(2)
#define ICH_LR11_EL2			__LR8_EL2(3)
#define ICH_LR12_EL2			__LR8_EL2(4)
#define ICH_LR13_EL2			__LR8_EL2(5)
#define ICH_LR14_EL2			__LR8_EL2(6)
#define ICH_LR15_EL2			__LR8_EL2(7)

#define __AP0Rx_EL2(x)			sys_reg(3, 4, 12, 8, x)
#define ICH_AP0R0_EL2			__AP0Rx_EL2(0)
#define ICH_AP0R1_EL2			__AP0Rx_EL2(1)
#define ICH_AP0R2_EL2			__AP0Rx_EL2(2)
#define ICH_AP0R3_EL2			__AP0Rx_EL2(3)

#define __AP1Rx_EL2(x)			sys_reg(3, 4, 12, 9, x)
#define ICH_AP1R0_EL2			__AP1Rx_EL2(0)
#define ICH_AP1R1_EL2			__AP1Rx_EL2(1)
#define ICH_AP1R2_EL2			__AP1Rx_EL2(2)
#define ICH_AP1R3_EL2			__AP1Rx_EL2(3)

/*
 * Low-level accessors
 *
 * These system registers are 32 bits, but we make sure that the compiler
 * sets the GP register's most significant bits to 0 with an explicit cast.
 */

static inline void arch_gic_write_eoir(u32 irq)
{
	asm volatile("msr_s " stringify(ICC_EOIR1_EL1) ", %0"
			: : "r" ((u64)irq));
	isb();
}

static inline void arch_gic_write_dir(u32 irq)
{
	asm volatile("msr_s " stringify(ICC_DIR_EL1) ", %0"
			: : "r" ((u64)irq));
	isb();
}

static inline u64 arch_gic_read_iar(void)
{
	u64 irqstat;

	asm volatile("mrs_s %0, " stringify(ICC_IAR1_EL1)
			: "=r" (irqstat));
	dsb();
	return irqstat;
}

/*
 * Cavium ThunderX erratum 23154
 *
 * The gicv3 of ThunderX requires a modified version for reading the
 * IAR status to ensure data synchronization (access to icc_iar1_el1
 * is not sync'ed before and after).
 */
static inline u64 arch_gic_read_iar_cavium_thunderx(void)
{
	u64 irqstat;

	asm volatile(
		"nop;nop;nop;nop\n\t"
		"nop;nop;nop;nop\n\t"
		"mrs_s %0, " stringify(ICC_IAR1_EL1) "\n\t"
		"nop;nop;nop;nop"
		: "=r" (irqstat));
	dmb();

	return irqstat;
}

static inline void arch_gic_write_pmr(u32 val)
{
	asm volatile("msr_s " stringify(ICC_PMR_EL1) ", %0"
			: : "r" ((u64)val));
}

static inline void arch_gic_write_ctlr(u32 val)
{
	asm volatile("msr_s " stringify(ICC_CTLR_EL1) ", %0"
			: : "r" ((u64)val));
	isb();
}

static inline void arch_gic_write_grpen1(u32 val)
{
	asm volatile("msr_s " stringify(ICC_GRPEN1_EL1) ", %0"
			: : "r" ((u64)val));
	isb();
}

static inline void arch_gic_write_sgi1r(u64 val)
{
	asm volatile("msr_s " stringify(ICC_SGI1R_EL1) ", %0"
			: : "r" (val));
	isb();
}

static inline u32 arch_gic_read_sre(void)
{
	u64 val;

	asm volatile("mrs_s %0, " stringify(ICC_SRE_EL2)
			: "=r" (val));
	return val;
}

static inline void arch_gic_write_sre(u32 val)
{
	asm volatile("msr_s " stringify(ICC_SRE_EL2) ", %0"
			: : "r" ((u64)val));
	isb();
}

static inline unsigned long arch_gic_current_mpidr(void)
{
	return mrs(mpidr_el1) & 0xFF00FFFFFF;
}

#ifdef CONFIG_ARM_SMP_OPS
#include <smp_ops.h>
#define arch_gic_cpu_logical_map(cpu)	smp_logical_map(cpu)
#endif

static inline void arch_gic_write_irouter(u64 val, volatile void *addr)
{
	vmm_writel_relaxed((u32)val, addr);
	vmm_writel_relaxed((u32)(val >> 32), addr + 4);
}

static inline u64 arch_gic_read_typer(volatile void *addr)
{
	u64 val;

	val = vmm_readl_relaxed(addr);
	val |= (u64)vmm_readl_relaxed(addr + 4) << 32;
	return val;
}

#endif /* __ARCH_GICV3_H__ */
