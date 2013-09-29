/**
 * Copyright (c) 2012 Sukanto Ghosh.
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
#include <cpu_inline_asm.h>

#define generic_timer_pcounter_read()	read_cntpct()
#define generic_timer_vcounter_read()	read_cntvct()

/* If security extension is not implemented hypervisor can write to cntfrq */
#define generic_timer_freq_writeable()	(!cpu_supports_securex())

static inline void generic_timer_reg_write(int reg, u32 val)
{
	switch (reg) {
	case GENERIC_TIMER_REG_FREQ:
		write_cntfrq(val);
		break;
	case GENERIC_TIMER_REG_HCTL:
		write_cnthctl(val);
		break;
	case GENERIC_TIMER_REG_KCTL:
		write_cntkctl(val);
		break;
	case GENERIC_TIMER_REG_HYP_CTRL:
		write_cnthp_ctl(val);
		break;
	case GENERIC_TIMER_REG_HYP_TVAL:
		write_cnthp_tval(val);
		break;
	case GENERIC_TIMER_REG_PHYS_CTRL:
		write_cntp_ctl(val);
		break;
	case GENERIC_TIMER_REG_PHYS_TVAL:
		write_cntp_tval(val);
		break;
	case GENERIC_TIMER_REG_VIRT_CTRL:
		write_cntv_ctl(val);
		break;
	case GENERIC_TIMER_REG_VIRT_TVAL:
		write_cntv_tval(val);
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
		val = read_cntfrq();
		break;
	case GENERIC_TIMER_REG_HCTL:
		val = read_cnthctl();
		break;
	case GENERIC_TIMER_REG_KCTL:
		val = read_cntkctl();
		break;
	case GENERIC_TIMER_REG_HYP_CTRL:
		val = read_cnthp_ctl();
		break;
	case GENERIC_TIMER_REG_HYP_TVAL:
		val = read_cnthp_tval();
		break;
	case GENERIC_TIMER_REG_PHYS_CTRL:
		val = read_cntp_ctl();
		break;
	case GENERIC_TIMER_REG_PHYS_TVAL:
		val = read_cntp_tval();
		break;
	case GENERIC_TIMER_REG_VIRT_CTRL:
		val = read_cntv_ctl();
		break;
	case GENERIC_TIMER_REG_VIRT_TVAL:
		val = read_cntv_tval();
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
		write_cnthp_cval(val);
		break;
	case GENERIC_TIMER_REG_PHYS_CVAL:
		write_cntp_cval(val);
		break;
	case GENERIC_TIMER_REG_VIRT_CVAL:
		write_cntv_cval(val);
		break;
	case GENERIC_TIMER_REG_VIRT_OFF:
		write_cntvoff(val);
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
		val = read_cnthp_cval();
		break;
	case GENERIC_TIMER_REG_PHYS_CVAL:
		val = read_cntp_tval();
		break;
	case GENERIC_TIMER_REG_VIRT_CVAL:
		val = read_cntv_cval();
		break;
	case GENERIC_TIMER_REG_VIRT_OFF:
		val = read_cntvoff();
		break;
	default:
		vmm_panic("Trying to read invalid generic-timer register\n");
	}

	return val;
}

#endif	/* __CPU_GENERIC_TIMER_H__ */
