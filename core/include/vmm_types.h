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
 * @file vmm_types.h
 * @author Himanshu Chauhan (hchauhan@nulltrace.org)
 * @author Pavel Borzenkov <pavel.borzenkov@gmail.com>
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
typedef long long s64;
typedef unsigned long long u64;

typedef unsigned int bool;
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

#endif /* __VMM_TYPES_H__ */
