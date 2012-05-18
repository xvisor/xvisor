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
 * @file vmm_percpu.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Interface for per-cpu areas 
 */

#ifndef __VMM_PERCPU_H__
#define __VMM_PERCPU_H__

#include <vmm_types.h>

#ifdef CONFIG_SMP

#define RELOC_HIDE(ptr, off)                    \
  ({ unsigned long __ptr;                       \
    __asm__ ("" : "=r"(__ptr) : "0"(ptr));      \
    (typeof(ptr)) (__ptr + (off)); })

#define __get_cpu_var(var) \
    (*RELOC_HIDE(&percpu__##var, vmm_percpu_current_offset()))

#define this_cpu(var)    __get_cpu_var(var)

#else

#define this_cpu(var)		percpu_##var

#endif

#define DEFINE_PER_CPU(type, name)				\
		__percpu __typeof__(type) percpu_##name

#define DECLARE_PER_CPU(type, name)				\
		extern __typeof__(type) percpu__##name

#define get_cpu_var(var) this_cpu(var)

#define put_cpu_var(var)

/** Retrive per-cpu offset of current cpu */
virtual_addr_t vmm_percpu_current_offset(void);

/** Initialize per-cpu areas */
int vmm_percpu_init(void);

#endif /* __VMM_PERCPU_H__ */
