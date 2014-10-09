/**
 * Copyright (c) 2013 Sukanto Ghosh.
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
 * @file cpu_generic_timer.h
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief CPU specific functions for ARM architecture generic timers
 */

#ifndef __CPU_GENERIC_TIMER_H__
#define __CPU_GENERIC_TIMER_H__

#include <vmm_types.h>
#include <arch_barrier.h>
#include <cpu_inline_asm.h>

#define generic_timer_pcounter_read()	mrs(cntpct_el0)
#define generic_timer_vcounter_read()	mrs(cntvct_el0)

/* cntfrq_el0 is writeable by highest implemented EL.
 * We are running at EL2 and if EL3 is not implemented, 
 * hypervisor can write to cntfrq_el0 */
#define generic_timer_freq_writeable()	(!cpu_supports_el3())

static inline void generic_timer_reg_write(int reg, u32 val)
{
	switch (reg) {
	case GENERIC_TIMER_REG_FREQ:
		msr(cntfrq_el0, val);
		break;
	case GENERIC_TIMER_REG_HCTL:
		msr(cnthctl_el2, val);
		break;
	case GENERIC_TIMER_REG_KCTL:
		msr(cntkctl_el1, val);
		break;
	case GENERIC_TIMER_REG_HYP_CTRL:
		msr(cnthp_ctl_el2, val);
		break;
	case GENERIC_TIMER_REG_HYP_TVAL:
		msr(cnthp_tval_el2, val);
		break;
	case GENERIC_TIMER_REG_PHYS_CTRL:
		msr(cntp_ctl_el0, val);
		break;
	case GENERIC_TIMER_REG_PHYS_TVAL:
		msr(cntp_tval_el0, val);
		break;
	case GENERIC_TIMER_REG_VIRT_CTRL:
		msr(cntv_ctl_el0, val);
		break;
	case GENERIC_TIMER_REG_VIRT_TVAL:
		msr(cntv_tval_el0, val);
		break;
	default:
		vmm_panic("Trying to write invalid generic-timer register\n");
	}

	isb();
}

static inline u32 generic_timer_reg_read(int reg)
{
	u32 val;

	switch (reg) {
	case GENERIC_TIMER_REG_FREQ:
		val = mrs(cntfrq_el0);
		break;
	case GENERIC_TIMER_REG_HCTL:
		val = mrs(cnthctl_el2);
		break;
	case GENERIC_TIMER_REG_KCTL:
		val = mrs(cntkctl_el1);
		break;
	case GENERIC_TIMER_REG_HYP_CTRL:
		val = mrs(cnthp_ctl_el2);
		break;
	case GENERIC_TIMER_REG_HYP_TVAL:
		val = mrs(cnthp_tval_el2);
		break;
	case GENERIC_TIMER_REG_PHYS_CTRL:
		val = mrs(cntp_ctl_el0);
		break;
	case GENERIC_TIMER_REG_PHYS_TVAL:
		val = mrs(cntp_tval_el0);
		break;
	case GENERIC_TIMER_REG_VIRT_CTRL:
		val = mrs(cntv_ctl_el0);
		break;
	case GENERIC_TIMER_REG_VIRT_TVAL:
		val = mrs(cntv_tval_el0);
		break;
	default:
		vmm_panic("Trying to read invalid generic-timer register\n");
	}

	return val;
}

static inline void generic_timer_reg_write64(int reg, u64 val)
{
	switch (reg) {
	case GENERIC_TIMER_REG_HYP_CVAL:
		msr(cnthp_cval_el2, val);
		break;
	case GENERIC_TIMER_REG_PHYS_CVAL:
		msr(cntp_cval_el0, val);
		break;
	case GENERIC_TIMER_REG_VIRT_CVAL:
		msr(cntv_cval_el0, val);
		break;
	case GENERIC_TIMER_REG_VIRT_OFF:
		msr(cntvoff_el2, val);
		break;
	default:
		vmm_panic("Trying to write invalid generic-timer register\n");
	}

	isb();
}

static inline u64 generic_timer_reg_read64(int reg)
{
	u64 val;

	switch (reg) {
	case GENERIC_TIMER_REG_HYP_CVAL:
		val = mrs(cnthp_cval_el2);
		break;
	case GENERIC_TIMER_REG_PHYS_CVAL:
		val = mrs(cntp_cval_el0);
		break;
	case GENERIC_TIMER_REG_VIRT_CVAL:
		val = mrs(cntv_cval_el0);
		break;
	case GENERIC_TIMER_REG_VIRT_OFF:
		val = mrs(cntvoff_el2);
		break;
	default:
		vmm_panic("Trying to read invalid generic-timer register\n");
	}

	return val;
}

#define HAVE_GENERIC_TIMER_REGS_SAVE

void generic_timer_regs_save(void *cntx);

#define HAVE_GENERIC_TIMER_REGS_RESTORE

void generic_timer_regs_restore(void *cntx);

#endif	/* __CPU_GENERIC_TIMER_H__ */
