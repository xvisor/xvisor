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
 * @file basic_string.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Header file for common string functions
 */

#ifndef __BASIC_STRING_H__
#define __BASIC_STRING_H__

#include <arch_types.h>

void *basic_memcpy(void *dest, const void *src, unsigned int count);
void *basic_memmove(void *dest, const void *src, unsigned int count);
void *basic_memset(void *dest, int c, unsigned int count);
int basic_memcmp(const void *s1, const void *s2, unsigned int count);
char *basic_memchr(const char *p, int ch, int count);
char *basic_strchr(const char *p, int ch);
char *basic_strcpy(char *dest, const char *src);
char *basic_strcat(char *dest, const char *src);
int basic_strcmp(const char *a, const char *b);
size_t basic_strlen(const char *s);
int basic_str2int(char * src);
void basic_int2str(char * dst, int src);
void basic_ulonglong2str(char * dst, unsigned long long src);
unsigned int basic_hexstr2uint(char * src);
unsigned long long basic_hexstr2ulonglong(char * src);
void basic_uint2hexstr(char * dst, unsigned int src);
void basic_ulonglong2hexstr(char * dst, unsigned long long src);
char *basic_strrchr(const char *src, int c);

#endif /* __BASIC_STRING_H__ */
