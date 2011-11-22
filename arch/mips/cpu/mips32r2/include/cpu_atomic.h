/**
 * Copyright (c) 2010 Himanshu Chauhan.
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
 * @file arch/mips/cpu/24K/include/cpu_atomic.h
 * @version 0.01
 * @author Himanshu Chauhan (hchauhan@nulltrace.org)
 * @brief Architecture specific implementation of synchronization mechanisms.
 */

#ifndef __CPU_ATOMIC_H__
#define __CPU_ATOMIC_H__

#include <vmm_types.h>

void __lock_section __cpu_atomic_inc (atomic_t *atom);
void __lock_section __cpu_atomic_dec (atomic_t *atom);

#endif /* __VMM_CPU_LOCKS_H__ */
