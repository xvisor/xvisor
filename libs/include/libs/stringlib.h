/**
 * Copyright (c) 2010 Anup Patel.
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
 * @file stringlib.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for string library
 */
#ifndef __STRINGLIB_H__
#define __STRINGLIB_H__

#include <vmm_types.h>
#include <arch_config.h>

#define isspace(c) (c == ' ' || c == '\f' || c == '\n' || c == '\r' || c == '\t' || c == '\v')

/** Some of the string APIs (e.g. memset(), memcpy(), etc) can be 
 *  optionally implemented in arch code by specifying #define ARCH_HAS_XXXX 
 *  in arch_config.h where xxxx() would be the string API
 *
 *  Usually the arch dependent functions would have arch_ prefix
 *  but in this case we cannot uses this prefix for arch implementation 
 *  of string APIs because GCC generates implicity calls to some of the
 *  string APIs such as memcpy, memset, memzero, etc.
 */

size_t strlen(const char *s);

char *strcpy(char *dest, const char *src);

char *strncpy(char *dest, const char *src, size_t n);

size_t strlcpy(char *dest, const char *src, size_t n);

char *strcat(char *dest, const char *src);

char *strncat(char *dest, const char *src, size_t n);

int strcmp(const char *a, const char *b);

int strncmp(const char *a, const char *b, size_t n);

char *strchr(const char *s, int c);

char *strrchr(const char *s, int c);

char *strnchr(const char *s, size_t n, int c);

void str2lower(char * s);

void str2upper(char * s);

int str2int(const char *s, unsigned int base);

long long str2longlong(const char *s, unsigned int base);

unsigned int str2uint(const char *s, unsigned int base);

unsigned long str2ulong(const char *s, unsigned int base);

unsigned long long str2ulonglong(const char *s, unsigned int base);

int str2ipaddr(unsigned char *ipaddr, const char *str);

char *strpbrk(const char *cs, const char *ct);

char *strsep(char **s, const char *ct);

void *memcpy(void *dest, const void *src, size_t count);

void *memcpy_toio(void *dest, const void *src, size_t count);

void *memcpy_fromio(void *dest, const void *src, size_t count);

void *memmove(void *dest, const void *src, size_t count);

void *memset(void *dest, int c, size_t count);

void *memset_io(void *dest, int c, size_t count);

int memcmp(const void *s1, const void *s2, size_t count);

void *memchr(const void *s, int c, size_t n);

char *skip_spaces(const char *str);

#endif /* __STRINGLIB_H__ */
