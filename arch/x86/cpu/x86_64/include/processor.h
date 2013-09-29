/**
 * Copyright (c) 2013 Himanshu Chauhan.
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
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief Header file for declaring CPU specific information.
 */
#ifndef _PROCESSOR_H__
#define _PROCESSOR_H__

#include <vmm_types.h>
#include <arch_cache.h>

#define PROCESSOR_NAME_STRING_LEN	48
#define PROCESSOR_VENDOR_ID_LEN		16

struct cpuinfo_x86 {
	u8 family;
	u8 model;
	u8 stepping;
	u8 vendor_string[PROCESSOR_VENDOR_ID_LEN];
	u8 name_string[PROCESSOR_NAME_STRING_LEN];
	u8 virt_bits;
	u8 phys_bits;
	u8 cpuid_level;
	u8 l1_dcache_size;
	u8 l1_dcache_line_size;
	u8 l1_icache_size;
	u8 l1_icache_line_size;
	u16 l2_cache_size;
	u16 l2_cache_line_size;
	u8 hw_virt_available;
}__aligned(ARCH_CACHE_LINE_SIZE);

#endif /* _PROCESSOR_H__ */
