/**
 * Copyright (c) 2015 Pranavkumar Sawargaonkar.
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
 * @file gpex.c
 * @author Pranavkumar Sawargaonkar (pranav.sawargaonkar@gmail.com)
 * @brief Generic PCIe Host Controller Emulator
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_devtree.h>
#include <vmm_devemu.h>
#include <vmm_timer.h>
#include <vmm_types.h>
#include <vmm_mutex.h>
#include <vmm_compiler.h>
#include <vmm_notifier.h>
#include <vmm_guest_aspace.h>
#include <vio/vmm_vserial.h>
#include <libs/stringlib.h>
#include <emu/pci/pci_emu_core.h>
#include <emu/pci/pci_ids.h>

#define GPEX_EMU_IPRIORITY		(PCI_EMU_CORE_IPRIORITY + 1)

#define MODULE_DESC			"Generic PCIe Host Emulator"
#define MODULE_AUTHOR			"Pranavkumar Sawargaonkar"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		GPEX_EMU_IPRIORITY
#define	MODULE_INIT			gpex_emulator_init
#define	MODULE_EXIT			gpex_emulator_exit

enum {
	GPEX_LOG_LVL_ERR,
	GPEX_LOG_LVL_INFO,
	GPEX_LOG_LVL_DEBUG,
	GPEX_LOG_LVL_VERBOSE
};

static int gpex_default_log_lvl = GPEX_LOG_LVL_VERBOSE;

#define GPEX_LOG(lvl, fmt, args...)					\
	do {								\
		if (GPEX_LOG_##lvl <= gpex_default_log_lvl) {	\
			vmm_printf("(%s:%d) " fmt, __func__,		\
				   __LINE__, ##args);			\
		}							\
	}while(0);

struct gpex_state {
	struct vmm_mutex lock;
	struct vmm_guest *guest;
	struct vmm_devtree_node *node;
	struct pci_host_controller *controller;
	struct vmm_notifier_block guest_aspace_client;
};

static u32 gpex_config_read(struct pci_class *pci_class, u16 reg_offset)
{
	/* TBD: Handle MSI/MSI-X */
	return 0;
}

static int gpex_config_write(struct pci_class *pci_class, u16 reg_offset,
			     u32 data)
{
	return VMM_OK;
}

static int gpex_reg_write(struct gpex_state *s, u32 addr,
			    u32 src_mask, u32 val)
{
	struct pci_device *pdev;
	u32 config_addr;
	int ret = VMM_OK;

	pdev = pci_emu_pci_dev_find_by_addr(s->controller, addr);

	if (!pdev) {
		ret = VMM_EFAIL;
		goto exit;
	}

	config_addr = addr & (PCI_CONFIG_SPACE_SIZE - 1);

	ret = pci_emu_config_space_write((struct pci_class *) pdev,
					 config_addr, val);

exit:
	return ret;
}

static int gpex_reg_read(struct gpex_state *s, u32 addr, u32 *dst, u32 size)
{
	struct pci_device *pdev;
	u32 config_addr;
	int ret = VMM_OK;

	pdev = pci_emu_pci_dev_find_by_addr(s->controller, addr);

	if (!pdev) {
		*dst = 0xFFFF;
		ret = VMM_EFAIL;
		goto exit;
	}

	config_addr = addr & (PCI_CONFIG_SPACE_SIZE - 1);
	*dst = pci_emu_config_space_read((struct pci_class *)pdev,
					 config_addr, size);

exit:
	return ret;
}

static int gpex_emulator_reset(struct vmm_emudev *edev)
{
	return VMM_OK;
}

static int gpex_emulator_read8(struct vmm_emudev *edev,
				 physical_addr_t offset,
				 u8 *dst)
{
	int rc;
	u32 regval = 0x0;

	rc = gpex_reg_read(edev->priv, offset, &regval, 1);
	if (!rc) {
		*dst = regval & 0xFF;
	}

	return rc;
}

static int gpex_emulator_read16(struct vmm_emudev *edev,
				  physical_addr_t offset,
				  u16 *dst)
{
	int rc;
	u32 regval = 0x0;

	rc = gpex_reg_read(edev->priv, offset, &regval, 2);
	if (!rc) {
		*dst = regval & 0xFFFF;
	}

	return rc;
}

static int gpex_emulator_read32(struct vmm_emudev *edev,
				  physical_addr_t offset,
				  u32 *dst)
{
	return gpex_reg_read(edev->priv, offset, dst, 4);
}

static int gpex_emulator_write8(struct vmm_emudev *edev,
				   physical_addr_t offset,
				   u8 src)
{
	return gpex_reg_write(edev->priv, offset, 0xFFFFFF00, src);
}

static int gpex_emulator_write16(struct vmm_emudev *edev,
				    physical_addr_t offset,
				    u16 src)
{
	return gpex_reg_write(edev->priv, offset, 0xFFFF0000, src);
}

static int gpex_emulator_write32(struct vmm_emudev *edev,
				   physical_addr_t offset,
				   u32 src)
{
	return gpex_reg_write(edev->priv, offset, 0x00000000, src);
}

