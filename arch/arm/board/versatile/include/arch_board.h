/**
 * Copyright (c) 2012 Jean-Christophe Dubois.
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
 * @author Jean-Chrsitophe Dubois (jcd@tribudubois.net)
 * @brief board specific functions
 */
#ifndef _ARCH_BOARD_H__
#define _ARCH_BOARD_H__

#include <vmm_types.h>
#include <vmm_devtree.h>
#include <vmm_host_aspace.h>
#include <versatile_board.h>
#include <pl190.h>

/** Default Terminal related function required by VMM core */
int arch_defterm_getc(u8 *ch);
int arch_defterm_putc(u8 ch);
int arch_defterm_init(void);

/** Interrupt controller related function required by VMM core */
#define ARCH_HOST_IRQ_COUNT		NR_IRQS_VERSATILE

static inline u32 arch_host_irq_active(u32 cpu_irq_no)
{
        return pl190_active_irq(0);
}

static inline int arch_host_irq_init(void)
{
        virtual_addr_t cpu_base;

        cpu_base = vmm_host_iomap(VERSATILE_VIC_BASE, 0x1000);

        return pl190_init(0, 0, cpu_base);
}

int arch_host_irq_init(void);

/** RAM related functions required by VMM core */
int arch_board_ram_start(physical_addr_t * addr);
int arch_board_ram_size(physical_size_t * size);

/** Device tree related function required by VMM core */
int arch_devtree_populate(struct vmm_devtree_node ** root);

/** Board specific functions */
int arch_board_getclock(struct vmm_devtree_node * node, u32 * clock);
int arch_board_reset(void);
int arch_board_shutdown(void);
int arch_board_early_init(void);
int arch_board_final_init(void);

#endif
