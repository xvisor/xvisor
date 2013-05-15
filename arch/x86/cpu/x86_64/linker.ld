/**
 * Copyright (c) 2012 Himanshu Chauhan.
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

        PROVIDE(_code_start = .);

        .mboot : { *(.mboot) }

        . = ALIGN(4);
	.text :
 	{
		*(.text)
	}

	.data :
	{
		*(.data)
	}

	.bss :
	{
		*(.bss)
	}

	.rodata :
	{
		*(.rodata .rodata.*)
	}

	.modtbl :
	{
		PROVIDE(_modtbl_start = .);
		*(.modtbl)
		. = ALIGN(8);
		PROVIDE(_modtbl_end = .);
	}

	.percpu :
	{
		PROVIDE(_percpu_start = .);
		*(.percpu)
		. = ALIGN(8);
		PROVIDE(_percpu_end = .);
	}

        .stack :
        {
                . = ALIGN(4096);
                PROVIDE(_stack_end = .);
                . += 8192;
                PROVIDE(_stack_start = .);
		/* IST Stacks */
		PROVIDE(_ist_stacks_end = .);
		. += 0x7000; /* 7 stacks for size 4K each */
		PROVIDE(_ist_stacks_start = .);
        }

	.init :
	{
		PROVIDE(_init_start = .);
		*(.init.*)
		. = ALIGN(4096);

        	PROVIDE(_code_bootstrap_start = .);
                *(.bootstrap.text)

                /*
                 * We have initial page tables in .bootstrap.bss
                 * So beginning should be page aligned.
                 */
                . = ALIGN(4096);
                KEEP(*(.bootstrap.bss))
                . = ALIGN(4);
                . += 4096;
                PROVIDE(_bootstrap_stack_start = .);

                . = ALIGN(8192);

                PROVIDE(_code_bootstrap_end = .);
		PROVIDE(_init_end = .);
	}

	PROVIDE(_code_end = .);
}
