/*
 * This file is part of Freax kernel.
 * Copyright (c) Himanshu Chauhan 2009-10.
 * All rights reserved.
 * 
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
 */

#ifndef _CPU_SYSTEM_H
#define _CPU_SYSTEM_H

#include <vmm_types.h>

extern u8 ioreadb (void *addr);
extern void iowriteb (void *addr, u8 data);
extern u32 get_cp0_prid(void);
extern u32 get_cp0_config(void);
extern u32 get_cp0_config1(void);

typedef union prid_ {
	struct prd {
		u32 companyopt:8;
		u32 companyid:8;
		u32 cpuid:8;
		u32 revision:8;
	}idfields;
	u32 prid;
}cp0_prid_t;

#endif /* _CPU_SYSTEM_H */
