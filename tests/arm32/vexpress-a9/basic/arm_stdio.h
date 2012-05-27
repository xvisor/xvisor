/**
 * Copyright (c) 2012 Sukanto Ghosh.
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
 * @file arm_stdio.h
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief Header file for common input/output functions
 *
 * Adapted from tests/arm32/pb-a8/basic/arm_stdio.h
 */

#ifndef __ARM_STDIO_H_
#define __ARM_STDIO_H_

#include <arm_types.h>

void arm_putc(char ch);
char arm_getc(void);
void arm_stdio_init(void);
void arm_puts(const char * str);
void arm_gets(char *s, int maxwidth, char endchar);

#endif /* __ARM_STDIO_H_ */
