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
 * @file vmm_guest.h
 * @version 0.01
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for guest representation data structures
 */

#ifndef __VMM_GUEST_H__
#define __VMM_GUEST_H__

#include <vmm_types.h>
#include <vmm_list.h>
#include <vmm_regs.h>
#include <vmm_devtree.h>

typedef struct vmm_guest_region vmm_guest_region_t;
typedef struct vmm_guest_aspace vmm_guest_aspace_t;
typedef struct vmm_vcpu_irqs vmm_vcpu_irqs_t;
typedef void (*vmm_vcpu_tick_t) (vmm_user_regs_t * regs, u32 ticks_left);
typedef struct vmm_vcpu vmm_vcpu_t;
typedef struct vmm_guest vmm_guest_t;

struct vmm_guest_region {
	struct dlist head;
	vmm_devtree_node_t *node;
	vmm_guest_aspace_t *aspace;
	physical_addr_t gphys_addr;
	physical_addr_t hphys_addr;
	physical_size_t phys_size;
	bool is_memory;
	bool is_virtual;
	void *priv;
};

struct vmm_guest_aspace {
	vmm_devtree_node_t *node;
	vmm_guest_t *guest;
	struct dlist reg_list;
	void *priv;
};

#define list_for_each_region(curr, aspace)	\
			list_for_each(curr, &(aspace->reg_list))

struct vmm_vcpu_irqs {
	u32 *reason;
	s32 *pending;
	s32 pending_first;
	s32 *active;
	s32 active_first;
};

struct vmm_guest {
	struct dlist head;
	u32 num;
	vmm_devtree_node_t *node;
	struct dlist vcpu_list;
	vmm_guest_aspace_t aspace;
};

#define list_for_each_vcpu(curr, guest)	\
			list_for_each(curr, &(guest->vcpu_list))

enum vmm_vcpu_states {
	VMM_VCPU_STATE_UNKNOWN = 0x00,
	VMM_VCPU_STATE_RESET = 0x01,
	VMM_VCPU_STATE_READY = 0x02,
	VMM_VCPU_STATE_RUNNING = 0x04,
	VMM_VCPU_STATE_PAUSED = 0x08,
	VMM_VCPU_STATE_HALTED = 0x10
};

struct vmm_vcpu {
	struct dlist head;
	u32 num;
	char name[64];
	vmm_devtree_node_t *node;
	vmm_guest_t *guest;
	u32 state;
	u32 tick_count;
	vmm_vcpu_tick_t tick_func;
	virtual_addr_t start_pc;
	physical_addr_t bootpg_addr;
	physical_size_t bootpg_size;
	vmm_user_regs_t uregs;
	vmm_super_regs_t sregs;
	vmm_vcpu_irqs_t irqs;
};

#endif /* __VMM_GUEST_H__ */
