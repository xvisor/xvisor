/**
 * Copyright (c) 2019 Western Digital Corporation or its affiliates.
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
 * @file cpu_init.h
 * @author Anup Patel (anup.patel@wdc.com)
 * @brief RISC-V CPU init functions
 */

#ifndef _CPU_INIT_H__
#define _CPU_INIT_H__

#include <vmm_types.h>

/* Parse HW capabilities from device tree */
int cpu_parse_devtree_hwcap(void);

/* Init function called from low-level boot-up */
void cpu_init(void);

#endif
