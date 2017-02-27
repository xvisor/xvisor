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

#include <vmm_limits.h>
#include <vmm_spinlocks.h>
#include <vmm_devtree.h>
#include <vmm_manager.h>

struct vmm_emudev;
struct vmm_emulator;

enum vmm_devemu_endianness {
	VMM_DEVEMU_UNKNOWN_ENDIAN=0,
	VMM_DEVEMU_NATIVE_ENDIAN=1,
	VMM_DEVEMU_LITTLE_ENDIAN=2,
	VMM_DEVEMU_BIG_ENDIAN=3,
	VMM_DEVEMU_MAX_ENDIAN=4,
};

struct vmm_emulator {
	struct dlist head;
	char name[VMM_FIELD_NAME_SIZE];
	const struct vmm_devtree_nodeid *match_table;
	enum vmm_devemu_endianness endian;
	int (*probe) (struct vmm_guest *guest,
		      struct vmm_emudev *edev,
		      const struct vmm_devtree_nodeid *nodeid);
	int (*remove) (struct vmm_emudev *edev);
	int (*reset) (struct vmm_emudev *edev);
	int (*sync) (struct vmm_emudev *edev,
		     unsigned long val, void *v);
	int (*read8) (struct vmm_emudev *edev,
		      physical_addr_t offset,
		      u8 *dst);
	int (*write8) (struct vmm_emudev *edev,
		       physical_addr_t offset,
		       u8 src);
	int (*read16) (struct vmm_emudev *edev,
		       physical_addr_t offset,
		       u16 *dst);
	int (*write16) (struct vmm_emudev *edev,
		        physical_addr_t offset,
		        u16 src);
	int (*read32) (struct vmm_emudev *edev,
		       physical_addr_t offset,
		       u32 *dst);
	int (*write32) (struct vmm_emudev *edev,
		        physical_addr_t offset,
		        u32 src);
	int (*read64) (struct vmm_emudev *edev,
		       physical_addr_t offset,
		       u64 *dst);
	int (*write64) (struct vmm_emudev *edev,
		        physical_addr_t offset,
		        u64 src);
	int (*read_simple) (struct vmm_emudev *edev,
			    physical_addr_t offset,
			    u32 *dst,
			    u32 size);
	int (*write_simple) (struct vmm_emudev *edev,
			     physical_addr_t offset,
			     u32 regmask,
			     u32 regval,
			     u32 size);
};

int vmm_devemu_simple_read8(struct vmm_emudev *edev,
			    physical_addr_t offset,
			    u8 *dst);
int vmm_devemu_simple_read16(struct vmm_emudev *edev,
			     physical_addr_t offset,
			     u16 *dst);
int vmm_devemu_simple_read32(struct vmm_emudev *edev,
			     physical_addr_t offset,
			     u32 *dst);
int vmm_devemu_simple_write8(struct vmm_emudev *edev,
			     physical_addr_t offset,
			     u8 src);
int vmm_devemu_simple_write16(struct vmm_emudev *edev,
			      physical_addr_t offset,
			      u16 src);
int vmm_devemu_simple_write32(struct vmm_emudev *edev,
			      physical_addr_t offset,
			      u32 src);

#define VMM_DECLARE_EMULATOR_SIMPLE(EMU, NAME, MATCH, ENDIAN, PROBE,	\
				    REMOVE, RESET, SYNC, READ, WRITE)	\
	static struct vmm_emulator EMU = {				\
		.name = NAME,						\
		.match_table = MATCH,					\
		.endian = ENDIAN,					\
		.probe = PROBE,						\
		.reset = RESET,						\
		.sync = SYNC,						\
		.remove = REMOVE,					\
		.read8 = vmm_devemu_simple_read8,			\
		.write8 = vmm_devemu_simple_write8,			\
		.read16 = vmm_devemu_simple_read16,			\
		.write16 = vmm_devemu_simple_write16,			\
		.read32 = vmm_devemu_simple_read32,			\
		.write32 = vmm_devemu_simple_write32,			\
		.read64 = NULL,						\
		.write64 = NULL,					\
		.read_simple = READ,					\
		.write_simple = WRITE,					\
	}

