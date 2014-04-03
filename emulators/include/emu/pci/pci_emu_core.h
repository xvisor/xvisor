/**
 * Copyright (c) 2014 Himanshu Chauhan.
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
 * @file pci_emu_core.h
 * @author Himanshu Chauhan <hschauhan@nulltrace.org>
 * @brief Interface for PCI emulation core.
 */
#ifndef __PCI_EMU_CORE_H
#define __PCI_EMU_CORE_H

#define PCI_EMU_CORE_IPRIORITY		1

#define PCI_CONFIG_HEADER_END	0x3f

#define PCI_CONFIG_VENDOR_ID_OFFS	0
#define PCI_CONFIG_DEVICE_ID_OFFS	2
#define PCI_CONFIG_COMMAND_REG_OFFS	4
#define PCI_CONFIG_STATUS_REG_OFFS	6
#define PCI_CONFIG_REVISION_ID_OFFS	8
#define PCI_CONFIG_CLASS_CODE_OFFS	9
#define PCI_CONFIG_CACHE_LINE_OFFS	12
#define PCI_CONFIG_LATENCY_TMR_OFFS	13
#define PCI_CONFIG_HEADER_TYPE_OFFS	14
#define PCI_CONFIG_BIST_OFFS		15
#define PCI_CONFIG_BAR0_OFFS		16
#define PCI_CONFIG_BAR1_OFFS		20
#define PCI_CONFIG_BAR2_OFFS		24
#define PCI_CONFIG_BAR3_OFFS		28
#define PCI_CONFIG_BAR4_OFFS		32
#define PCI_CONFIG_BAR5_OFFS		36
#define PCI_CONFIG_CARD_BUS_PTR_OFFS	40
#define PCI_CONFIG_SUBSYS_VID		44
#define PCI_CONFIG_SUBSYS_DID		46
#define PCI_CONFIG_EROM_OFFS		48
#define PCI_CONFIG_CAP_PTR_OFFS		52
#define PCI_CONFIG_INT_LINE_OFFS	60
#define PCI_CONFIG_INT_PIN_OFFS		61
#define PCI_CONFIG_MIN_GNT_OFFS		62
#define PCI_CONFIG_MAX_LAT_OFFS		63

struct pci_device;
struct pci_bus;
struct pci_host_controller;
struct pci_conf_header;
struct pci_class;

typedef u32 (*pci_config_read_t)(struct pci_class *pci_class, u16 reg_offset);
typedef int (*pci_config_write_t)(struct pci_class *pci_class, u16 reg_offset, u32 data);

struct pci_conf_header {
	u16 vendor_id;
	u16 device_id;
	u16 command;
	u16 status;
	u8 revision;
	u16 class;
	u8 cache_line_sz;
	u8 latency_timer;
	u8 header_type;
	u8 bist;
	u32 bars[6];
	u32 card_bus_ptr;
	u16 subsystem_vendor_id;
	u16 subsystem_device_id;
	u32 expansion_rom_base;
	u8 cap_pointer;
	u8 resv1;
	u16 resv2;
	u32 rev3;
	u8 int_line;
	u8 int_pin;
	u8 min_gnt;
	u8 max_lat;
} __packed;

struct pci_class {
	struct pci_conf_header conf_header;
	vmm_spinlock_t lock;
	pci_config_read_t config_read;
	pci_config_write_t config_write;
};

struct pci_host_controller {
	struct pci_class class;
	u8 name[VMM_FIELD_NAME_SIZE];
	u16 nr_buses;
	u16 bus_start;
	struct vmm_mutex lock;
	struct dlist attached_buses;
	struct dlist head;
	struct vmm_guest *guest;
};

struct pci_bus {
	struct dlist head;
	u16 bus_id;
	struct vmm_mutex lock;
	struct pci_host_controller *host_controller;
	struct dlist attached_devices;
};

struct pci_device {
	struct pci_class class;
	u32 device_id; /* ID for responding to BDF */
	struct dlist head;
	struct pci_bus *pci_bus;
	struct vmm_guest *guest;
	struct vmm_devtree_node *node;
	struct vmm_mutex lock;
	void *priv;
};

struct pci_dev_emulator {
	struct dlist head;
	char name[VMM_FIELD_NAME_SIZE];
	const struct vmm_devtree_nodeid *match_table;
	int (*probe) (struct pci_device *pdev,
		      struct vmm_guest *guest,
		      const struct vmm_devtree_nodeid *nodeid);
	int (*reset) (struct pci_device *pdev);
	int (*remove) (struct pci_device *pdev);
};

int pci_emu_register_device(struct pci_dev_emulator *emu);
int pci_emu_unregister_device(struct pci_dev_emulator *emu);
struct pci_dev_emulator *pci_emu_find_device(const char *name);
int pci_emu_probe_devices(struct vmm_guest *guest,
			  struct pci_host_controller *controller,
			  struct vmm_devtree_node *node);
int pci_emu_register_controller(struct vmm_devtree_node *node, struct vmm_guest *guest,
				struct pci_host_controller *controller);
int pci_emu_attach_new_pci_bus(struct pci_host_controller *controller, u32 bus_id);
int pci_emu_detach_pci_bus(struct pci_host_controller *controller, u32 bus_id);
int pci_emu_config_space_write(struct pci_class *class, u32 reg_offs, u32 val);
u32 pci_emu_config_space_read(struct pci_class *class, u32 reg_offs, u32 size);
int __init pci_devemu_init(void);

#endif /* __PCI_EMU_CORE_H */
