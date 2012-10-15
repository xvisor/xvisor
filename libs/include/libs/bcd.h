/**
 * Copyright (c) 2012 Anup Patel.
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
 * @file bcd.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief binary coded decimal conversion library
 *
 * This header file is largely adapted from linux-xxx/include/linux/bcd.h
 *
 * The original code is licensed under the GPL.
 */

#ifndef __BCD_H__
#define __BCD_H__

/** BCD char to Binary char */
unsigned bcd2bin(unsigned char val);

/** Binary char to BCD char*/
unsigned char bin2bcd(unsigned val);

#endif /* __BCD_H__ */
