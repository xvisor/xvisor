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
 * @file pci_emu_core.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief source file for core PCI emulation.
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_stdio.h>
#include <vmm_spinlocks.h>
#include <vmm_host_aspace.h>
#include <vmm_guest_aspace.h>
#include <vmm_manager.h>
#include <vmm_cache.h>
#include <vmm_modules.h>
#include <vmm_devtree.h>
#include <vmm_devemu.h>
#include <vmm_spinlocks.h>
#include <vmm_mutex.h>
#include <vmm_types.h>
#include <libs/list.h>
#include <libs/stringlib.h>
#include <emu/pci/pci_emu_core.h>

#define MODULE_DESC			"PCI Bus Emulator"
#define MODULE_AUTHOR			"Himanshu Chauhan"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		PCI_EMU_CORE_IPRIORITY
#define	MODULE_INIT			pci_emulator_core_init
#define	MODULE_EXIT			pci_emulator_core_exit

struct pci_devemu_ctrl {
	struct vmm_mutex emu_lock;
        struct dlist emu_list;
};

static struct pci_devemu_ctrl pci_emu_dectrl;

static struct pci_bus *pci_find_bus_by_id(struct pci_host_controller *controller,
					  u32 bus_id)
{
	struct pci_bus *bus = NULL;
	irq_flags_t flags;

	vmm_spin_lock_irqsave(&controller->lock, flags);

	list_for_each_entry(bus, &controller->attached_buses, head) {
		if (bus->bus_id == bus_id) {
			vmm_spin_unlock_irqrestore(&controller->lock, flags);
			return bus;
		}
	}

	vmm_spin_unlock_irqrestore(&controller->lock, flags);

	return NULL;
}

static int pci_emu_attach_pci_device(struct pci_host_controller *controller,
					 struct pci_device *dev, u32 bus_id)
{
	struct pci_bus *bus = pci_find_bus_by_id(controller, bus_id);
	irq_flags_t flags;

	if (!bus) {
		return VMM_ENODEV;
	}

	vmm_spin_lock_irqsave(&bus->lock, flags);
	list_add_tail(&dev->head, &bus->attached_devices);
	vmm_spin_unlock_irqrestore(&bus->lock, flags);

	return VMM_OK;
}

#if 0
static int pci_emu_detach_pci_device(struct pci_host_controller *controller,
					 struct pci_device *dev, u32 bus_id)
{
	return VMM_OK;
}
#endif

int pci_emu_register_device(struct pci_dev_emulator *emu)
{
	struct pci_dev_emulator *e;

	if (!emu) {
		return VMM_EFAIL;
	}

	vmm_mutex_lock(&pci_emu_dectrl.emu_lock);

	e = NULL;
	list_for_each_entry(e, &pci_emu_dectrl.emu_list, head) {
		if (strcmp(e->name, emu->name) == 0) {
			vmm_mutex_unlock(&pci_emu_dectrl.emu_lock);
			return VMM_EINVALID;
		}
	}

	INIT_LIST_HEAD(&emu->head);

	list_add_tail(&emu->head, &pci_emu_dectrl.emu_list);

	vmm_mutex_unlock(&pci_emu_dectrl.emu_lock);

	return VMM_OK;
}

int pci_emu_unregister_device(struct pci_dev_emulator *emu)
{
	struct pci_dev_emulator *e;

	if (!emu) {
		return VMM_EFAIL;
	}

	vmm_mutex_lock(&pci_emu_dectrl.emu_lock);

	e = NULL;
	list_for_each_entry(e, &pci_emu_dectrl.emu_list, head) {
		if (strcmp(e->name, emu->name) == 0) {
			vmm_mutex_unlock(&pci_emu_dectrl.emu_lock);
			return VMM_ENOTAVAIL;
		}
	}

	list_del(&e->head);

	vmm_mutex_unlock(&pci_emu_dectrl.emu_lock);

	return VMM_OK;
}

int pci_emu_find_pci_device(struct pci_host_controller *controller,
			    int bus_id, int dev_id, struct pci_device **pdev)
{
	struct pci_bus *bus = pci_find_bus_by_id(controller, bus_id);
	struct pci_device *ldev;
	irq_flags_t flags;

	if (!bus)
		return VMM_ENODEV;

	vmm_spin_lock_irqsave(&bus->lock, flags);

	list_for_each_entry(ldev, &bus->attached_devices, head) {
		if (ldev->device_id == dev_id) {
			vmm_spin_unlock_irqrestore(&bus->lock, flags);
			*pdev = ldev;
			return VMM_OK;
		}
	}

	vmm_spin_unlock_irqrestore(&bus->lock, flags);

	return VMM_ENODEV;
}

