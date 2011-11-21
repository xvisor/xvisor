/**
 * Copyright (c) 2011 Pranav Sawargaonkar.
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
 * @file config.h
 * @version 1.0
 * @author Pranav Sawargaonkar (pranav.sawargaonkar@gmail.com)
 * @brief header file for OMAP3 configuration macors & defines
 */
#ifndef __OMAP3_CONFIG_H__
#define __OMAP3_CONFIG_H__

#define OMAP3_COM_FREQ   	48000000L
#define OMAP3_UART_BASE 	0x49020000
#define OMAP3_UART_BAUD 	115200
#define OMAP3_UART_INCLK 	OMAP3_COM_FREQ

#endif
