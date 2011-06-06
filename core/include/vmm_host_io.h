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
 * @file vmm_host_io.h
 * @version 1.0
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief header file for common io functions.
 */

#ifndef __VMM_HOST_IO_H_
#define __VMM_HOST_IO_H_

#include <vmm_types.h>

/** I/O read/write legacy functions (Assumed to be Little Endian) */
u8 vmm_ioreadb(volatile void *addr);
void vmm_iowriteb(volatile void *addr, u8 data);
u16 vmm_ioreadw(volatile void *addr);
void vmm_iowritew(volatile void *addr, u16 data);
u32 vmm_ioreadl(volatile void *addr);
void vmm_iowritel(volatile void *addr, u32 data);

/** Memory read/write legacy functions (Assumed to be Little Endian) */
u8 vmm_readb(volatile void *addr);
void vmm_writeb(u8 data, volatile void *addr);
u16 vmm_readw(volatile void *addr);
void vmm_writew(u16 data, volatile void *addr);
u32 vmm_readl(volatile void *addr);
void vmm_writel(u32 data, volatile void *addr);

/** Memory read/write functions */
u8 vmm_in_8(volatile u8 * addr);
void vmm_out_8(volatile u8 * addr, u8 data);
u16 vmm_in_le16(volatile u16 * addr);
void vmm_out_le16(volatile u16 * addr, u16 data);
u16 vmm_in_be16(volatile u16 * addr);
void vmm_out_be16(volatile u16 * addr, u16 data);
u32 vmm_in_le32(volatile u32 * addr);
void vmm_out_le32(volatile u32 * addr, u32 data);
u32 vmm_in_be32(volatile u32 * addr);
void vmm_out_be32(volatile u32 * addr, u32 data);

#endif /* __VMM_HOST_IO_H_ */
