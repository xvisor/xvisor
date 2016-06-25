/**
 * Copyright (c) 2015 Himanshu Chauhan.
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
 * @file cpu_string.c
 * @author Himanshu Chauhan (hchauhan@xvisor-x86.org)
 * @brief Low level architecture specific code for string operations.
 */

#include <vmm_types.h>

void *memcpy(void *dest, const void *src, size_t count)
{
	int d0, d1, d2;

	asm volatile("rep ; movsl\n\t"
		     "movl %4, %%ecx\n\t"
		     "andl $3, %%ecx\n\t"
		     "jz 1f\n\t"
		     "rep ; movsb\n\t"
		     "1:"
		     :"=&c"(d0), "=&D"(d1), "=&S"(d2)
		     :""(count/4), "g"(count), "1"((long)dest), "2"((long)src)
		     :"memory");

	return dest;
}
