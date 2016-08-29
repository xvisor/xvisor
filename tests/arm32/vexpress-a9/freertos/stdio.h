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
 * @file stdio.h
 * @author Philipp Ittershagen <pit@shgn.de>
 * @brief Header wrapper for FreeRTOS
 */

#ifndef STDIO_WRAPPER_H_INCLUDED_
#define STDIO_WRAPPER_H_INCLUDED_

#include <arm_types.h>
#include <arm_stdio.h>

#define sprintf arm_sprintf

#endif /* STDIO_WRAPPER_H_INCLUDED_ */
