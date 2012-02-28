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
 * @file cpu_main.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief C code for cpu functions
 */

#include <arch_cpu.h>
#include <vmm_string.h>
#include <vmm_stdio.h>
#include <vmm_error.h>
#include <vmm_main.h>

void cpu_regs_dump(arch_regs_t *tregs)
{
}

int __init arch_cpu_early_init(void)
{
	/*
	 * Host virtual memory, device tree, heap is up.
	 * Do necessary early stuff like iomapping devices
	 * memory or boot time memory reservation here.
	 */
	return 0;
}

int __init arch_cpu_final_init(void)
{
        return 0;
}

void __init cpu_init(void)
{
	/* Initialize VMM (APIs only available after this) */
	vmm_init();

	/* We should never come back here. */
	while (1);
}
