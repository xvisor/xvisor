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
ENTRY(_start_32)

SECTIONS 
{
	. = CPU_TEXT_LMA;

	PROVIDE(_code_bootstrap_start = .);

        .bootstrap.text :
        {
                PROVIDE(_text_bootstrap_start = .);
                *(.bootstrap.text)
                PROVIDE(_text_bootstrap_end = .);
        }

        . = ALIGN(4);
        .bootstrap_readonly :
        {
                *(.bootstrap_readonly)
        }

        /*
         * We use the first few pages for initial page
         * page tables. For this the start of PML4 should
         * be aligned to page size.
         */
        . = ALIGN(4096);
        .bootstrap.bss :
        {
                PROVIDE(_bss_bootstrap_start = .);
                *(.bootstrap.bss)
                . = ALIGN(4);
                . += 4096;
                PROVIDE(_bss_bootstrap_end = .);
        }

        . = ALIGN(4096);
        PROVIDE(_code_bootstrap_end = .);

        . += CPU_TEXT_VMA;

        PROVIDE(_code_start = .);

	.text : AT(ADDR(.text) - CPU_TEXT_VMA)
 	{
		PROVIDE(_text_start = .);
		*(.start.text)
		*(.text)
		. = ALIGN(4);
		PROVIDE(_text_end = .);
	}

	.data : AT(ADDR(.data) - CPU_TEXT_VMA)
	{
		PROVIDE(_data_start = .);
		*(.data)
		. = ALIGN(4);
		PROVIDE(_data_end = .);
	}

	.bss : AT(ADDR(.bss) - CPU_TEXT_VMA)
	{
		PROVIDE(_bss_start = .);
		*(.bss)
		. = ALIGN(4);
		PROVIDE(_bss_end = .);
	}

	.rodata : AT(ADDR(.rodata) - CPU_TEXT_VMA)
	{
		PROVIDE(_rodata_start = .);
		*(.rodata .rodata.*)
		. = ALIGN(4);
		PROVIDE(_rodata_end = .);
	}

	.modtbl : AT(ADDR(.modtbl) - CPU_TEXT_VMA)
	{
		PROVIDE(_modtbl_start = .);
		*(.modtbl)
		. = ALIGN(4);
		PROVIDE(_modtbl_end = .);
	}

	.heap : AT(ADDR(.heap) - CPU_TEXT_VMA)
	{
		. = ALIGN(4096);
		PROVIDE(_heap_start = .);
		. = . + (CONFIG_HEAP_SIZE * 1024);
		. = ALIGN(4);
		PROVIDE(_eheap = .);
	}

	.init : AT(ADDR(.heap) - CPU_TEXT_VMA)
	{
		PROVIDE(_init_text_start = .);
		*(.init.text)
		. = ALIGN(4);
		PROVIDE(_init_text_end = .);
	}
	PROVIDE(_code_end = .);
}
