/**
 * Copyright (c) 2010 Himanshu Chauhan.
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
 * @file cpu_interrupts.h
 * @version 1.0
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief header file for cpu interrupts
 */
#ifndef _CPU_INTERRUPTS_H__
#define _CPU_INTERRUPTS_H__

#include <vmm_types.h>
#include <arch_regs.h>

#define NR_SYS_INT				8
#define SYS_TIMER_INT_STATUS_MASK		(0x01UL << 30)
#define SYS_INT_ST_BIT				8
#define SYS_INT_MASK_DEF(int_num)		\
        SYS_INT ## int_num ## _MASK = (0x01UL << (SYS_INT_ST_BIT + int_num))

enum sys_ints {
        SYS_INT_MASK_DEF(0),
        SYS_INT_MASK_DEF(1),
        SYS_INT_MASK_DEF(2),
        SYS_INT_MASK_DEF(3),
        SYS_INT_MASK_DEF(4),
        SYS_INT_MASK_DEF(5),
        SYS_INT_MASK_DEF(6),
        SYS_INT_MASK_DEF(7)
};

/** Useful macros */
#define enable_interrupts()			\
	__asm__ __volatile__("ei $0\n\t");

#define disable_interrupts()			\
	__asm__ __volatile__("di $0\n\t");

void setup_interrupts();
s32 handle_internal_timer_interrupt(arch_regs_t *uregs);

#endif
