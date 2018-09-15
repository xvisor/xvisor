/*
 ****************************************************************************
 *
 *                   "DHRYSTONE" Portability Section
 *                   -------------------------------
 *                                                                            
 *  Version:    C, Version 2.1
 *                                                                            
 *  File:       dhry_port.c (for portability)
 *
 *  Date:       Aug 15, 2011
 *
 *  Author:     Anup Patel (anup@brainfault.org)
 *
 ****************************************************************************
 */

#include <arch_board.h>
#include <basic_heap.h>
#include <basic_stdio.h>
#include <basic_string.h>
#include "dhry_port.h"

extern u8 _heap_start;

void *dhry_malloc(unsigned int size)
{
	return basic_malloc(size);
}

TimeStamp dhry_timestamp(void)
{
	return arch_udiv64(arch_board_timer_timestamp(), 1000);
}

long dhry_to_microsecs(TimeStamp UserTime)
{
	return UserTime;
}

long dhry_iter_per_sec(TimeStamp UserTime, int Number_Of_Runs)
{
	return arch_udiv64(((TimeStamp)Number_Of_Runs * (TimeStamp)1000000),
								 UserTime);
}

int dhry_strcmp(char *dst, char *src)
{
	return basic_strcmp(dst, src);
}

void dhry_strcpy(char *dst, char *src)
{
	basic_strcpy(dst, src);
}

void dhry_printc(char ch)
{
	char tmp[2];
	tmp[0] = ch;
	tmp[1] = '\0';
	basic_puts(tmp);
}

void dhry_prints(char *str)
{
	basic_puts(str);
}

void dhry_printi(int val)
{
	char tmp[128];
	basic_int2str(tmp, val);
	basic_puts(tmp);
}

void dhry_printl(unsigned long val)
{
	char tmp[128];
	basic_ulonglong2str(tmp, (unsigned long long)val);
	basic_puts(tmp);
}
