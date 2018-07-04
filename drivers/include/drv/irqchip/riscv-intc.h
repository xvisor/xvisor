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
 * @file riscv-intc.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief RISC-V local interrupt controller interface
 */

#ifndef __RISCV_INTC_H__
#define __RISCV_INTC_H__

#define RISCV_IRQ_COUNT			__riscv_xlen

#define RISCV_IRQ_SUPERVISOR_SOFTWARE	1
#define RISCV_IRQ_SUPERVISOR_TIMER	5
#define RISCV_IRQ_SUPERVISOR_EXTERNAL	9
#define RISCV_IRQ_LOCAL_BASE		16

#endif
