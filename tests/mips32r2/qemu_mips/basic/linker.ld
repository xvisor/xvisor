/**
 * Copyright (c) 2010 Himanshu Chauhan.
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
 * @file linker.ld
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief Board specific linker script
 */

OUTPUT_FORMAT("elf32-tradbigmips")
OUTPUT_ARCH("mips")
ENTRY(_start)

SECTIONS 
{
	. = 0x3FC00000;
	.text :
 	{
		*(.start.text)
		*(.text)
		. = ALIGN(4);
		_etext = .;
	}

	.data :
	{
		*(.data)
		. = ALIGN(4);
		_edata = .;
	}

	.bss :
	{
		*(.bss)
		. = ALIGN(4);
		_ebss = .;
	}

	.rodata :
	{
		*(.rodata .rodata.*)
		. = ALIGN(4);
		_erodata = .;
	}

	PROVIDE(_stack_end = .);
	. = . + 4096;
	. = ALIGN(4);
	PROVIDE(_stack_start = .);
}
