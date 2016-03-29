/**
 * Copyright (c) 2010 Himanshu Chauhan.
 *               2016 Open Wide
 *               2016 Institut de Recherche Technologique SystemX
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
 * @file vmm_types.h
 * @author Himanshu Chauhan (hchauhan@nulltrace.org)
 * @author Pavel Borzenkov (pavel.borzenkov@gmail.com)
 * @author Jean Guyomarc'h (jean.guyomarch@openwide.fr)
 * @brief header file for common types used in xvisor.
 */

#ifndef __VMM_TYPES_H__
#define __VMM_TYPES_H__

typedef char s8;
typedef short s16;
typedef int s32;
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
#ifndef CONFIG_64BIT
typedef long long s64;
typedef unsigned long long u64;
#else
typedef long s64;
typedef unsigned long u64;
#endif

typedef unsigned int bool;
typedef unsigned int size_t;
typedef signed int ssize_t;
typedef unsigned long ulong;
typedef signed int off_t;
typedef signed long long loff_t;

/** Boolean macros */
#define TRUE			1
#define FALSE			0
#define true			TRUE
#define false			FALSE	
#define NULL 			((void *)0)

#include <arch_types.h>

typedef physical_addr_t dma_addr_t;
typedef physical_addr_t resource_addr_t;
typedef physical_size_t resource_size_t;


/* The following macros are part of POSIX.1 requirements for
 * inttypes.h. They provide a portable interface to the printf-like
 * functions that takes a format as input. */

/* Decimal notation: signed integers */
#define PRId8		"d"
#define PRId16		"d"
#define PRId32		"d"
#define PRId64		__ARCH_PRI64_PREFIX "d"
#define PRIi8		"i"
#define PRIi16		"i"
#define PRIi32		"i"
#define PRIi64		__ARCH_PRI64_PREFIX "i"

/* Decimal notation: unsigned integers */
#define PRIu8		"u"
#define PRIu16		"u"
#define PRIu32		"u"
#define PRIu64		__ARCH_PRI64_PREFIX "u"

/* Hexadecimal notation, lowercase */
#define PRIx8		"x"
#define PRIx16		"x"
#define PRIx32		"x"
#define PRIx64		__ARCH_PRI64_PREFIX "x"

/* Hexadecimal notation, uppercase */
#define PRIX8		"X"
#define PRIX16		"X"
#define PRIX32		"X"
#define PRIX64		__ARCH_PRI64_PREFIX "X"

/* Non-standard. Used to print addresses and their sizes. Avoids
 * to write endless tests to print addresses in function of the
 * architecture. */
#define PRIADDR		"0" __ARCH_PRIADDR_DIGITS __ARCH_PRIADDR_PREFIX "X"
#define PRIADDR64	"016" __ARCH_PRI64_PREFIX "X"
#define PRISIZE		__ARCH_PRISIZE_PREFIX "u"

#define PRIPADDR	"0" __ARCH_PRIPADDR_DIGITS __ARCH_PRIPADDR_PREFIX "X"
#define PRIPADDR64	"016" __ARCH_PRI64_PREFIX "X"
#define PRIPSIZE	__ARCH_PRIPSIZE_PREFIX "u"

#endif /* __VMM_TYPES_H__ */