struct pci_dev_emulator *pci_emu_find_device(const char *name)
{
	struct pci_dev_emulator *emu;

	if (!name) {
		return NULL;
	}

	emu = NULL;

	vmm_mutex_lock(&pci_emu_dectrl.emu_lock);

	list_for_each_entry(emu, &pci_emu_dectrl.emu_list, head) {
		if (strcmp(emu->name, name) == 0) {
			vmm_mutex_unlock(&pci_emu_dectrl.emu_lock);
			return emu;
		}
	}

	vmm_mutex_unlock(&pci_emu_dectrl.emu_lock);

	return NULL;
}

static int pci_emu_register_bar(struct vmm_guest *guest,
				const char *name,
				struct pci_class *class,
				u32 barnum,
				struct vmm_devtree_node *bar_node)
{
	int rc;
	physical_addr_t addr;

	if (vmm_devtree_read_physaddr(bar_node,
				      VMM_DEVTREE_GUEST_PHYS_ATTR_NAME, &addr)) {
		return VMM_EFAIL;
	}

	if ((rc = vmm_guest_add_region_from_node(guest, bar_node)) != VMM_OK)
		return rc;

	class->conf_header.bars[barnum] = addr;

	return VMM_OK;
}

static int pci_emu_enumerate_bars(struct vmm_guest *guest,
				  struct pci_device *pdev,
				  struct vmm_devtree_node *bus_node)
{
	struct vmm_devtree_node *bar_node = NULL, *bars = NULL;
	char reg_name[64];
	int rc;
	u32 barnum;
	struct pci_class *class = (struct pci_class *)pdev;

	bar_node = vmm_devtree_getchild(bus_node, "bars");

	if (!bar_node) {
		/* Its okay if device tree doesn't have bars */
		return VMM_OK;
	}

	list_for_each_entry(bars, &bar_node->child_list, head) {
		rc = vmm_devtree_read_u32(bars, "barnum", &barnum);
		if (rc) {
			vmm_printf("%s: Bar number not specified for %s\n",
				   __func__, bars->name);
			return rc;
		}

		if (barnum >= 6) {
			vmm_printf("%s: Bar number for %s is out of range: %d\n",
				   __func__, bars->name, barnum);
			return VMM_EFAIL;
		}

		vmm_sprintf(reg_name, "%s@%s", bars->name, bus_node->name);
		/* FIXME: Unmap previously register bars, or let it go??? */
		if ((rc = pci_emu_register_bar(guest, reg_name, class, barnum, bars)) != VMM_OK) {
			vmm_printf("%s: Failed to register bar region %s\n",
				   __func__, reg_name);
			return rc;
		}
	}

	return VMM_OK;
}

int pci_emu_probe_devices(struct vmm_guest *guest,
			  struct pci_host_controller *controller,
			  struct vmm_devtree_node *node)
{
	int rc, bcount;
	struct pci_device *pdev;
	struct pci_dev_emulator *emu;
	struct vmm_devtree_node *bus_node, *rnode, *tnode;
	u8 bus_name[32];

	if (!guest || !node || !controller) {
		return VMM_EFAIL;
	}

	vmm_mutex_lock(&pci_emu_dectrl.emu_lock);

