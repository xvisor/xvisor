/**
 * Copyright (c) 2013 Jean-Christophe.
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
 * @file imx-uart.h
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @brief Header file for IMX serial port driver.
 */

#ifndef __IMX_UART_H
#define __IMX_UART_H

#include <vmm_types.h>

bool imx_lowlevel_can_getc(virtual_addr_t base);
u8 imx_lowlevel_getc(virtual_addr_t base);
bool imx_lowlevel_can_putc(virtual_addr_t base);
void imx_lowlevel_putc(virtual_addr_t base, u8 ch);
void imx_lowlevel_init(virtual_addr_t base, u32 baudrate, u32 input_clock);

#endif /* __IMX_UART_H */
