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
 * @file vmm_string.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for libc functions required by VMM
 */
#ifndef _VMM_STRING_H__
#define _VMM_STRING_H__

#include <vmm_types.h>

size_t vmm_strlen(const char *s);

char *vmm_strcpy(char *dest, const char *src);

char *vmm_strncpy(char *dest, const char *src, size_t n);

char *vmm_strcat(char *dest, const char *src);

int vmm_strcmp(const char *a, const char *b);

int vmm_strncmp(const char *a, const char *b, int n);

void vmm_str2lower(char * s);

void vmm_str2upper(char * s);

int vmm_str2int(const char *s, unsigned int base);

long long vmm_str2longlong(const char *s, unsigned int base);

unsigned int vmm_str2uint(const char *s, unsigned int base);

unsigned long long vmm_str2ulonglong(const char *s, unsigned int base);

void *vmm_memcpy(void *dest, const void *src, size_t count);

void *vmm_memmove(void *dest, const void *src, size_t count);

void *vmm_memset(void *dest, int c, size_t count);

int vmm_memcmp(const void *s1, const void *s2, size_t count);

#endif
