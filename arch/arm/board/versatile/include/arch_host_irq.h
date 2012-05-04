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
 * @file arch_host_irq.h
 * @author Jean-Chrsitophe Dubois (jcd@tribudubois.net)
 * @brief board specific host irq functions
 */
#ifndef _ARCH_HOST_IRQ_H__
#define _ARCH_HOST_IRQ_H__

#include <vmm_types.h>
#include <vmm_host_aspace.h>
#include <pl190.h>

#define ARCH_HOST_IRQ_COUNT		NR_IRQS_VERSATILE

/* Get current active host irq */
static inline u32 arch_host_irq_active(u32 cpu_irq_no)
{
        return pl190_active_irq(0);
}

/* Initialize board specifig host irq hardware (i.e PIC) */
static inline int arch_host_irq_init(void)
{
        virtual_addr_t cpu_base;

        cpu_base = vmm_host_iomap(VERSATILE_VIC_BASE, 0x1000);

        return pl190_init(0, 0, cpu_base);
}

#endif
