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

	. = ALIGN(0x1000); /* Need this to create proper pages */

	PROVIDE(_code_start = .);

	/* Beginning of the code section */

	.text :
 	{
		. = ALIGN(4);
		PROVIDE(_text_start = .);
		*(.entry)
		*(.text)
		PROVIDE(_text_end = .);
	}

	. = ALIGN(0x1000); /* Ensure next section is page aligned */

	.init :
	{
		PROVIDE(_init_start = .);

		. = ALIGN(4);
		*(.init.text)

		PROVIDE(__setup_start = .);
		*(.setup.init);
		PROVIDE(__setup_end = .);

		. = ALIGN(CONFIG_ARM_MAX_DTB_SIZE);
		*(.devtree)
		. = ALIGN(CONFIG_ARM_MAX_DTB_SIZE);

		PROVIDE(_init_end = .);

		. = ALIGN(0x1000);

		PROVIDE(_initdata_start = .);
		*(.init.data)
		PROVIDE(_initdata_end = .);
	}

	. = ALIGN(0x1000); /* Ensure next section is page aligned */

	.cpuinit :
	{
		. = ALIGN(4);
		PROVIDE(_cpuinit_start = .);
		*(.cpuinit.*)
		PROVIDE(_cpuinit_end = .);
	}

	. = ALIGN(0x1000); /* Ensure next section is page aligned */

	.spinlock :
	{
		. = ALIGN(4);
		PROVIDE(_spinlock_start = .);
		*(.spinlock.*)
		PROVIDE(_spinlock_end = .);
	}

	/* End of the code sections */

	/* Beginning of the read-only data sections */

	. = ALIGN(0x1000); /* Ensure next section is page aligned */

	.rodata :
	{
		PROVIDE(_rodata_start = .);
		. = ALIGN(4);
		*(.symtbl)
		*(.rodata .rodata.*)
		PROVIDE(_rodata_end = .);
	}

	/* End of the read-only data sections */

	/* Beginning of the read-write data sections */

	. = ALIGN(0x1000); /* Ensure next section is page aligned */

	.percpu :
	{
		PROVIDE(_percpu_start = .);
		. = ALIGN(4);
		*(.percpu)
		PROVIDE(_percpu_end = .);
	}

	. = ALIGN(0x1000); /* Ensure next section is page aligned */

	.data :
	{
		PROVIDE(_data_start = .);
		*(.data)
		*(.readmostly.data)

		PROVIDE(_modtbl_start = .);
		. = ALIGN(4);
		*(.modtbl)
		PROVIDE(_modtbl_end = .);

		PROVIDE(_nidtbl_start = .);
		. = ALIGN(4);
		*(.nidtbl)
		PROVIDE(_nidtbl_end = .);

		PROVIDE(_data_end = .);
	}

	. = ALIGN(0x1000); /* Ensure next section is page aligned */

	.bss :
	{
		PROVIDE(_bss_start = .);
		*(.bss)
		*(COMMON)
		PROVIDE(_bss_end = .);
	}

	. = ALIGN(0x1000); /* Ensure next section is page aligned */

	.svc_stack :
	{
		PROVIDE(_svc_stack_start = .);
		. = . + (CONFIG_IRQ_STACK_SIZE * CONFIG_CPU_COUNT);
		. = ALIGN(4);
		PROVIDE(_svc_stack_end = .);
        }

	.abt_stack :
	{
		PROVIDE(_abt_stack_start = .);
		. = . + (0x100 * CONFIG_CPU_COUNT);
		. = ALIGN(4);
		PROVIDE(_abt_stack_end = .);
	}

	.und_stack :
	{
		PROVIDE(_und_stack_start = .);
		. = . + (0x100 * CONFIG_CPU_COUNT);
		. = ALIGN(4);
		PROVIDE(_und_stack_end = .);
	}

	.irq_stack :
	{
		PROVIDE(_irq_stack_start = .);
		. = . + (0x100 * CONFIG_CPU_COUNT);
		. = ALIGN(4);
		PROVIDE(_irq_stack_end = .);
	}

	.fiq_stack :
	{
		PROVIDE(_fiq_stack_start = .);
		. = . + (0x100 * CONFIG_CPU_COUNT);
		. = ALIGN(4);
		PROVIDE(_fiq_stack_end = .);
	}

	/* End of the read-write data sections */

	. = ALIGN(0x1000); /* Need this to create proper pages */

	PROVIDE(_code_end = .);
}
