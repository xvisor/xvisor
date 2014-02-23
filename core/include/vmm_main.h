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
 * @file vmm_main.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief main header file to start, stop and reset hypervisor
 */
#ifndef _VMM_MAIN_H__
#define _VMM_MAIN_H__

#include <vmm_types.h>
#include <vmm_compiler.h>

/** Hang hypervisor */
void __noreturn vmm_hang(void);

/** Initialize hypervisor */
void vmm_init(void);

/** Register system reset callback function */
void vmm_register_system_reset(int (*callback)());

/** Do system reset */
void vmm_reset(void);

/** Register system shutdown callback function */
void vmm_register_system_shutdown(int (*callback)());

/** Do system shutdown */
void vmm_shutdown(void);

#endif
