/**
 * Copyright (c) 2018 Anup Patel.
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
 * @file basic_irq.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for common interrupt handling
 */
#ifndef __BASIC_IRQ_H__
#define __BASIC_IRQ_H__

#include <arch_types.h>

typedef int (*irq_handler_t) (u32 irq_no, struct pt_regs *regs);

void basic_irq_setup(void);

void basic_irq_register(u32 irq_no, irq_handler_t hndl);

int basic_irq_exec_handler(struct pt_regs *regs);

void basic_irq_enable(void);

void basic_irq_disable(void);

void basic_irq_wfi(void);

#endif
