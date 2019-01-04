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
 * @file riscv_timer.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief RISC-V Timer Header
 */
#ifndef __RISCV_TIMER_H__
#define __RISCV_TIMER_H__

#include <arch_types.h>

void riscv_timer_enable(void);
void riscv_timer_disable(void);
u64 riscv_timer_irqcount(void);
u64 riscv_timer_irqdelay(void);
u64 riscv_timer_timestamp(void);
int riscv_timer_irqhndl(u32 irq_no, struct pt_regs *regs);
void riscv_timer_change_period(u32 usecs);
int riscv_timer_init(u32 usecs, u64 freq);

#endif
