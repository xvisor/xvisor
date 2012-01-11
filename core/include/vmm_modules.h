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
 * @file vmm_modules.h
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file module managment code
 */
#ifndef _VMM_MODULES_H__
#define _VMM_MODULES_H__

#include <vmm_types.h>

#define VMM_MODULE_SIGNATURE		0x564D4F44

typedef int (*vmm_module_init_t) (void);
typedef void (*vmm_module_exit_t) (void);

struct vmm_module {
	u32 signature;
	s8 name[44];
	s8 author[32];
	u32 ipriority;
	s32 istatus;
	vmm_module_init_t init;
	vmm_module_exit_t exit;
};

#define VMM_DECLARE_MODULE(varid,name,author,ipriority,init,exit) \
__modtbl struct vmm_module varid = \
{ VMM_MODULE_SIGNATURE, name, author, ipriority, 0, init, exit }

/** Retrive a module at given position in table */
struct vmm_module *vmm_modules_getmodule(u32 index);

/** Count number of valid modules */
u32 vmm_modules_count(void);

/** Initialize all modules based on type */
int vmm_modules_init(void);

#endif
