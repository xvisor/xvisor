/**
 * Copyright (c) 2010-20 Himanshu Chauhan.
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
 * @file brd_console.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief main source file for serial port in qemu-mips board.
 */

#include <vmm_types.h>
#include <vmm_error.h>
#include <vmm_host_aspace.h>

extern void cls();
extern void putch(unsigned char c);
extern void settextcolor(u8 forecolor, u8 backcolor);
extern void init_console(void);

int arch_defterm_getc(u8 *ch)
{
	return VMM_OK;
}

int arch_defterm_putc(u8 ch)
{
        putch(ch);

	return VMM_OK;
}

int __init arch_defterm_init(void)
{
        init_console();
	return VMM_OK;
}
