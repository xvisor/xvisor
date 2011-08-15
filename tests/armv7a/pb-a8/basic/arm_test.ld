/**
 * Copyright (c) 2010 Anup Patel.
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
 * @author Anup Patel (anup@brainfault.org)
 * @brief Linker script for ARM test code
 */

OUTPUT_FORMAT("elf32-littlearm", "elf32-littlearm", "elf32-littlearm")
OUTPUT_ARCH("arm")
ENTRY(_start_vect)

SECTIONS 
{
	. = TEXT_START;

	.text :
 	{
		*(.expvect)
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

	.heap :
	{
		PROVIDE(_heap_start = .);
		*(.heap)
                . = . + 8192;
		. = ALIGN(4);
		PROVIDE(_heap_end = .);
	}

 	.svc_stack :
        {
                PROVIDE(_svc_stack_start = .);
                . = . + 4096;
                . = ALIGN(4);
                PROVIDE(_svc_stack_end = .);
        }

 	.mon_stack :
        {
                PROVIDE(_mon_stack_start = .);
                . = . + 4096;
                . = ALIGN(4);
                PROVIDE(_mon_stack_end = .);
        }

 	.abt_stack :
        {
                PROVIDE(_abt_stack_start = .);
                . = . + 4096;
                . = ALIGN(4);
                PROVIDE(_abt_stack_end = .);
        }

 	.und_stack :
        {
                PROVIDE(_und_stack_start = .);
                . = . + 4096;
                . = ALIGN(4);
                PROVIDE(_und_stack_end = .);
        }

 	.irq_stack :
        {
                PROVIDE(_irq_stack_start = .);
                . = . + 4096;
                . = ALIGN(4);
                PROVIDE(_irq_stack_end = .);
        }

 	.fiq_stack :
        {
                PROVIDE(_fiq_stack_start = .);
                . = . + 4096;
                . = ALIGN(4);
                PROVIDE(_fiq_stack_end = .);
        }

 	.usr_stack :
        {
                PROVIDE(_usr_stack_start = .);
                . = . + 4096;
                . = ALIGN(4);
                PROVIDE(_usr_stack_end = .);
        }
}
