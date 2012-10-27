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
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for hypervisor manager
 */
#ifndef _VMM_MANAGER_H__
#define _VMM_MANAGER_H__

#include <arch_regs.h>
#include <vmm_types.h>
#include <vmm_spinlocks.h>
#include <vmm_devtree.h>
#include <libs/list.h>

enum vmm_region_flags {
	VMM_REGION_REAL=0x00000001,
	VMM_REGION_VIRTUAL=0x00000002,
	VMM_REGION_ALIAS=0x00000004,
	VMM_REGION_MEMORY=0x00000008,
	VMM_REGION_IO=0x00000010,
	VMM_REGION_CACHEABLE=0x00000020,
	VMM_REGION_BUFFERABLE=0x00000040,
	VMM_REGION_READONLY=0x00000080,
	VMM_REGION_ISRAM=0x00000100,
	VMM_REGION_ISROM=0x00000200,
	VMM_REGION_ISDEVICE=0x00000400,
};

struct vmm_region;
struct vmm_guest_aspace;
struct vmm_vcpu_irqs;
struct vmm_vcpu;
struct vmm_guest;

struct vmm_region {
	struct dlist head;
	struct vmm_devtree_node *node;
	struct vmm_guest_aspace *aspace;
	physical_addr_t gphys_addr;
	physical_addr_t hphys_addr;
	physical_size_t phys_size;
	u32 flags;
	void *devemu_priv;
};

struct vmm_guest_aspace {
	struct vmm_devtree_node *node;
	struct vmm_guest *guest;
	struct dlist reg_list;
	void *devemu_priv;
};

#define list_for_each_region(curr, aspace)	\
			list_for_each(curr, &(aspace->reg_list))

struct vmm_vcpu_irqs {
	vmm_spinlock_t lock;
	u32 irq_count;
	bool *assert;
	bool *execute;
	u32 *reason;
	int assert_pending;
	u64 assert_count;
	u64 execute_count;
	u64 deassert_count;
	bool wfi_state;
	u64 wfi_tstamp;
	void *wfi_priv;
};

struct vmm_guest {
	struct dlist head;
	vmm_spinlock_t lock;
	u32 id;
	struct vmm_devtree_node *node;
	u32 reset_count;
	u32 vcpu_count;
	struct dlist vcpu_list;
	struct vmm_guest_aspace aspace;
	void *arch_priv;
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
#define VMM_VCPU_DEF_TIME_SLICE		(CONFIG_TSLICE_MS * 1000000)

struct vmm_vcpu {
	struct dlist head;
	vmm_spinlock_t lock;
	u32 id;
	u32 subid;
	char name[64];
	struct vmm_devtree_node *node;
	bool is_normal;
	struct vmm_guest *guest;
	u32 state;
	u32 reset_count;
	virtual_addr_t start_pc;
	virtual_addr_t start_sp;

	arch_regs_t regs;
	void *arch_priv;

	struct vmm_vcpu_irqs irqs;

	u8 priority; /**< Scheduling Parameter */
	u32 preempt_count; /**< Scheduling Parameter */
	u64 time_slice; /**< Scheduling Parameter (nano seconds) */
	void *sched_priv; /**< Scheduling Context */

	struct dlist wq_head; /**< Wait Queue List head */
	void *wq_priv; /**< Wait Queue Context */

	void *devemu_priv; /**< Device Emulation Context */
};

/** Maximum number of vcpus */
u32 vmm_manager_max_vcpu_count(void);

/** Current number of vcpus (orphan + normal) */
u32 vmm_manager_vcpu_count(void);

/** Retrieve vcpu with given ID. 
 *  Returns NULL if there is no vcpu associated with given ID.
 */
struct vmm_vcpu * vmm_manager_vcpu(u32 vcpu_id);

/** Reset a vcpu */
int vmm_manager_vcpu_reset(struct vmm_vcpu * vcpu);

/** Kick a vcpu out of reset state */
int vmm_manager_vcpu_kick(struct vmm_vcpu * vcpu);

/** Pause a vcpu */
int vmm_manager_vcpu_pause(struct vmm_vcpu * vcpu);

/** Resume a vcpu */
int vmm_manager_vcpu_resume(struct vmm_vcpu * vcpu);

/** Halt a vcpu */
int vmm_manager_vcpu_halt(struct vmm_vcpu * vcpu);

/** Dump registers of a vcpu */
int vmm_manager_vcpu_dumpreg(struct vmm_vcpu * vcpu);

/** Dump registers of a vcpu */
int vmm_manager_vcpu_dumpstat(struct vmm_vcpu * vcpu);

/** Create an orphan vcpu */
struct vmm_vcpu * vmm_manager_vcpu_orphan_create(const char *name,
					    virtual_addr_t start_pc,
					    virtual_addr_t start_sp,
					    u8 priority,
					    u64 time_slice_nsecs);

/** Destroy an orphan vcpu */
int vmm_manager_vcpu_orphan_destroy(struct vmm_vcpu * vcpu);

/** Maximum number of guests */
u32 vmm_manager_max_guest_count(void);

/** Current number of guests */
u32 vmm_manager_guest_count(void);

/** Retrieve guest with given ID. 
 *  Returns NULL if there is no guest associated with given ID.
 */
struct vmm_guest * vmm_manager_guest(u32 guest_id);

/** Number of vcpus belonging to a given guest */
u32 vmm_manager_guest_vcpu_count(struct vmm_guest *guest);

/** Retrieve vcpu belonging to a given guest with particular subid */
struct vmm_vcpu * vmm_manager_guest_vcpu(struct vmm_guest *guest, u32 subid);

/** Reset a guest */
int vmm_manager_guest_reset(struct vmm_guest * guest);

/** Kick a guest out of reset state */
int vmm_manager_guest_kick(struct vmm_guest * guest);

/** Pause a guest */
int vmm_manager_guest_pause(struct vmm_guest * guest);

/** Resume a guest */
int vmm_manager_guest_resume(struct vmm_guest * guest);

/** Halt a guest */
int vmm_manager_guest_halt(struct vmm_guest * guest);

/** Dump registers of a guest */
int vmm_manager_guest_dumpreg(struct vmm_guest * guest);

/** Create a guest based on device tree configuration */
struct vmm_guest * vmm_manager_guest_create(struct vmm_devtree_node * gnode);

/** Destroy a guest */
int vmm_manager_guest_destroy(struct vmm_guest * guest);

/** Initialize manager */
int vmm_manager_init(void);

#endif
