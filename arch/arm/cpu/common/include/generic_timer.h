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
 * @file generic_timer.h
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief API for ARM architecture generic timers
 */

#ifndef __GENERIC_TIMER_H__
#define __GENERIC_TIMER_H__

#define GENERIC_TIMER_HCTL_KERN_PCNT_EN		(1 << 0)
#define GENERIC_TIMER_HCTL_KERN_PTMR_EN		(1 << 1)

#define GENERIC_TIMER_CTRL_ENABLE		(1 << 0)
#define GENERIC_TIMER_CTRL_IT_MASK		(1 << 1)
#define GENERIC_TIMER_CTRL_IT_STAT		(1 << 2)

#define GENERIC_TIMER_CONTEXT_phys_timer_irq	0x0
#define GENERIC_TIMER_CONTEXT_virt_timer_irq	0x4
#define GENERIC_TIMER_CONTEXT_cntvoff		0x8
#define GENERIC_TIMER_CONTEXT_cntpcval		0x10
#define GENERIC_TIMER_CONTEXT_cntvcval		0x18
#define GENERIC_TIMER_CONTEXT_cntkctl		0x20
#define GENERIC_TIMER_CONTEXT_cntpctl		0x24
#define GENERIC_TIMER_CONTEXT_cntvctl		0x28

#ifndef __ASSEMBLY__

#include <vmm_types.h>
#include <vmm_compiler.h>

enum {
	GENERIC_TIMER_REG_FREQ,
	GENERIC_TIMER_REG_HCTL,
	GENERIC_TIMER_REG_KCTL,
	GENERIC_TIMER_REG_HYP_CTRL,
	GENERIC_TIMER_REG_HYP_TVAL,
	GENERIC_TIMER_REG_HYP_CVAL,
	GENERIC_TIMER_REG_PHYS_CTRL,
	GENERIC_TIMER_REG_PHYS_TVAL,
	GENERIC_TIMER_REG_PHYS_CVAL,
	GENERIC_TIMER_REG_VIRT_CTRL,
	GENERIC_TIMER_REG_VIRT_TVAL,
	GENERIC_TIMER_REG_VIRT_CVAL,
	GENERIC_TIMER_REG_VIRT_OFF,
};

struct generic_timer_context {
	u32 phys_timer_irq;
	u32 virt_timer_irq;
	u64 cntvoff;
	u64 cntpcval;
	u64 cntvcval;
	u32 cntkctl;
	u32 cntpctl;
	u32 cntvctl;
}__packed;

u64 generic_timer_wakeup_timeout(void);

int generic_timer_vcpu_context_init(void *vcpu_ptr,
				    void **context,
				    u32 phys_irq, u32 virt_irq);

int generic_timer_vcpu_context_deinit(void *vcpu_ptr, void **context);

void generic_timer_vcpu_context_save(void *vcpu_ptr, void *context);

void generic_timer_vcpu_context_restore(void *vcpu_ptr, void *context);

#endif /* __ASSEMBLY__ */

#endif /* __GENERIC_TIMER_H__ */
