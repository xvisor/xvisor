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
 * @file vmm_netstack.h
 * @author Sukanto Ghosh <sukantoghosh@gmail.com>
 * @brief Network Stack Interface APIs
 */

#include <vmm_types.h>
#include <vmm_error.h>

struct vmm_netstack {
	char *name;
	int (*set_ipaddr)(u8 *ipaddr);
	int (*get_ipaddr)(u8 *ipaddr);
	int (*set_ipmask)(u8 *ipmask);
	int (*get_ipmask)(u8 *ipmask);
	int (*get_hwaddr)(u8 *hwaddr);
};

int vmm_netstack_register(struct vmm_netstack *stack);

struct vmm_netstack *vmm_netstack_get(void);

inline static char *vmm_netstack_get_name(void) 
{
	struct vmm_netstack *stack = vmm_netstack_get();
	if(stack)
		return stack->name;
	else
		return NULL;
}

#define VMM_NETSTACK_OP_DEFINE(op)				\
inline static int vmm_netstack_##op(u8 *param)			\
{								\
	struct vmm_netstack *stack = vmm_netstack_get();	\
	if(stack) {						\
		return stack->op(param);			\
	}							\
	return VMM_EFAIL;					\
}

VMM_NETSTACK_OP_DEFINE(set_ipaddr);
VMM_NETSTACK_OP_DEFINE(get_ipaddr);
VMM_NETSTACK_OP_DEFINE(set_ipmask);
VMM_NETSTACK_OP_DEFINE(get_ipmask);
VMM_NETSTACK_OP_DEFINE(get_hwaddr);

