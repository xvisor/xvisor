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
 * @file vmm_compiler.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for compiler specific quirks.
 */

#ifndef __VMM_COMPILER_H__
#define __VMM_COMPILER_H__

#define stringify(s)		tostring(s)
#define tostring(s)		#s
#define barrier() 		__asm__ __volatile__("": : :"memory")

#define __always_inline 	__attribute__((always_inline))
#define __unused		__attribute__((unused))
#define __used			__attribute__((used))
#define __aligned(x)		__attribute__((aligned(x)))
#define __noreturn		__attribute__((noreturn))
#define __notrace		__attribute__((no_instrument_function))
#define __packed		__attribute__((packed))
#define __weak			__attribute__((weak))

#define __section(S)		__attribute__((section(#S)))
#define __read_mostly		__section(".readmostly.data")
#define __lock			__section(".spinlock.text")
#define __modtbl		__section(".modtbl")
#define __symtbl		__section(".symtbl")
#define __percpu		__section(".percpu")
#define __init			__section(".init.text")
#define __initconst		__section(".init.data")
#define __initdata		__section(".init.data")
#define __exit

#if defined(CONFIG_SMP)
#define __cpuinit		__section(".cpuinit.text")
#define __cpuexit
#else
#define __cpuinit		__init
#define __cpuexit		__exit
#endif

/* Help in branch prediction */
#define likely(x)		__builtin_expect(!!(x), 1)
#define unlikely(x)		__builtin_expect(!!(x), 0)

#if __GNUC__ >= 4
#define __compiler_offsetof(type, member) __builtin_offsetof(type, member)
#endif

#endif /* __VMM_COMPILER_H__ */
