/**
 * Copyright (c) 2019 Anup Patel.
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
 * @file uart8250.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Header file for UART 8250 serial port driver.
 */

#ifndef __UART8250_H_
#define __UART8250_H_

#include <arch_types.h>

bool uart8250_can_getc(virtual_addr_t base, u32 reg_shift, u32 reg_width);

char uart8250_getc(virtual_addr_t base, u32 reg_shift, u32 reg_width);

void uart8250_putc(virtual_addr_t base,
		   u32 reg_shift, u32 reg_width, char ch);

void uart8250_init(virtual_addr_t base,
		   u32 reg_shift, u32 reg_width,
		   u32 baudrate, u32 input_clock);

#endif /* __UART8250_H_ */
