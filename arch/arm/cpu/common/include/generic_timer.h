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

#include <vmm_devtree.h>

enum {
	GENERIC_TIMER_REG_FREQ,
	GENERIC_TIMER_REG_HCTL,
	GENERIC_TIMER_REG_KCTL,
	GENERIC_TIMER_REG_HYP_CTRL,
	GENERIC_TIMER_REG_HYP_TVAL,
	GENERIC_TIMER_REG_PHYS_CTRL,
	GENERIC_TIMER_REG_PHYS_TVAL,
	GENERIC_TIMER_REG_PHYS_CVAL,
	GENERIC_TIMER_REG_VIRT_CTRL,
	GENERIC_TIMER_REG_VIRT_TVAL,
	GENERIC_TIMER_REG_VIRT_CVAL,
	GENERIC_TIMER_REG_VIRT_OFF,
};

#define GENERIC_TIMER_HCTL_KERN_PCNT_EN		(1 << 0)
#define GENERIC_TIMER_HCTL_KERN_PTMR_EN		(1 << 1)

#define GENERIC_TIMER_CTRL_ENABLE		(1 << 0)
#define GENERIC_TIMER_CTRL_IT_MASK		(1 << 1)
#define GENERIC_TIMER_CTRL_IT_STAT		(1 << 2)

enum gen_timer_type {
	GENERIC_HYPERVISOR_TIMER,
	GENERIC_PHYSICAL_TIMER,
	GENERIC_VIRTUAL_TIMER,
};


int generic_timer_clocksource_init(struct vmm_devtree_node *node); 

int generic_timer_clockchip_init(struct vmm_devtree_node *node);

u64 generic_timer_wakeup_timeout(void);

struct generic_timer_context {
	u64 cntvoff;
	u32 cntkctl;
	u32 cntpcval;
	u32 cntpctl;
	u32 cntvcval;
	u32 cntvctl;
	u32 phys_timer_irq;
	u32 virt_timer_irq;
};

void generic_timer_vcpu_context_init(struct generic_timer_context *context);
void generic_timer_vcpu_context_save(struct generic_timer_context *context);
void generic_timer_vcpu_context_restore(struct generic_timer_context *context);

#endif /* __GENERIC_TIMER_H__ */
