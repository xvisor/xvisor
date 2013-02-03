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
 * @file brd_pic.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief board specific progammable interrupt contoller
 */

#include <vmm_error.h>
#include <vmm_compiler.h>
#include <arch_host_irq.h>

#if CONFIG_LOCAL_APIC
#include <cpu_apic.h>
#endif

u32 arch_host_irq_active(u32 cpu_irq_no)
{
	return cpu_irq_no;
}

int __cpuinit arch_host_irq_init(void)
{
#if CONFIG_LOCAL_APIC
	apic_init();
#endif

	return VMM_OK;
}

