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

#include <vmm_types.h>

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

u64 generic_timer_wakeup_timeout(void);

int generic_timer_vcpu_context_init(void **context,
				    u32 phys_irq, u32 virt_irq);

int generic_timer_vcpu_context_deinit(void **context);

void generic_timer_vcpu_context_save(void *context);

void generic_timer_vcpu_context_restore(void *context);

#endif /* __GENERIC_TIMER_H__ */
