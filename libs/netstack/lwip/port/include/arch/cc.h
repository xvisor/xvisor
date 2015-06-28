/**
 * Copyright (c) 2013 Anup Patel.
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
 * @file arch/cc.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief basic system interface header required for lwIP
 */

#ifndef __LWIP_CC_H_
#define __LWIP_CC_H_

#include <vmm_error.h>
#include <vmm_types.h>
#include <vmm_compiler.h>
#include <vmm_heap.h>
#include <vmm_stdio.h>
#include <vmm_host_io.h>
#include <vmm_spinlocks.h>
#include <libs/stringlib.h>
#include <libs/mathlib.h>

/** Common types */
typedef u8 u8_t;
typedef u16 u16_t;
typedef u32 u32_t;
typedef s8 s8_t;
typedef s16 s16_t;
typedef s32 s32_t;
typedef virtual_addr_t mem_ptr_t;

/** Printf formatting macros */
#define U16_F	"u"
#define S16_F	"d"
#define X16_F	"x"
#define U32_F	"u"
#define S32_F	"d"
#define X32_F	"x"
#define SZT_F	"u"

/** Endianness macros */
#if !defined(CONFIG_CPU_BE)
#define BYTE_ORDER LITTLE_ENDIAN
#define LWIP_PLATFORM_BYTESWAP		1
#define LWIP_PLATFORM_HTONS(x)		vmm_cpu_to_be16(x)
#define LWIP_PLATFORM_HTONL(x)		vmm_cpu_to_be32(x)
#else
#define BYTE_ORDER BIG_ENDIAN
#endif

/** Structure packing macros */
#define PACK_STRUCT_BEGIN
#define PACK_STRUCT_FIELD(x)		x
#define PACK_STRUCT_STRUCT		__packed
#define PACK_STRUCT_END

/** Debug macros */
#define LWIP_PLATFORM_DIAG(x)		vmm_printf x
#define LWIP_PLATFORM_ASSERT(x)		vmm_panic(x)

/** Light-weight protection */
#define SYS_ARCH_DECL_PROTECT(x)	DEFINE_SPINLOCK(x)
#define SYS_ARCH_PROTECT(x)		vmm_spin_lock(&x)
#define SYS_ARCH_UNPROTECT(x)		vmm_spin_unlock(&x)

/** Use lwIP provided error codes */
#define LWIP_PROVIDE_ERRNO

#define LWIP_ERR_T			int

#endif /* __LWIP_CC_H_ */
