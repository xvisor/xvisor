/**
 * Copyright (c) 2010 Anup Patel.
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
 * @file vmm_timer.h
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for timer subsystem
 */
#ifndef _VMM_TIMER_H__
#define _VMM_TIMER_H__

#include <vmm_types.h>
#include <vmm_list.h>
#include <vmm_regs.h>
#include <vmm_spinlocks.h>

typedef void (*vmm_ticker_hndl_t) (vmm_user_regs_t * regs, u32 ticks);

/** Structure representing a ticker (i.e. tick handler) */
struct vmm_ticker {
	struct dlist head;
	bool enabled;
	char name[32];
	vmm_ticker_hndl_t hndl;
};

typedef struct vmm_ticker vmm_ticker_t;

/** Control structure for Timer Subsystem */
struct vmm_timer_ctrl {
	u32 tick_usecs;
	struct dlist ticker_list;
};

typedef struct vmm_timer_ctrl vmm_timer_ctrl_t;

/** Process timer tick (Must be called from somewhere) */
void vmm_timer_tick_process(vmm_user_regs_t * regs, u32 ticks);

/** Get timer tick delay in microseconds */
u32 vmm_timer_tick_usecs(void);

/** Enable a ticker */
int vmm_timer_enable_ticker(vmm_ticker_t * tk);

/** Disable a ticker */
int vmm_timer_disable_ticker(vmm_ticker_t * tk);

/** Register a ticker */
int vmm_timer_register_ticker(vmm_ticker_t * tk);

/** Unregister a ticker */
int vmm_timer_unregister_ticker(vmm_ticker_t * tk);

/** Find a ticker */
vmm_ticker_t *vmm_timer_find_ticker(const char *name);

/** Retrive ticker with given index */
vmm_ticker_t *vmm_timer_ticker(int index);

/** Count number of ticker */
u32 vmm_timer_ticker_count(void);

/** Start timer */
void vmm_timer_start(void);

/** Stop timer */
void vmm_timer_stop(void);

/** Initialize timer subsystem */
int vmm_timer_init(void);

#endif
