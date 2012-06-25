/**
 * Copyright (c) 2012 Sukanto Ghosh.
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
 * @file vmm_netstack.c
 * @author Sukanto Ghosh <sukantoghosh@gmail.com>
 * @brief Network Stack Interface
 */

#include <vmm_stdio.h>
#include <net/vmm_netstack.h>

struct vmm_netstack *cur_stack;

int vmm_netstack_register(struct vmm_netstack *stack)
{
	if(cur_stack != NULL) {
		vmm_panic("Network stack [%s] already registered\nCannot register again\n",
				cur_stack->name);
	}
	cur_stack = stack;
	return VMM_OK;
}

struct vmm_netstack *vmm_netstack_get(void)
{
	return cur_stack;
}