	for (bcount = 0; bcount < controller->nr_buses; bcount++) {
		memset(bus_name, 0, sizeof(bus_name));

		vmm_sprintf((char *)bus_name, "pci_bus%d", bcount);

		bus_node = vmm_devtree_getchild(node, (const char *)bus_name);

		if (!bus_node) {
			vmm_mutex_unlock(&pci_emu_dectrl.emu_lock);
			return VMM_EFAIL;
		}

		bus_node = vmm_devtree_getchild(bus_node, "devices");

		if (!bus_node) {
			vmm_mutex_unlock(&pci_emu_dectrl.emu_lock);
			return VMM_EFAIL;
		}

		rnode = bus_node;
		list_for_each_entry(tnode, &rnode->child_list, head) {
			list_for_each_entry(emu, &pci_emu_dectrl.emu_list, head) {
				bus_node = vmm_devtree_find_matching(tnode,
								     emu->match_table);
				if (!bus_node) {
					continue;
				}
				pdev = vmm_zalloc(sizeof(struct pci_device));
				if (!pdev) {
					/* FIXME: more cleanup to do */
					vmm_mutex_unlock(&pci_emu_dectrl.emu_lock);
					return VMM_EFAIL;
				}
				INIT_SPIN_LOCK(&pdev->lock);
				pdev->node = bus_node;
				pdev->priv = NULL;
				rc = vmm_devtree_read_u32(bus_node,
							  "device_id", &pdev->device_id);
				if (rc) {
					vmm_printf("%s: error getting device ID information.\n",
						   __func__);
					vmm_free(pdev);
					vmm_mutex_unlock(&pci_emu_dectrl.emu_lock);
					return rc;
				}
				vmm_printf("Probe emulated PCI device %s/%s on PCI Bus %d\n",
					   guest->name, pdev->node->name, bcount);
				if ((rc = emu->probe(pdev, guest, NULL))) {
					vmm_printf("%s: %s/%s probe error %d\n",
						   __func__, guest->name, pdev->node->name, rc);
					vmm_free(pdev);
					vmm_mutex_unlock(&pci_emu_dectrl.emu_lock);
					return rc;
				}

				if ((rc = emu->reset(pdev))) {
					vmm_printf("%s: %s/%s reset error %d\n",
						   __func__, guest->name, pdev->node->name, rc);
					vmm_free(pdev);
					vmm_mutex_unlock(&pci_emu_dectrl.emu_lock);
					return rc;
				}

				/* Attach newly found device to its bus */
				if ((rc = pci_emu_attach_pci_device(controller, pdev,
								    bcount))) {
					vmm_printf("%s: %s/%s couldn't attach PCI device to bus.\n",
						   __func__, guest->name, pdev->node->name);
					vmm_free(pdev);
					vmm_mutex_unlock(&pci_emu_dectrl.emu_lock);
					return rc;
				}

				/* FIXME: Unregister the complete device */
				if ((rc = pci_emu_enumerate_bars(guest, pdev, bus_node)) != VMM_OK) {
					vmm_free(pdev);
					vmm_mutex_unlock(&pci_emu_dectrl.emu_lock);
					return rc;
				}
			}
		}
	}

	vmm_mutex_unlock(&pci_emu_dectrl.emu_lock);

	return VMM_OK;
}

int pci_emu_register_controller(struct vmm_devtree_node *node, struct vmm_guest *guest,
				struct pci_host_controller *controller)
{
	return pci_emu_probe_devices(guest, controller, node);
}

int pci_emu_attach_new_pci_bus(struct pci_host_controller *controller, u32 bus_id)
{
	struct pci_bus *nbus = vmm_zalloc(sizeof(struct pci_bus));
	irq_flags_t flags;

	if (nbus) {
		INIT_SPIN_LOCK(&nbus->lock);
		nbus->bus_id = bus_id;
		INIT_LIST_HEAD(&nbus->attached_devices);
		nbus->host_controller = controller;

		vmm_spin_lock_irqsave(&controller->lock, flags);
		list_add(&nbus->head, &controller->attached_buses);
		vmm_spin_unlock_irqrestore(&controller->lock, flags);

		return VMM_OK;
	}

	return VMM_ENOMEM;
}

int pci_emu_detach_pci_bus(struct pci_host_controller *controller, u32 bus_id)
{
	struct pci_bus *bus;
	irq_flags_t flags;

	vmm_spin_lock_irqsave(&controller->lock, flags);

	list_for_each_entry(bus, &controller->attached_buses, head) {
		if (bus->bus_id == bus_id) {
			list_del(&bus->head);
			vmm_free(bus);
			vmm_spin_unlock_irqrestore(&controller->lock, flags);
			return VMM_OK;
		}
	}

	vmm_spin_unlock_irqrestore(&controller->lock, flags);

	return VMM_EFAIL;
}

