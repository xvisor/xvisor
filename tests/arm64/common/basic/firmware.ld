/**
 * Copyright (c) 2013 Sukanto Ghosh.
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
 * @file firmware.ld
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief Linker script for basic firmware
 */

OUTPUT_FORMAT("elf64-littleaarch64", "elf64-littleaarch64", "elf64-littleaarch64")
OUTPUT_ARCH("aarch64")
ENTRY(_start)

SECTIONS 
{
	. = TEXT_START;

	PROVIDE(_code_start = .);
	PROVIDE(_reloc_region_start = .);

	.text :
	{
		*(.entry)
		*(.text)
		. = ALIGN(8);
		_etext = .;
	}

	. = ALIGN(4096);

	.data :
	{
		*(.data)
		. = ALIGN(8);
		_edata = .;
	}

	. = ALIGN(4096);

	.rodata :
	{
		*(.rodata .rodata.*)
		. = ALIGN(8);
		_erodata = .;
	}

	. = ALIGN(4096);

	PROVIDE(_reloc_region_end = .);

	PROVIDE(_zero_region_start = .);

	.bss :
	{
		PROVIDE(_bss_start = .);
		*(.bss)
		. = ALIGN(8);
		_ebss = .;
		PROVIDE(_bss_end = .);
	}

	. = ALIGN(4096);

	.heap :
	{
		PROVIDE(_heap_start = .);
		*(.heap)
		. = . + 4096;
		. = ALIGN(8);
		PROVIDE(_heap_end = .);
	}

	. = ALIGN(4096);

	PROVIDE(_zero_region_end = .);

	.svc_stack :
	{
		PROVIDE(_svc_stack_start = .);
		. = . + 4096;
		. = ALIGN(8);
		PROVIDE(_svc_stack_end = .);
	}

	.usr_stack :
	{
		PROVIDE(_usr_stack_start = .);
		. = . + 4096;
		. = ALIGN(8);
		PROVIDE(_usr_stack_end = .);
	}

	PROVIDE(_code_end = .);
}
