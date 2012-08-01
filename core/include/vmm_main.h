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
 * @brief main header file for vmm
 */
#ifndef _VMM_MAIN_H__
#define _VMM_MAIN_H__

#include <vmm_types.h>

/** Hang hypervisor */
void __noreturn vmm_hang(void);

/** Initialize hypervisor */
void vmm_init(void);

#if defined(CONFIG_SMP)
/** Initialize hypervisor for secondary CPUs */
void vmm_init_secondary(void);
#endif

/** Reset hypervisor */
void vmm_reset(void);

/** Shutdown hypervisor */
void vmm_shutdown(void);

#endif