static int gpex_guest_aspace_notification(struct vmm_notifier_block *nb,
					  unsigned long evt, void *data)
{
	int ret = NOTIFY_DONE;
	int rc;
	struct gpex_state *gpex =
		container_of(nb, struct gpex_state, guest_aspace_client);

	vmm_mutex_lock(&gpex->lock);

	switch (evt) {
	case VMM_GUEST_ASPACE_EVENT_RESET:
		if ((rc =
		     pci_emu_register_controller(gpex->node,
						 gpex->guest,
						 gpex->controller))
		    != VMM_OK) {
			GPEX_LOG(LVL_ERR,
				   "Failed to attach PCI controller.\n");
			goto _failed;
		}
		ret = NOTIFY_OK;
		break;
	default:
		break;
	}

 _failed:
	vmm_mutex_unlock(&gpex->lock);

	return ret;
}

static int gpex_emulator_probe(struct vmm_guest *guest,
			       struct vmm_emudev *edev,
			       const struct vmm_devtree_nodeid *eid)
{
	int rc = VMM_OK, i;
	char name[64];
	struct gpex_state *s;
	struct pci_class *class;

	s = vmm_zalloc(sizeof(struct gpex_state));
	if (!s) {
		GPEX_LOG(LVL_ERR, "Failed to allocate gpex state.\n");
		rc = VMM_EFAIL;
		goto _failed;
	}

	s->node = edev->node;
	s->guest = guest;
	s->controller = vmm_zalloc(sizeof(struct pci_host_controller));
	if (!s->controller) {
		GPEX_LOG(LVL_ERR, "Failed to allocate pci host contoller"
					"for gpex.\n");
		goto _failed;
	}
	INIT_MUTEX(&s->lock);
	INIT_LIST_HEAD(&s->controller->head);
	INIT_LIST_HEAD(&s->controller->attached_buses);

	/* initialize class */
	class = (struct pci_class *)s->controller;
	INIT_SPIN_LOCK(&class->lock);
	class->conf_header.vendor_id = PCI_VENDOR_ID_REDHAT;
	class->conf_header.device_id = PCI_DEVICE_ID_REDHAT_PCIE_HOST;
	class->config_read = gpex_config_read;
	class->config_write = gpex_config_write;

	rc = vmm_devtree_read_u32(edev->node, "nr_buses",
				  &s->controller->nr_buses);
	if (rc) {
		GPEX_LOG(LVL_ERR, "Failed to get fifo size in guest DTS.\n");
		goto _failed;
	}

	GPEX_LOG(LVL_VERBOSE, "%s: %d busses on this controller.\n",
		   __func__, s->controller->nr_buses);

	for (i = 0; i < s->controller->nr_buses; i++) {
		if ((rc = pci_emu_attach_new_pci_bus(s->controller, i))
		    != VMM_OK) {
			GPEX_LOG(LVL_ERR, "Failed to attach PCI bus %d\n",
				   i+1);
			goto _failed;
		}
	}

	strlcpy(name, guest->name, sizeof(name));
	strlcat(name, "/", sizeof(name));
	if (strlcat(name, edev->node->name, sizeof(name)) >= sizeof(name)) {
		rc = VMM_EOVERFLOW;
		goto _failed;
	}

	edev->priv = s;

	s->guest_aspace_client.notifier_call = &gpex_guest_aspace_notification;
	s->guest_aspace_client.priority = 0;

	vmm_guest_aspace_register_client(&s->guest_aspace_client);

	GPEX_LOG(LVL_VERBOSE, "Success.\n");

	goto _done;

_failed:
	if (s && s->controller) vmm_free(s->controller);
	if (s) vmm_free(s);

_done:
	return rc;
}

static int gpex_emulator_remove(struct vmm_emudev *edev)
{
	return VMM_OK;
}

static struct vmm_devtree_nodeid gpex_emuid_table[] = {
	{
		.type = "pci-host-controller",
		.compatible = "pci-host-cam-generic",
	},
	{
		.type = "pci-host-controller",
		.compatible = "pci-host-ecam-generic"
	},
	{ /* end of list */ },
};

static struct vmm_emulator gpex_emulator = {
	.name = "pci-host-generic",
	.match_table = gpex_emuid_table,
	.endian = VMM_DEVEMU_LITTLE_ENDIAN, // ??
	.probe = gpex_emulator_probe,
	.read8 = gpex_emulator_read8,
	.write8 = gpex_emulator_write8,
	.read16 = gpex_emulator_read16,
	.write16 = gpex_emulator_write16,
	.read32 = gpex_emulator_read32,
	.write32 = gpex_emulator_write32,
	.reset = gpex_emulator_reset,
	.remove = gpex_emulator_remove,
};

static int __init gpex_emulator_init(void)
{
	return vmm_devemu_register_emulator(&gpex_emulator);
}

static void __exit gpex_emulator_exit(void)
{
	vmm_devemu_unregister_emulator(&gpex_emulator);
}

VMM_DECLARE_MODULE(MODULE_DESC,
		   MODULE_AUTHOR,
		   MODULE_LICENSE,
		   MODULE_IPRIORITY,
		   MODULE_INIT,
		   MODULE_EXIT);
