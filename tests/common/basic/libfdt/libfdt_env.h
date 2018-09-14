/*
 * libfdt - Flat Device Tree manipulation (build/run environment adaptation)
 * Copyright (C) 2007 Gerald Van Baren, Custom IDEAS, vanbaren@cideas.com
 * Original version written by David Gibson, IBM Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef _LIBFDT_ENV_H
#define _LIBFDT_ENV_H

#include <arch_types.h>
#include <basic_stdio.h>
#include <basic_string.h>

extern struct fdt_header *working_fdt;  /* Pointer to the working fdt */

#define fdt32_to_cpu(x)		be32_to_cpu(x)
#define cpu_to_fdt32(x)		cpu_to_be32(x)
#define fdt64_to_cpu(x)		be64_to_cpu(x)
#define cpu_to_fdt64(x)		cpu_to_be64(x)

#define memmove		basic_memmove
#define memcpy		basic_memcpy
#define memcmp		basic_memcmp
#define memchr		basic_memchr
#define memset		basic_memset
#define strchr		basic_strchr
#define strcpy		basic_strcpy
#define strcmp		basic_strcmp
#define strlen		basic_strlen

#define printf		basic_printf
#define sprintf		basic_sprintf
#define snprintf	basic_snprintf

#endif /* _LIBFDT_ENV_H */
