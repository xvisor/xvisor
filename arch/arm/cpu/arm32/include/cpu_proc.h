/**
 * Copyright (c) 2013 Anup Patel.
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
 * @file cpu_proc.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Interface of processor specific quirky functions
 */
#ifndef __CPU_PROC_H__
#define __CPU_PROC_H__

#include <vmm_types.h>

/** Idle the processor (eg, wait for interrupt). */
void proc_do_idle(void);

/** MMU context switch 
 *  @param ttbr physical address of translation table
 *  @param contexidr new value to be set for CONTEX ID register
 */
void proc_mmu_switch(u32 ttbr, u32 contexidr);

/** Boot-time processor setup function
 *  @return value to be set in system control register
 */
u32 proc_setup(void);

#endif /* __CPU_PROC_H__ */
