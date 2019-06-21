/**
 * Copyright (c) 2018 Anup Patel.
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
 * @file riscv_lrsc.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Load reserve and store clear macros & defines
 */
#ifndef __RISCV_LRSC_H__
#define __RISCV_LRSC_H__

#ifndef __ASSEMBLY__

#define __xchg(ptr, new, size)						\
({									\
	__typeof__(ptr) __ptr = (ptr);					\
	__typeof__(*(ptr)) __new = (new);				\
	__typeof__(*(ptr)) __ret;					\
	register unsigned int __rc;					\
	switch (size) {							\
	case 4:								\
		__asm__ __volatile__ (					\
			"0:	lr.w %0, %2\n"				\
			"	sc.w.rl %1, %z3, %2\n"			\
			"	bnez %1, 0b\n"				\
			"	fence rw, rw\n"				\
			: "=&r" (__ret), "=&r" (__rc), "+A" (*__ptr)	\
			: "rJ" (__new)					\
			: "memory");					\
		break;							\
	case 8:								\
		__asm__ __volatile__ (					\
			"0:	lr.d %0, %2\n"				\
			"	sc.d.rl %1, %z3, %2\n"			\
			"	bnez %1, 0b\n"				\
			"	fence rw, rw\n"				\
			: "=&r" (__ret), "=&r" (__rc), "+A" (*__ptr)	\
			: "rJ" (__new)					\
			: "memory");					\
		break;							\
	default:							\
		break;							\
	}								\
	__ret;								\
})

#define xchg(ptr, n)							\
({									\
	__typeof__(*(ptr)) _n_ = (n);					\
	(__typeof__(*(ptr))) __xchg((ptr), _n_, sizeof(*(ptr)));	\
})

#define __cmpxchg(ptr, old, new, size)					\
({									\
	__typeof__(ptr) __ptr = (ptr);					\
	__typeof__(*(ptr)) __old = (old);				\
	__typeof__(*(ptr)) __new = (new);				\
	__typeof__(*(ptr)) __ret;					\
	register unsigned int __rc;					\
	switch (size) {							\
	case 4:								\
		__asm__ __volatile__ (					\
			"0:	lr.w %0, %2\n"				\
			"	bne  %0, %z3, 1f\n"			\
			"	sc.w.rl %1, %z4, %2\n"			\
			"	bnez %1, 0b\n"				\
			"	fence rw, rw\n"				\
			"1:\n"						\
			: "=&r" (__ret), "=&r" (__rc), "+A" (*__ptr)	\
			: "rJ" (__old), "rJ" (__new)			\
			: "memory");					\
		break;							\
	case 8:								\
		__asm__ __volatile__ (					\
			"0:	lr.d %0, %2\n"				\
			"	bne %0, %z3, 1f\n"			\
			"	sc.d.rl %1, %z4, %2\n"			\
			"	bnez %1, 0b\n"				\
			"	fence rw, rw\n"				\
			"1:\n"						\
			: "=&r" (__ret), "=&r" (__rc), "+A" (*__ptr)	\
			: "rJ" (__old), "rJ" (__new)			\
			: "memory");					\
		break;							\
	default:							\
		break;							\
	}								\
	__ret;								\
})

#define cmpxchg(ptr, o, n)						\
({									\
	__typeof__(*(ptr)) _o_ = (o);					\
	__typeof__(*(ptr)) _n_ = (n);					\
	(__typeof__(*(ptr))) __cmpxchg((ptr),				\
				       _o_, _n_, sizeof(*(ptr)));	\
})

#define clrx()								\
({									\
	volatile unsigned long __tmp = 0;				\
	switch (sizeof(__tmp)) {					\
	case 4:								\
		__asm__ __volatile__ (					\
			"	sc.w zero, zero, %0\n"			\
			: "+A" (__tmp) : : "memory");			\
		break;							\
	case 8:								\
		__asm__ __volatile__ (					\
			"	sc.d zero, zero, %0\n"			\
			: "+A" (__tmp) : : "memory");			\
		break;							\
	default:							\
		break;							\
	}								\
})

#endif /* __ASSEMBLY__ */

#endif
