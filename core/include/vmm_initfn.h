/**
 * Copyright (c) 2017 Anup Patel.
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
 * @file vmm_initfn.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file device-tree based init functions
 */
#ifndef _VMM_INITFN_H__
#define _VMM_INITFN_H__

#include <vmm_types.h>
#include <vmm_compiler.h>
#include <vmm_devtree.h>

/* nodeid table based initfn callback */
typedef int (*vmm_initfn_t)(struct vmm_devtree_node *);

/* Declare final init function */
#define VMM_INITFN_DECLARE_FINAL(name, compat, fn)	\
VMM_DEVTREE_NIDTBL_ENTRY(name, "initfn_final", "", "", compat, fn)

/* Declare early init function */
#define VMM_INITFN_DECLARE_EARLY(name, compat, fn)	\
VMM_DEVTREE_NIDTBL_ENTRY(name, "initfn_early", "", "", compat, fn)

/** Call early init functions */
int vmm_initfn_early(void);

/** Call final init functions */
int vmm_initfn_final(void);

#endif
