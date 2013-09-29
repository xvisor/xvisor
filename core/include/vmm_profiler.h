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
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @brief header file of hypervisor profiler.
 */

#ifndef _VMM_PROFILER_H__
#define _VMM_PROFILER_H__

#include <vmm_types.h>

#define VMM_PROFILE_ARRAY_SIZE		15
#define VMM_PROFILE_OTHER_INDEX		(VMM_PROFILE_ARRAY_SIZE - 1)
#define VMM_PROFILE_OTHER_PARENT	0xffffffff

struct vmm_profiler_counter {
        u32 index;
        u32 parent_index;
        atomic_t count;
        atomic64_t total_time;
        atomic64_t time_per_call;
};

struct vmm_profiler_stat {
        struct vmm_profiler_counter counter[VMM_PROFILE_ARRAY_SIZE];
};

/**
 * Check status of function level profiling.
 * Called from somewhere (usually cmd_profile).
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

/**
 * Get the base of the stat data array
 */
struct vmm_profiler_stat *vmm_profiler_get_stat_array(void);

/**
 * Initialize Profiler. 
 * Called from vmm_init() 
 */
int vmm_profiler_init(void);

#endif
