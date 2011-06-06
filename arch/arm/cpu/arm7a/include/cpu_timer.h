/**
 * Copyright (c) 2011 Anup Patel.
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
 * @file cpu_timer.h
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for cpu timer handling functions
 */
#ifndef _CPU_TIMER_H__
#define _CPU_TIMER_H__

#include <vmm_types.h>
#include <vmm_guest.h>

/** Low-level Decrementer functions */
void decrementer_setup(u32 count, u32 reload_count);
void decrementer_clear_status();
void decrementer_disable();
void decrementer_enable();

/** High-level helper functions */
void read_timebase(u32 * upper, u32 * lower);
void write_timebase(u32 upper, u32 lower);

/** High-level functions to emulate timer features */
s32 cpu_emulate_decrementer(vmm_vcpu_t * vcpu);
s32 cpu_emulate_fit(vmm_vcpu_t * vcpu);
s32 cpu_emulate_watchdog(vmm_vcpu_t * vcpu);

#endif
