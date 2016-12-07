/**
 * Copyright (c) 2013 Jean-Christophe Dubois.
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
 * @file vmm_limits.h
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @brief header file for common limits in xvisor.
 */

#ifndef __VMM_LIMITS_H__
#define __VMM_LIMITS_H__

#include <vmm_types.h>

#define USHRT_MAX	((u16)(~0U))
#define SHRT_MAX	((s16)(USHRT_MAX>>1))
#define SHRT_MIN	((s16)(-SHRT_MAX - 1))
#define INT_MAX		((int)(~0U>>1))
#define INT_MIN		(-INT_MAX - 1)
#define UINT_MAX	(~0U)
#define LONG_MAX	((long)(~0UL>>1))
#define LONG_MIN	(-LONG_MAX - 1)
#define ULONG_MAX	(~0UL)
#define LLONG_MAX	((long long)(~0ULL>>1))
#define LLONG_MIN	(-LLONG_MAX - 1)
#define ULLONG_MAX	(~0ULL)
#define SIZE_MAX	(~(size_t)0)

#define U8_MAX		((u8)~0U)
#define S8_MAX		((s8)(U8_MAX>>1))
#define S8_MIN		((s8)(-S8_MAX - 1))
#define U16_MAX		((u16)~0U)
#define S16_MAX		((s16)(U16_MAX>>1))
#define S16_MIN		((s16)(-S16_MAX - 1))
#define U32_MAX		((u32)~0U)
#define S32_MAX		((s32)(U32_MAX>>1))
#define S32_MIN		((s32)(-S32_MAX - 1))
#define U64_MAX		((u64)~0ULL)
#define S64_MAX		((s64)(U64_MAX>>1))
#define S64_MIN		((s64)(-S64_MAX - 1))

#define SZ_1		0x00000001
#define SZ_2		0x00000002
#define SZ_4		0x00000004
#define SZ_8		0x00000008
#define SZ_16		0x00000010
#define SZ_32		0x00000020
#define SZ_64		0x00000040
#define SZ_128		0x00000080
#define SZ_256		0x00000100
#define SZ_512		0x00000200

#define SZ_1K		0x00000400
#define SZ_2K		0x00000800
#define SZ_4K		0x00001000
#define SZ_8K		0x00002000
#define SZ_16K		0x00004000
#define SZ_32K		0x00008000
#define SZ_64K		0x00010000
#define SZ_128K		0x00020000
#define SZ_256K		0x00040000
#define SZ_512K		0x00080000

#define SZ_1M		0x00100000
#define SZ_2M		0x00200000
#define SZ_4M		0x00400000
#define SZ_8M		0x00800000
#define SZ_16M		0x01000000
#define SZ_32M		0x02000000
#define SZ_64M		0x04000000
#define SZ_128M		0x08000000
#define SZ_256M		0x10000000
#define SZ_512M		0x20000000

#define SZ_1G		0x40000000
#define SZ_2G		0x80000000

#define VMM_FIELD_LICENSE_SIZE		32
#define VMM_FIELD_AUTHOR_SIZE		32
#define VMM_FIELD_TYPE_SIZE		32
#define VMM_FIELD_SHORT_NAME_SIZE	32
#define VMM_FIELD_NAME_SIZE		64
#define VMM_FIELD_DESC_SIZE		128
#define VMM_FIELD_COMPAT_SIZE		128

#endif /* __VMM_LIMITS_H__ */
