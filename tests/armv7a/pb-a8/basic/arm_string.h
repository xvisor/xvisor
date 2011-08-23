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
 * @file arm_string.h
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief Header file for common string functions
 */

#ifndef __ARM_STRING_H_
#define __ARM_STRING_H_

#include <arm_types.h>

char *arm_strcpy(char *dest, const char *src);
int arm_strcmp(const char *a, const char *b);
int arm_str2int(char * src);
void arm_int2str(char * dst, int src);
unsigned int arm_hexstr2uint(char * src);
void arm_uint2hexstr(char * dst, unsigned int src);
void arm_ulonglong2hexstr(char * dst, unsigned long long src);

#endif /* __ARM_STRING_H_ */
