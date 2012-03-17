/**
 * Copyright (c) 2011 Anup Patel.
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
 * @file arch_board.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief board specific functions
 */
#ifndef _ARCH_BOARD_H__
#define _ARCH_BOARD_H__

#include <vmm_types.h>
#include <vmm_devtree.h>
#include <vmm_host_aspace.h>
#include <ca15x4_board.h>
#include <gic.h>

/** Default Terminal related function required by VMM core */
int arch_defterm_getc(u8 *ch);
int arch_defterm_putc(u8 ch);
int arch_defterm_init(void);

/** Host IRQ related function required by VMM core */
#define ARCH_HOST_IRQ_COUNT			GIC_NR_IRQS
static inline u32 arch_host_irq_active(u32 cpu_irq_no)
{
	return gic_active_irq(0);
}
static inline int arch_host_irq_init(void)
{
	virtual_addr_t dist_base, cpu_base;

	dist_base = vmm_host_iomap(A15_MPCORE_GIC_DIST, 0x1000);
	cpu_base = vmm_host_iomap(A15_MPCORE_GIC_CPU, 0x1000);

	return gic_init(0, IRQ_CA15X4_GIC_START, cpu_base, dist_base);
}

/** RAM related functions required by VMM core */
int arch_board_ram_start(physical_addr_t * addr);
int arch_board_ram_size(physical_size_t * size);

/** Device tree related function required by VMM core */
int arch_devtree_populate(struct vmm_devtree_node ** root);

/** Board specific functions */
int arch_board_reset(void);
int arch_board_shutdown(void);
int arch_board_early_init(void);
int arch_board_final_init(void);

#endif
