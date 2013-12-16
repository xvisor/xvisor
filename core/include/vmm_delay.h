/**
 * Copyright (c) 2012 Anup Patel.
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
 * @file vmm_delay.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for soft delay subsystem
 */
#ifndef _VMM_DELAY_H__
#define _VMM_DELAY_H__

#include <vmm_types.h>

/** Emulate soft delay in-terms of microseconds */
void vmm_udelay(u32 usecs);

/** Emulate soft delay in-terms of milliseconds */
void vmm_mdelay(u32 msecs);

/** Emulate soft delay in-terms of seconds */
void vmm_sdelay(u32 secs);

/** Get estimated speed of given host cpu in MHz */
u32 vmm_delay_estimate_cpu_mhz(u32 cpu);

/** Get estimated speed of given host cpu in KHz */
u32 vmm_delay_estimate_cpu_khz(u32 cpu);

/** Recaliberate soft delay subsystem */
void vmm_delay_recaliberate(void);

/** Initialization soft delay subsystem */
int vmm_delay_init(void);

#endif
