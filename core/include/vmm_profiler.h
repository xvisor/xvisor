/**
 * Copyright (c) 2011 Jean-Christophe Dubois
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
 * @file vmm_profiler.h
 * @version 0.01
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @brief header file of hypervisor profiler.
 */

#ifndef _VMM_PROFILER_H__
#define _VMM_PROFILER_H__

#include <vmm_types.h>

/**
 * Check status of function level profiling.
 * Called from some where (usually cmd_profile).
 */
bool vmm_profiler_isactive(void);

/**
 * Start function level profiling.
 * Called from some where (usually cmd_profile).
 */
int vmm_profiler_start(void); 

/**
 * Stop function level profiling. 
 * Called from some where (usually cmd_profile).
 */
int vmm_profiler_stop(void); 

u64 vmm_profiler_get_function_count(unsigned long addr);
u64 vmm_profiler_get_function_total_time(unsigned long addr);

/**
 * Initialize Profiler. 
 * Called from vmm_init() 
 */
int vmm_profiler_init(void);

#endif