int pci_emu_config_space_write(struct pci_class *class, u32 reg_offs, u32 val)
{
	int retv = 0;
	irq_flags_t flags;

	vmm_spin_lock_irqsave(&class->lock, flags);

	if (reg_offs > PCI_CONFIG_HEADER_END) {
		if (class->config_write) {
			retv = class->config_write(class, reg_offs, val);
			vmm_spin_unlock_irqrestore(&class->lock, flags);
			return retv;
		} else {
			vmm_printf("%s: Access to register 0x%x but not "
				   "implemented outside class.\n",
				   __func__, reg_offs);
			vmm_spin_unlock_irqrestore(&class->lock, flags);
			return VMM_EINVALID;
		}
	}

	switch(reg_offs) {
	case PCI_CONFIG_VENDOR_ID_OFFS:
		class->conf_header.vendor_id = val;
		break;

	case PCI_CONFIG_DEVICE_ID_OFFS:
		class->conf_header.device_id = val;
		break;

	case PCI_CONFIG_COMMAND_REG_OFFS:
		class->conf_header.command = val;
		break;

	case PCI_CONFIG_STATUS_REG_OFFS:
		class->conf_header.status = val;
		break;

	case PCI_CONFIG_REVISION_ID_OFFS:
		class->conf_header.revision = val;
		break;

	case PCI_CONFIG_CLASS_CODE_OFFS:
		class->conf_header.class = val;
		break;

	case PCI_CONFIG_SUBCLASS_CODE_OFFS:
		class->conf_header.sub_class = val;
		break;

	case PCI_CONFIG_PROG_IF_OFFS:
		class->conf_header.prog_if = val;
		break;

	case PCI_CONFIG_CACHE_LINE_OFFS:
		class->conf_header.cache_line_sz = val;
		break;

	case PCI_CONFIG_LATENCY_TMR_OFFS:
		class->conf_header.latency_timer = val;
		break;

	case PCI_CONFIG_HEADER_TYPE_OFFS:
		class->conf_header.header_type = val;
		break;

	case PCI_CONFIG_BIST_OFFS:
		class->conf_header.bist = val;
		break;

	case PCI_CONFIG_BAR0_OFFS:
		class->conf_header.bars[0] = val;
		break;

	case PCI_CONFIG_BAR1_OFFS:
		class->conf_header.bars[1] = val;
		break;

	case PCI_CONFIG_BAR2_OFFS:
		class->conf_header.bars[2] = val;
		break;

	case PCI_CONFIG_BAR3_OFFS:
		class->conf_header.bars[3] = val;
		break;

	case PCI_CONFIG_BAR4_OFFS:
		class->conf_header.bars[4] = val;
		break;

	case PCI_CONFIG_BAR5_OFFS:
		class->conf_header.bars[5] = val;
		break;

	case PCI_CONFIG_CARD_BUS_PTR_OFFS:
		class->conf_header.card_bus_ptr = val;
		break;

	case PCI_CONFIG_SUBSYS_VID:
		class->conf_header.subsystem_vendor_id = val;
		break;

	case PCI_CONFIG_SUBSYS_DID:
		class->conf_header.subsystem_device_id = val;
		break;

	case PCI_CONFIG_EROM_OFFS:
		class->conf_header.expansion_rom_base = val;
		break;

	case PCI_CONFIG_CAP_PTR_OFFS:
		class->conf_header.cap_pointer = val;
		break;

	case PCI_CONFIG_INT_LINE_OFFS:
		class->conf_header.int_line = val;
		break;

	case PCI_CONFIG_INT_PIN_OFFS:
		class->conf_header.int_pin = val;
		break;

	case PCI_CONFIG_MIN_GNT_OFFS:
		class->conf_header.min_gnt = val;
		break;

	case PCI_CONFIG_MAX_LAT_OFFS:
		class->conf_header.max_lat = val;
		break;
	}

	vmm_spin_unlock_irqrestore(&class->lock, flags);

	return VMM_OK;
}

u32 pci_emu_config_space_read(struct pci_class *class, u32 reg_offs, u32 size)
{
	u32 ret = 0;
	irq_flags_t flags;

	vmm_spin_lock_irqsave(&class->lock, flags);

	if (reg_offs > PCI_CONFIG_HEADER_END) {
		if (class->config_read) {
			ret = class->config_read(class, reg_offs);
			vmm_spin_unlock_irqrestore(&class->lock, flags);
			return ret;
		} else {
			vmm_printf("%s: Access to register 0x%x but not "
				   "implemented outside class.\n",
				   __func__, reg_offs);
			vmm_spin_unlock_irqrestore(&class->lock,flags);
			return VMM_EINVALID;
		}
	}

	memcpy(&ret, (const void *)(((u8 *)(&class->conf_header)) + reg_offs), size);

	vmm_spin_unlock_irqrestore(&class->lock, flags);

	return ret;
}

static int __init pci_emulator_core_init(void)
{
	memset(&pci_emu_dectrl, 0, sizeof(pci_emu_dectrl));

	INIT_MUTEX(&pci_emu_dectrl.emu_lock);
	INIT_LIST_HEAD(&pci_emu_dectrl.emu_list);

	return VMM_OK;
}

static void __exit pci_emulator_core_exit(void)
{
}

VMM_DECLARE_MODULE(MODULE_DESC,
		   MODULE_AUTHOR,
		   MODULE_LICENSE,
		   MODULE_IPRIORITY,
		   MODULE_INIT,
		   MODULE_EXIT);
