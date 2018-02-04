/**
 * Copyright (c) 2018 Himanshu Chauhan.
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
 * @file processor.h
 * @author Himanshu Chauhan (hchauhan@xvisor-x86.org)
 * @brief architectural functions (serialize etc.)
 */

#ifndef _PROCESSOR_H
#define _PROCESSOR_H

/*
 * Force the processor to complete all modifications to flags,
 * registers, and memory by previous instructions and to drain
 * all buffered writes to memory before the next instruction
 * is fetched and executed.
 *
 * CPUID can be executed at any privilege level to serialize
 * instruction execution with no effect on program flow, except
 * that the EAX, EBX, ECX, and EDX registers are modified.
 */
static inline void __sync(void)
{
	int t;
	asm volatile("cpuid" : "=a"(t) : "0"(1):"ebx","ecx","edx","memory");
}

static inline void prefetch(void *d)
{
	asm volatile("prefetch0 %0" :: "m" (*(unsigned long *)d));
}

static inline void rep_nop(void)
{
	asm volatile("rep; nop" ::: "memory");
}

#define rdtscl(low)						\
	__asm__ __volatile__("rdtsc" : "=a"(low) :: "edx")

#define rdtscll(val) do {					\
		unsigned int __a, __d;				\
		asm volatile("rdtsc" : "=a"(__a), "=d"(__d));	\
		(val) = ((u64)__a) | (((u64)__d) << 32);	\
} while(0);

#endif /* _PROCESSOR_H */
