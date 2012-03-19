/**
 * Copyright (c) 2012 Anup Patel.
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
 * @file cpu_entry_ttbl.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Initial translation table setup at reset time
 */

#include <vmm_types.h>
#include <cpu_defines.h>

/* Note: This function must be called with MMU disabled from 
 * primary CPU only.
 * Note: This function cannot refer to any global variable &
 * functions to ensure that it can be executed from anywhere.
 */
void __attribute__ ((section (".entry"))) 
_setup_initial_ttbl(virtual_addr_t load_start,
		    virtual_addr_t load_end,
		    virtual_addr_t exec_start,
		    virtual_addr_t exec_end)
{
	/* FIXME: */
}
