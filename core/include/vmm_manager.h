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
 * @file vmm_manager.h
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for hypervisor manager
 */
#ifndef _VMM_MANAGER_H__
#define _VMM_MANAGER_H__

#include <vmm_types.h>
#include <vmm_regs.h>
#include <vmm_list.h>
#include <vmm_spinlocks.h>
#include <vmm_devtree.h>

enum vmm_region_flags {
	VMM_REGION_REAL=0x00000001,
	VMM_REGION_VIRTUAL=0x00000002,
	VMM_REGION_ALIAS=0x00000004,
	VMM_REGION_MEMORY=0x00000008,
	VMM_REGION_IO=0x00000010,
	VMM_REGION_CACHEABLE=0x00000020,
	VMM_REGION_READONLY=0x00000040,
	VMM_REGION_ISRAM=0x00000080,
	VMM_REGION_ISROM=0x00000100,
	VMM_REGION_ISDEVICE=0x00000200,
};

typedef struct vmm_region vmm_region_t;
typedef struct vmm_guest_aspace vmm_guest_aspace_t;
typedef struct vmm_vcpu_irqs vmm_vcpu_irqs_t;
typedef struct vmm_vcpu vmm_vcpu_t;
typedef struct vmm_guest vmm_guest_t;

struct vmm_region {
	struct dlist head;
	vmm_devtree_node_t *node;
	vmm_guest_aspace_t *aspace;
	physical_addr_t gphys_addr;
	physical_addr_t hphys_addr;
	physical_size_t phys_size;
	u32 flags;
	void *devemu_priv;
};

struct vmm_guest_aspace {
	vmm_devtree_node_t *node;
	vmm_guest_t *guest;
	struct dlist reg_list;
	void *devemu_priv;
};

#define list_for_each_region(curr, aspace)	\
			list_for_each(curr, &(aspace->reg_list))

struct vmm_vcpu_irqs {
	bool *assert;
	u32 *reason;
	u32 depth;
	u64 assert_count;
	u64 execute_count;
	u64 deassert_count;
	bool wait_for_irq;
};

struct vmm_guest {
	struct dlist head;
	vmm_spinlock_t lock;
	u32 id;
	vmm_devtree_node_t *node;
	u32 vcpu_count;
	struct dlist vcpu_list;
	vmm_guest_aspace_t aspace;
};

#define list_for_each_vcpu(curr, guest)	\
			list_for_each(curr, &(guest->vcpu_list))

enum vmm_vcpu_states {
	VMM_VCPU_STATE_UNKNOWN = 0x01,
	VMM_VCPU_STATE_RESET = 0x02,
	VMM_VCPU_STATE_READY = 0x04,
	VMM_VCPU_STATE_RUNNING = 0x08,
	VMM_VCPU_STATE_PAUSED = 0x10,
	VMM_VCPU_STATE_HALTED = 0x20
};

#define VMM_VCPU_STATE_SAVEABLE		( VMM_VCPU_STATE_RUNNING | \
					  VMM_VCPU_STATE_PAUSED | \
					  VMM_VCPU_STATE_HALTED )

#define VMM_VCPU_MIN_PRIORITY		0
#define VMM_VCPU_MAX_PRIORITY		7
#define VMM_VCPU_DEF_PRIORITY		3
#define VMM_VCPU_DEF_TIME_SLICE		1000000

struct vmm_vcpu {
	struct dlist head;
	vmm_spinlock_t lock;
	u32 id;
	u32 subid;
	char name[64];
	vmm_devtree_node_t *node;
	bool is_normal;
	vmm_guest_t *guest;
	u32 state;
	u32 reset_count;
	virtual_addr_t start_pc;
	virtual_addr_t start_sp;
	vmm_user_regs_t *uregs;
	vmm_super_regs_t *sregs;
	vmm_vcpu_irqs_t *irqs;

	u8 priority; /**< Scheduling Parameter */
	u32 preempt_count; /**< Scheduling Parameter */
	u64 time_slice; /**< Scheduling Parameter (nano seconds) */
	void * sched_priv; /**< Scheduling Context */

	struct dlist wq_head; /**< Wait Queue List head */
	void * wq_priv; /**< Wait Queue Context */

	void * devemu_priv; /**< Device Emulation Context */
};

/** Maximum number of vcpus (thread or normal) */
u32 vmm_manager_max_vcpu_count(void);

/** Current number of vcpus (thread + normal) */
u32 vmm_manager_vcpu_count(void);

/** Retrieve vcpu */
vmm_vcpu_t * vmm_manager_vcpu(u32 vcpu_id);

/** Reset a vcpu */
int vmm_manager_vcpu_reset(vmm_vcpu_t * vcpu);

/** Kick a vcpu out of reset state */
int vmm_manager_vcpu_kick(vmm_vcpu_t * vcpu);

/** Pause a vcpu */
int vmm_manager_vcpu_pause(vmm_vcpu_t * vcpu);

/** Resume a vcpu */
int vmm_manager_vcpu_resume(vmm_vcpu_t * vcpu);

/** Halt a vcpu */
int vmm_manager_vcpu_halt(vmm_vcpu_t * vcpu);

/** Dump registers of a vcpu */
int vmm_manager_vcpu_dumpreg(vmm_vcpu_t * vcpu);

/** Dump registers of a vcpu */
int vmm_manager_vcpu_dumpstat(vmm_vcpu_t * vcpu);

/** Create an orphan vcpu */
vmm_vcpu_t * vmm_manager_vcpu_orphan_create(const char *name,
					    virtual_addr_t start_pc,
					    virtual_addr_t start_sp,
					    u8 priority,
					    u64 time_slice_nsecs);

/** Destroy an orphan vcpu */
int vmm_manager_vcpu_orphan_destroy(vmm_vcpu_t * vcpu);

/** Number of guests */
u32 vmm_manager_guest_count(void);

/** Retrieve guest */
vmm_guest_t * vmm_manager_guest(u32 guest_id);

/** Number of vcpus belonging to a given guest */
u32 vmm_manager_guest_vcpu_count(vmm_guest_t *guest);

/** Retrieve vcpu belonging to a given guest with particular subid */
vmm_vcpu_t * vmm_manager_guest_vcpu(vmm_guest_t *guest, u32 subid);

/** Reset a guest */
int vmm_manager_guest_reset(vmm_guest_t * guest);

/** Kick a guest out of reset state */
int vmm_manager_guest_kick(vmm_guest_t * guest);

/** Pause a guest */
int vmm_manager_guest_pause(vmm_guest_t * guest);

/** Resume a guest */
int vmm_manager_guest_resume(vmm_guest_t * guest);

/** Halt a guest */
int vmm_manager_guest_halt(vmm_guest_t * guest);

/** Dump registers of a guest */
int vmm_manager_guest_dumpreg(vmm_guest_t * guest);

/** Create a guest based on device tree configuration */
vmm_guest_t * vmm_manager_guest_create(vmm_devtree_node_t * gnode);

/** Destroy a guest */
int vmm_manager_guest_destroy(vmm_guest_t * guest);

/** Initialize manager */
int vmm_manager_init(void);

#endif
