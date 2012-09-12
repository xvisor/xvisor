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
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for device emulation framework
 */
#ifndef _VMM_DEVEMU_H__
#define _VMM_DEVEMU_H__

#include <vmm_spinlocks.h>
#include <vmm_devtree.h>
#include <vmm_manager.h>

struct vmm_emudev;
struct vmm_emupic;
struct vmm_emuid;
struct vmm_emulator;

enum vmm_emupic_type {
	VMM_EMUPIC_IRQCHIP = 0,
	VMM_EMUPIC_GPIO = 1,
	VMM_EMUPIC_UNKNOWN = 3
};

enum vmm_emupic_return {
	VMM_EMUPIC_IRQ_HANDLED = 0,
	VMM_EMUPIC_IRQ_UNHANDLED = 1,
	VMM_EMUPIC_GPIO_HANDLED = 2,
	VMM_EMUPIC_GPIO_UNHANDLED = 3
};

typedef int (*vmm_emupic_handle_t) (struct vmm_emupic *epic,
				    u32 irq_num,
				    int cpu,
				    int irq_level);

typedef int (*vmm_emulator_probe_t) (struct vmm_guest *guest,
				     struct vmm_emudev *edev,
				     const struct vmm_emuid *eid);

typedef int (*vmm_emulator_read_t) (struct vmm_emudev *edev,
				    physical_addr_t offset, 
				    void *dst, u32 dst_len);

typedef int (*vmm_emulator_reset_t) (struct vmm_emudev *edev);

typedef int (*vmm_emulator_write_t) (struct vmm_emudev *edev,
				     physical_addr_t offset, 
				     void *src, u32 src_len);

typedef int (*vmm_emulator_remove_t) (struct vmm_emudev *edev);

struct vmm_emudev {
	vmm_spinlock_t lock;
	struct vmm_devtree_node *node;
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
	u32 type;
	vmm_emupic_handle_t handle;
	void *priv;
};

struct vmm_emuid {
	char name[32];
	char type[32];
	char compatible[128];
	void *data;
};

struct vmm_emulator {
	struct dlist head;
	char name[32];
	const struct vmm_emuid *match_table;
	vmm_emulator_probe_t probe;
	vmm_emulator_read_t read;
	vmm_emulator_write_t write;
	vmm_emulator_reset_t reset;
	vmm_emulator_remove_t remove;
};

/** Emulate read for given VCPU */
int vmm_devemu_emulate_read(struct vmm_vcpu *vcpu, 
			    physical_addr_t gphys_addr,
			    void *dst, u32 dst_len);

/** Emulate write for given VCPU */
int vmm_devemu_emulate_write(struct vmm_vcpu *vcpu, 
			     physical_addr_t gphys_addr,
			     void *src, u32 src_len);

/** Internal function to emulate irq (should not be called directly) */
extern int __vmm_devemu_emulate_irq(struct vmm_guest *guest, u32 irq_num, 
				    int cpu, int irq_level);

/** Emulate shared irq for guest */
#define vmm_devemu_emulate_irq(guest, irq, level)	\
		__vmm_devemu_emulate_irq(guest, irq, -1, level) 

/** Emulate percpu irq for guest */
#define vmm_devemu_emulate_percpu_irq(guest, irq, cpu, level)	\
		__vmm_devemu_emulate_irq(guest, irq, cpu, level) 

/** Signal completion of host-to-guest mapped irq 
 *  (Note: For proper functioning of host-to-guest mapped irq, the PIC 
 *  emulators must call this function upon completion/end-of interrupt)
 */
int vmm_devemu_complete_h2g_irq(struct vmm_guest *guest, u32 irq_num);

/** Register emulated pic */
int vmm_devemu_register_pic(struct vmm_guest *guest, 
			    struct vmm_emupic *emu);

/** Unregister emulated pic */
int vmm_devemu_unregister_pic(struct vmm_guest *guest, 
			      struct vmm_emupic *emu);

/** Find a registered emulated pic */
struct vmm_emupic *vmm_devemu_find_pic(struct vmm_guest *guest, 
					const char *name);

/** Get a registered emulated pic */
struct vmm_emupic *vmm_devemu_pic(struct vmm_guest *guest, int index);

/** Count available emulated pic */
u32 vmm_devemu_pic_count(struct vmm_guest *guest);

/** Register emulator */
int vmm_devemu_register_emulator(struct vmm_emulator *emu);

/** Unregister emulator */
int vmm_devemu_unregister_emulator(struct vmm_emulator *emu);

/** Find a registered emulator */
struct vmm_emulator *vmm_devemu_find_emulator(const char *name);

/** Get a registered emulator */
struct vmm_emulator *vmm_devemu_emulator(int index);

/** Count available emulators */
u32 vmm_devemu_emulator_count(void);

/** Reset context for given guest */
int vmm_devemu_reset_context(struct vmm_guest *guest);

/** Reset emulators for given region */
int vmm_devemu_reset_region(struct vmm_guest *guest, struct vmm_region *reg);

/** Probe emulators for given region */
int vmm_devemu_probe_region(struct vmm_guest *guest, struct vmm_region *reg);

/** Remove emulator for given region */
int vmm_devemu_remove_region(struct vmm_guest *guest, struct vmm_region *reg);

/** Initialize context for given guest */
int vmm_devemu_init_context(struct vmm_guest *guest);

/** DeInitialize context for given guest */
int vmm_devemu_deinit_context(struct vmm_guest *guest);

/** Initialize device emulation framework */
int vmm_devemu_init(void);

#endif
