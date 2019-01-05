/**
 * Copyright (c) 2019 Anup Patel.
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
 * @file riscv_intc.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief RISC-V local interrupt controller driver
 */
#ifndef __RISCV_INTC_H__
#define __RISCV_INTC_H__

#include <arch_types.h>

u32 riscv_intc_nr_irqs(void);
u32 riscv_intc_active_irq(void);
int riscv_intc_ack_irq(u32 irq);
int riscv_intc_eoi_irq(u32 irq);
int riscv_intc_mask(u32 irq);
int riscv_intc_unmask(u32 irq);
int riscv_intc_init(void);

#endif
