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
#define __noreturn		__attribute__((noreturn))
#define __notrace		__attribute__((no_instrument_function))
#define __packed		__attribute__((packed))

#define __read_mostly		__attribute__((section(".readmostly.data")))
#define __lock			__attribute__((section(".spinlock.text")))
#define __modtbl		__attribute__((section(".modtbl")))
#define __symtbl		__attribute__((section(".symtbl")))
#define __percpu		__attribute__((section(".percpu")))
#define __init			__attribute__((section(".init.text")))
#define __initconst		__attribute__((section(".init.data")))
#define __exit

/* Help in branch prediction */
#define likely(x) __builtin_expect((x), 1)
#define unlikely(x) __builtin_expect((x), 0)

#if __GNUC__ >= 4
#define __compiler_offsetof(type, member) __builtin_offsetof(type, member)
#endif

#endif /* __VMM_COMPILER_H__ */
