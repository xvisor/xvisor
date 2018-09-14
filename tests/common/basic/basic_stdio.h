/**
 * Copyright (c) 2011 Anup Patel.
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
 * @file basic_stdio.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Header file for common input/output functions
 */

#ifndef __BASIC_STDIO_H__
#define __BASIC_STDIO_H__

#include <arch_types.h>

#define __printf(a, b)		__attribute__((format(printf, a, b)))

bool basic_isprintable(char ch);
void basic_putc(char ch);
bool basic_can_getc(void);
char basic_getc(void);
void basic_stdio_init(void);
void basic_puts(const char * str);
void basic_gets(char *s, int maxwidth, char endchar);
int __printf(2, 3) basic_sprintf(char *out, const char *format, ...);
int __printf(3, 4) basic_snprintf(char *out, u32 out_sz, const char *format, ...);
int __printf(1, 2) basic_printf(const char *format, ...);

#endif /* __BASIC_STDIO_H__ */
