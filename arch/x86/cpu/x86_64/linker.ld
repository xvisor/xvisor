/**
 * Copyright (c) 2012-13 Himanshu Chauhan.
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
 * @version 1.0
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 */
OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start_64)

SECTIONS 
{
	. = CPU_TEXT_START;

	PROVIDE(_code_start = .);

	.text :
 	{
		PROVIDE(_text_start = .);
		*(.start.text)
		*(.text)
		. = ALIGN(4);
		PROVIDE(_text_end = .);
	}

	.data :
	{
		PROVIDE(_data_start = .);
		*(.data)
		. = ALIGN(4);
		PROVIDE(_data_end = .);
	}

	.bss :
	{
		PROVIDE(_bss_start = .);
		*(.bss)
		. = ALIGN(4);
		PROVIDE(_bss_end = .);
	}

	.rodata :
	{
		PROVIDE(_rodata_start = .);
		*(.rodata .rodata.*)
		. = ALIGN(4);
		PROVIDE(_rodata_end = .);
	}

	.modtbl :
	{
		PROVIDE(_modtbl_start = .);
		*(.modtbl)
		. = ALIGN(4);
		PROVIDE(_modtbl_end = .);
	}

	.heap :
	{
		. = ALIGN(4096);
		PROVIDE(_heap_start = .);
		. = . + (CONFIG_HEAP_SIZE * 1024);
		. = ALIGN(4);
		PROVIDE(_eheap = .);
	}

	PROVIDE(_stack_end = .);
	. = . + CONFIG_IRQ_STACK_SIZE;
	. = ALIGN(4);
	PROVIDE(_stack_start = .);

	/* Interrupt stack */
	PROVIDE(_int_stack_end = .);
	. = . + CONFIG_IRQ_STACK_SIZE;
	. = ALIGN(4);
	PROVIDE(_int_stack = .);

	. = ALIGN(0x1000); /* Put init functions into separate pages */
	.init :
	{
		PROVIDE(_init_text_start = .);
		*(.init.text)
		. = ALIGN(4);
		PROVIDE(_init_text_end = .);
	}
	PROVIDE(_code_end = .);
}
