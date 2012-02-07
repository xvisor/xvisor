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
 * @file vmm_string.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief source file for CPU specific string functions required by VMM
 */

#include <vmm_types.h>
#include <vmm_string.h>

size_t vmm_strlen(const char *s)
{
	size_t ret = 0;
	while (s[ret]) {
		ret++;
	}
	return ret;
}

char *vmm_strcpy(char *dest, const char *src)
{
	u32 i;
	for (i = 0; src[i] != '\0'; ++i)
		dest[i] = src[i];
	dest[i] = '\0';
	return dest;
}

char *vmm_strncpy(char *dest, const char *src, size_t n)
{
	u32 i;
	for (i = 0; src[i] != '\0' && n; ++i, n--)
		dest[i] = src[i];
	dest[i] = '\0';
	return dest;
}

char *vmm_strcat(char *dest, const char *src)
{
	char *save = dest;

	for (; *dest; ++dest) ;
	while ((*dest++ = *src++) != 0) ;

	return (save);
}

int vmm_strcmp(const char *a, const char *b)
{
	while (*a == *b) {
		if (*a == '\0' || *b == '\0') {
			return (unsigned char)*a - (unsigned char)*b;
		}
		++a;
		++b;
	}
	return (unsigned char)*a - (unsigned char)*b;
}

int vmm_strncmp(const char *a, const char *b, int n)
{
	if (n == 0)
		return 0;
	while (*a == *b && n > 0) {
		if (*a == '\0' || *b == '\0') {
			if (n) {
				return (unsigned char)*a - (unsigned char)*b;
			} else {
				return 0;
			}
		}
		++a;
		++b;
		--n;
	}
	if (n) {
		return (unsigned char)*a - (unsigned char)*b;
	} else {
		return 0;
	}
}

void vmm_str2lower(char * s)
{
	if (!s) {
		return;
	}

	while (*s) {
		if ('A' <= *s && *s <= 'Z') {
			*s = (*s - 'A') + 'a';
		}
		s++;
	}
}

void vmm_str2upper(char * s)
{
	if (!s) {
		return;
	}

	while (*s) {
		if ('a' <= *s && *s <= 'z') {
			*s = (*s - 'a') + 'A';
		}
		s++;
	}
}

long long vmm_str2longlong(const char *s, unsigned int base)
{
	long long val = 0;
	unsigned int digit;
	int neg = 0;

	if (base < 2 || base > 16)
		return 0;

	while (*s == ' ' || *s == '\t') {
		s++;
	}

	if (*s == '-') {
		neg = 1;
		s++;
	} else if (*s == '+') {
		s++;
	}

	if ((*s == '0') && (*(s+1) == 'x')) {
		base = 16;
		s++;
		s++;
	}

	while (*s) {
		if ('A' <= *s && *s <= 'F')
			digit = 10 + (*s - 'A');
		else if ('a' <= *s && *s <= 'f')
			digit = 10 + (*s - 'a');
		else if ('0' <= *s && *s <= '9')
			digit = *s - '0';
		else
			digit = 0;

		val = val * base + digit;

		s++;
	}

	if (neg) {
		return -val;
	}

	return val;
}

int vmm_str2int(const char *s, unsigned int base)
{
	return vmm_str2longlong(s, base);
}

unsigned long long vmm_str2ulonglong(const char *s, unsigned int base)
{
	unsigned long long val = 0;
	unsigned int digit;

	if (base < 2 || base > 16)
		return 0;

	while (*s == ' ' || *s == '\t') {
		s++;
	}

	if ((*s == '0') && (*(s+1) == 'x')) {
		base = 16;
		s++;
		s++;
	}

	while (*s) {
		if ('A' <= *s && *s <= 'F')
			digit = 10 + (*s - 'A');
		else if ('a' <= *s && *s <= 'f')
			digit = 10 + (*s - 'a');
		else if ('0' <= *s && *s <= '9')
			digit = *s - '0';
		else
			digit = 0;

		val = val * base + digit;

		s++;
	}

	return val;
}

unsigned int vmm_str2uint(const char *s, unsigned int base)
{
	return vmm_str2ulonglong(s, base);
}

void *vmm_memcpy(void *dest, const void *src, size_t count)
{
	u8 *dst8 = (u8 *) dest;
	u8 *src8 = (u8 *) src;

	if (count & 1) {
		dst8[0] = src8[0];
		dst8 += 1;
		src8 += 1;
	}

	count /= 2;
	while (count--) {
		dst8[0] = src8[0];
		dst8[1] = src8[1];

		dst8 += 2;
		src8 += 2;
	}

	return dest;
}

void *vmm_memset(void *dest, int c, size_t count)
{
	u8 *dst8 = (u8 *) dest;
	u8 ch = (u8) c;

	if (count & 1) {
		dst8[0] = ch;
		dst8 += 1;
	}

	count /= 2;
	while (count--) {
		dst8[0] = ch;
		dst8[1] = ch;
		dst8 += 2;
	}

	return dest;
}

int vmm_memcmp(const void *s1, const void *s2, size_t count)
{
	u8 *p1 = (u8 *) s1;
	u8 *p2 = (u8 *) s2;
	if (count != 0) {
		do {
			if (*p1++ != *p2++)
				return (*--p1 - *--p2);
		} while (--count != 0);
	}
	return (0);
}

