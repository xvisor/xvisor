/**
 * Copyright (c) 2011 Pranav Sawargaonkar.
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
 * @author Pranav Sawargaonkar (pranav.sawargaonkar@gmail.com)
 * @brief CPU specific linker script
 */

OUTPUT_FORMAT("elf32-littlearm", "elf32-littlearm", "elf32-littlearm")
OUTPUT_ARCH("arm")
ENTRY(_start_vect)

SECTIONS 
{
	. = CPU_TEXT_START;

	. = ALIGN(0x100000); /* Need this to create proper sections */
	
	PROVIDE(_code_start = .);

	.text :
 	{
		PROVIDE(_text_start = .);
		*(.entry)
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

	.svc_stack :
	{
		PROVIDE(_svc_stack_start = .);
		. = . + CONFIG_IRQ_STACK_SIZE;
		. = ALIGN(4);
		PROVIDE(_svc_stack_end = .);
        }

	.mon_stack :
	{
		PROVIDE(_mon_stack_start = .);
		. = . + CONFIG_IRQ_STACK_SIZE;
		. = ALIGN(4);
		PROVIDE(_mon_stack_end = .);
	}

	.abt_stack :
	{
		PROVIDE(_abt_stack_start = .);
		. = . + CONFIG_IRQ_STACK_SIZE;
		. = ALIGN(4);
		PROVIDE(_abt_stack_end = .);
	}

	.und_stack :
	{
		PROVIDE(_und_stack_start = .);
		. = . + CONFIG_IRQ_STACK_SIZE;
		. = ALIGN(4);
		PROVIDE(_und_stack_end = .);
	}

	.irq_stack :
	{
		PROVIDE(_irq_stack_start = .);
		. = . + CONFIG_IRQ_STACK_SIZE;
		. = ALIGN(4);
		PROVIDE(_irq_stack_end = .);
	}

	.fiq_stack :
	{
		PROVIDE(_fiq_stack_start = .);
		. = . + CONFIG_IRQ_STACK_SIZE;
		. = ALIGN(4);
		PROVIDE(_fiq_stack_end = .);
	}

	. = ALIGN(0x1000); /* Put init functions into separate pages */
	.init :
	{
		PROVIDE(_init_text_start = .);
		*(.init.text)
		. = ALIGN(4);
		PROVIDE(_init_text_end = .);
	}

	. = ALIGN(0x100000); /* Need this to create proper sections */
	
	PROVIDE(_code_end = .);
}
