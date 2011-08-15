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
 * @file arm_string.c
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief source file for common string functions
 */

#include <arm_string.h>

char *arm_strcpy(char *dest, const char *src)
{
	u32 i;
	for (i = 0; src[i] != '\0'; ++i)
		dest[i] = src[i];
	dest[i] = '\0';
	return dest;
}

int arm_strcmp(const char *a, const char *b)
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

void arm_int2str(char * dst, int src)
{
	int val, count = 0, pos = 0;
	char intchars[] = "0123456789";

	val = src;
	while (val) {
		count++;
		val = val / 10;
	}

	val = src;
	while (val) {
		dst[count - pos - 1] = intchars[val % 10];
		pos++;
		val = val / 10;
	}

	dst[count] = '\0';
}

void arm_uint2hexstr(char * dst, unsigned int src)
{
	int ite, pos = 0;
	char hexchars[] = "0123456789ABCDEF";

	for (ite = 0; ite < 8; ite++) {
		if ((pos == 0) && !((src >> (4 * (8 - ite - 1))) & 0xF)) {
			continue;
		}
		dst[pos] = hexchars[(src >> (4 * (8 - ite - 1))) & 0xF];
		pos++;
	}

	if (pos == 0) {
		dst[pos] = '0';
		pos++;
	}

	dst[pos] = '\0';
}

void arm_ulonglong2hexstr(char *dst, unsigned long long src)
{
	int ite, pos = 0;
	char hexchars[] = "0123456789ABCDEF";

	for (ite = 0; ite < 16; ite++) {
		if ((pos == 0) && !((src >> (4 * (16 - ite - 1))) & 0xF)) {
			continue;
		}
		dst[pos] = hexchars[(src >> (4 * (16 - ite - 1))) & 0xF];
		pos++;
	}

	if (pos == 0) {
		dst[pos] = '0';
		pos++;
	}

	dst[pos] = '\0';
}