struct vmm_emudev {
	vmm_spinlock_t lock;
	struct vmm_devtree_node *node;
	struct vmm_region *reg;
	struct vmm_emulator *emu;
	struct vmm_emudev *parent;
	struct dlist head;
	vmm_rwlock_t child_list_lock;
	struct dlist child_list;
	void *priv;
#ifdef CONFIG_DEVEMU_DEBUG
	u32 debug_info;
#endif
};

struct vmm_devemu_irqchip {
	const char *name;
	void (*handle) (u32 irq, int cpu, int level, void *opaque);
	void (*map_host2guest) (u32 irq, u32 host_irq, void *opaque);
	void (*unmap_host2guest) (u32 irq, void *opaque);
};

/** Emulate memory read to virtual device for given VCPU */
int vmm_devemu_emulate_read(struct vmm_vcpu *vcpu,
			    physical_addr_t gphys_addr,
			    void *dst, u32 dst_len,
			    enum vmm_devemu_endianness dst_endian);

/** Emulate memory write to virtual device for given VCPU */
int vmm_devemu_emulate_write(struct vmm_vcpu *vcpu,
			     physical_addr_t gphys_addr,
			     void *src, u32 src_len,
			     enum vmm_devemu_endianness src_endian);

/** Emulate IO read to virtual device for given VCPU */
int vmm_devemu_emulate_ioread(struct vmm_vcpu *vcpu,
			      physical_addr_t gphys_addr,
			      void *dst, u32 dst_len,
			      enum vmm_devemu_endianness dst_endian);

/** Emulate IO write to virtual device for given VCPU */
int vmm_devemu_emulate_iowrite(struct vmm_vcpu *vcpu,
			       physical_addr_t gphys_addr,
			       void *src, u32 src_len,
			       enum vmm_devemu_endianness src_endian);

/** Internal function to emulate irq (should not be called directly) */
extern int __vmm_devemu_emulate_irq(struct vmm_guest *guest,
				    u32 irq, int cpu, int level);

/** Emulate shared irq for guest
 *  Note: This will only work after guest is created.
 */
#define vmm_devemu_emulate_irq(guest, irq, level)	\
		__vmm_devemu_emulate_irq(guest, irq, -1, level)

/** Emulate percpu irq for guest
 *  Note: This will only work after guest is created.
 */
#define vmm_devemu_emulate_percpu_irq(guest, irq, cpu, level)	\
		__vmm_devemu_emulate_irq(guest, irq, cpu, level)

/** Map host irq to guest irq for guest
 *  Note: This will only work after guest is created.
 */
int vmm_devemu_map_host2guest_irq(struct vmm_guest *guest, u32 irq,
				  u32 host_irq);

/** Unmap host irq to guest irq mapping for guest
 *  Note: This will only work after guest is created.
 */
int vmm_devemu_unmap_host2guest_irq(struct vmm_guest *guest, u32 irq);

/** Register guest irqchip */
int vmm_devemu_register_irqchip(struct vmm_guest *guest, u32 irq,
				struct vmm_devemu_irqchip *chip,
				void *opaque);

/** Unregister guest irqchip */
int vmm_devemu_unregister_irqchip(struct vmm_guest *guest, u32 irq,
				  struct vmm_devemu_irqchip *chip,
				  void *opaque);

/** Count available irqs of a guest */
u32 vmm_devemu_count_irqs(struct vmm_guest *guest);

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

/** Sync children of given emulated device */
int vmm_devemu_sync_children(struct vmm_guest *guest,
			     struct vmm_emudev *edev,
			     unsigned long val, void *v);

/** Sync parent of given emulated device */
int vmm_devemu_sync_parent(struct vmm_guest *guest,
			   struct vmm_emudev *edev,
			   unsigned long val, void *v);

/** Reset context for given guest */
int vmm_devemu_reset_context(struct vmm_guest *guest);

/** Reset emulators for given region */
int vmm_devemu_reset_region(struct vmm_guest *guest,
			    struct vmm_region *reg);

/** Remove emulator for given region */
int vmm_devemu_remove_region(struct vmm_guest *guest,
			     struct vmm_region *reg);

/** Probe emulators for given region */
int vmm_devemu_probe_region(struct vmm_guest *guest,
			    struct vmm_region *reg);

/** Initialize context for given guest */
int vmm_devemu_init_context(struct vmm_guest *guest);

/** DeInitialize context for given guest */
int vmm_devemu_deinit_context(struct vmm_guest *guest);

/** Initialize device emulation framework */
int vmm_devemu_init(void);

#endif
