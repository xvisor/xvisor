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
 * @file vmm_devemu.h
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for device emulation framework
 */
#ifndef _VMM_DEVEMU_H__
#define _VMM_DEVEMU_H__

#include <vmm_spinlocks.h>
#include <vmm_devtree.h>
#include <vmm_guest.h>

typedef struct vmm_emudev vmm_emudev_t;
typedef struct vmm_emupic vmm_emupic_t;
typedef struct vmm_emuclk vmm_emuclk_t;
typedef struct vmm_emuid vmm_emuid_t;
typedef struct vmm_emuguest vmm_emuguest_t;
typedef struct vmm_emulator vmm_emulator_t;
typedef struct vmm_devemu_ctrl vmm_devemu_ctrl_t;

typedef void (*vmm_emupic_hndl_t) (vmm_emupic_t *epic,
				   u32 irq_num,
				   int irq_level);

typedef void (*vmm_emuclk_tick_t) (vmm_emuclk_t *eclk);

typedef int (*vmm_emulator_probe_t) (vmm_guest_t *guest,
				     vmm_emudev_t *edev,
				     const vmm_emuid_t *eid);

typedef int (*vmm_emulator_read_t) (vmm_emudev_t *edev,
				    physical_addr_t offset, 
				    void *dst, u32 dst_len);

typedef int (*vmm_emulator_reset_t) (vmm_emudev_t *edev);

typedef int (*vmm_emulator_write_t) (vmm_emudev_t *edev,
				     physical_addr_t offset, 
				     void *src, u32 src_len);

typedef int (*vmm_emulator_remove_t) (vmm_emudev_t *edev);

struct vmm_emudev {
	vmm_spinlock_t lock;
	vmm_devtree_node_t *node;
	vmm_emulator_probe_t probe;
	vmm_emulator_read_t read;
	vmm_emulator_write_t write;
	vmm_emulator_reset_t reset;
	vmm_emulator_remove_t remove;
	void *priv;
};

struct vmm_emupic {
	struct dlist head;
	char name[32];
	vmm_emupic_hndl_t hndl;
	void *priv;
};

struct vmm_emuclk {
	struct dlist head;
	char name[32];
	vmm_emuclk_tick_t tick;
	void *priv;
};

struct vmm_emuid {
	char name[32];
	char type[32];
	char compatible[128];
	void *data;
};

struct vmm_emuguest {
	struct dlist emupic_list;
	struct dlist emuclk_list;
};

struct vmm_emulator {
	struct dlist head;
	char name[32];
	const vmm_emuid_t *match_table;
	vmm_emulator_probe_t probe;
	vmm_emulator_read_t read;
	vmm_emulator_write_t write;
	vmm_emulator_reset_t reset;
	vmm_emulator_remove_t remove;
};

struct vmm_devemu_ctrl {
	struct dlist emu_list;
};

/** Emulate read for guest */
int vmm_devemu_emulate_read(vmm_guest_t *guest, 
			    physical_addr_t gphys_addr,
			    void *dst, u32 dst_len);

/** Emulate write for guest */
int vmm_devemu_emulate_write(vmm_guest_t *guest, 
			     physical_addr_t gphys_addr,
			     void *src, u32 src_len);

/** Emulate irq for guest */
int vmm_devemu_emulate_irq(vmm_guest_t *guest, u32 irq_num, int irq_level);

/** Register emulated pic */
int vmm_devemu_register_pic(vmm_guest_t *guest, vmm_emupic_t * emu);

/** Unregister emulated pic */
int vmm_devemu_unregister_pic(vmm_guest_t *guest, vmm_emupic_t * emu);

/** Find a registered emulated pic */
vmm_emupic_t *vmm_devemu_find_pic(vmm_guest_t *guest, const char *name);

/** Get a registered emulated pic */
vmm_emupic_t *vmm_devemu_pic(vmm_guest_t *guest, int index);

/** Count available emulated pic */
u32 vmm_devemu_pic_count(vmm_guest_t *guest);

/** Get period emulated clk in microseconds */
u32 vmm_devemu_clk_microsecs(void);

/** Register emulated clock */
int vmm_devemu_register_clk(vmm_guest_t *guest, vmm_emuclk_t * clk);

/** Unregister emulated clock */
int vmm_devemu_unregister_clk(vmm_guest_t *guest, vmm_emuclk_t * clk);

/** Find a registered emulated clock */
vmm_emuclk_t *vmm_devemu_find_clk(vmm_guest_t *guest, const char *name);

/** Get a registered emulated clock */
vmm_emuclk_t *vmm_devemu_clk(vmm_guest_t *guest, int index);

/** Count available emulated clock */
u32 vmm_devemu_clk_count(vmm_guest_t *guest);

/** Register emulator */
int vmm_devemu_register_emulator(vmm_emulator_t * emu);

/** Unregister emulator */
int vmm_devemu_unregister_emulator(vmm_emulator_t * emu);

/** Find a registered emulator */
vmm_emulator_t *vmm_devemu_find_emulator(const char *name);

/** Get a registered emulator */
vmm_emulator_t *vmm_devemu_emulator(int index);

/** Count available emulators */
u32 vmm_devemu_emulator_count(void);

/** Reset emulators for given region */
int vmm_devemu_reset(vmm_guest_t *guest, vmm_guest_region_t *reg);

/** Probe emulators for given region */
int vmm_devemu_probe(vmm_guest_t *guest, vmm_guest_region_t *reg);

/** Initialize device emulation framework */
int vmm_devemu_init(void);

#endif
