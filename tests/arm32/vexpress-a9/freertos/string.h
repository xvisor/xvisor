/**
 * Copyright (c) 2016 Philipp Ittershagen.
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
 * @file string.h
 * @author Philipp Ittershagen <pit@shgn.de>
 * @brief Header wrapper for FreeRTOS
 */

#ifndef STRING_WRAPPER_H_INCLUDED_
#define STRING_WRAPPER_H_INCLUDED_

#include <arm_string.h>

#define memcpy arm_memcpy
#define memset arm_memset
#define strlen arm_strlen
#define strcpy arm_strcpy

#endif /* STRING_WRAPPER_H_INCLUDED_ */
