/**
 * Copyright (c) 2016 Anup Patel.
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
 * @file vminfo.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Guest/VM Info driver source
 */
#ifndef __VMINFO_H__
#define __VMINFO_H__

#include <arm_types.h>

u32 vminfo_magic(virtual_addr_t base);
u32 vminfo_vendor(virtual_addr_t base);
u32 vminfo_version(virtual_addr_t base);
u32 vminfo_vcpu_count(virtual_addr_t base);
u32 vminfo_boot_delay(virtual_addr_t base);
physical_addr_t vminfo_ram_base(virtual_addr_t base, u32 bank);
physical_size_t vminfo_ram_size(virtual_addr_t base, u32 bank);

#endif
