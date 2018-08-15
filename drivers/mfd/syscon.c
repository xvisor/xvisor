/**
 * Copyright (c) 2018 Anup Patel.
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
 * @file syscon.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief System Control Driver
 *
 * The source has been largely adapted from Linux
 * drivers/mfd/syscon.c
 *
 * System Control Driver
 *
 * Copyright (C) 2012 Freescale Semiconductor, Inc.
 * Copyright (C) 2012 Linaro Ltd.
 *
 * Author: Dong Aisheng <dong.aisheng@linaro.org>
 *
 * The original code is licensed under the GPL.
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_stdio.h>
#include <vmm_spinlocks.h>
#include <vmm_devtree.h>
#include <vmm_devdrv.h>
#include <vmm_platform.h>
#include <vmm_modules.h>
#include <libs/list.h>
#include <libs/stringlib.h>
#include <drv/regmap.h>
#include <drv/mfd/syscon.h>

#define MODULE_DESC			"System Control Driver"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			syscon_init
#define	MODULE_EXIT			syscon_exit

#undef DEBUG

#ifdef DEBUG
#define DPRINTF(msg...)			vmm_printf(msg)
#else
#define DPRINTF(msg...)
#endif

static struct vmm_driver syscon_driver;

static DEFINE_SPINLOCK(syscon_list_slock);
static LIST_HEAD(syscon_list);

struct syscon {
	struct vmm_devtree_node *np;
	struct regmap *regmap;
	void *base;
	struct dlist list;
};

static const struct regmap_config syscon_regmap_config = {
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = 4,
};

static struct syscon *of_syscon_register(struct vmm_devtree_node *np)
{
	int ret;
	irq_flags_t flags;
	struct syscon *syscon;
	struct regmap_config syscon_config = syscon_regmap_config;
	physical_addr_t pa;
	physical_size_t sz;
	virtual_addr_t va;
	u32 reg_io_width;

	syscon = vmm_zalloc(sizeof(*syscon));
	if (!syscon)
		return VMM_ERR_PTR(VMM_ENOMEM);
	vmm_devtree_ref_node(np);
	syscon->np = np;
	INIT_LIST_HEAD(&syscon->list);

	ret = vmm_devtree_regaddr(np, &pa, 0);
	if (ret) {
		vmm_lerror(np->name, "failed to get register space address\n");
		vmm_devtree_dref_node(np);
		vmm_free(syscon);
		return VMM_ERR_PTR(ret);
	}

	ret = vmm_devtree_regsize(np, &sz, 0);
	if (ret) {
		vmm_lerror(np->name, "failed to get register space size\n");
		vmm_devtree_dref_node(np);
		vmm_free(syscon);
		return VMM_ERR_PTR(ret);
	}

	ret = vmm_devtree_request_regmap(np, &va, 0, "syscon");
	if (ret) {
		vmm_lerror(np->name, "failed to map register space\n");
		vmm_devtree_dref_node(np);
		vmm_free(syscon);
		return VMM_ERR_PTR(ret);
	}
	syscon->base = (void *)va;

	/* Parse the device's DT node for an endianness specification */
	if (vmm_devtree_getattr(np, "big-endian"))
		syscon_config.val_format_endian = REGMAP_ENDIAN_BIG;
	else if (vmm_devtree_getattr(np, "little-endian"))
		syscon_config.val_format_endian = REGMAP_ENDIAN_LITTLE;
	else if (vmm_devtree_getattr(np, "native-endian"))
		syscon_config.val_format_endian = REGMAP_ENDIAN_NATIVE;

	/*
	 * search for reg-io-width property in DT. If it is not provided,
	 * default to 4 bytes. regmap_init_mmio will return an error if values
	 * are invalid so there is no need to check them here.
	 */
	ret = vmm_devtree_read_u32(np, "reg-io-width", &reg_io_width);
	if (ret)
		reg_io_width = 4;

	syscon_config.reg_stride = reg_io_width;
	syscon_config.val_bits = reg_io_width * 8;
	syscon_config.max_register = sz - reg_io_width;

	syscon->regmap = regmap_init_mmio(NULL, syscon->base, &syscon_config);
	if (VMM_IS_ERR(syscon->regmap)) {
		vmm_lerror(np->name, "regmap init failed\n");
		vmm_devtree_regunmap_release(np, va, 0);
		vmm_devtree_dref_node(np);
		vmm_free(syscon);
		return VMM_ERR_CAST(syscon->regmap);
	}

	vmm_spin_lock_irqsave(&syscon_list_slock, flags);
	list_add_tail(&syscon->list, &syscon_list);
	vmm_spin_unlock_irqrestore(&syscon_list_slock, flags);

	vmm_linfo(np->name, "regmap @ 0x%"PRIPADDR" registered\n", pa);

	return syscon;
}

static void of_syscon_unregister(struct syscon *syscon)
{
	irq_flags_t flags;

	if (!syscon)
		return;

	vmm_spin_lock_irqsave(&syscon_list_slock, flags);
	list_del(&syscon->list);
	vmm_spin_unlock_irqrestore(&syscon_list_slock, flags);

	regmap_exit(syscon->regmap);

	vmm_devtree_regunmap_release(syscon->np,
				     (virtual_addr_t)syscon->base, 0);

	vmm_devtree_dref_node(syscon->np);

	vmm_free(syscon);
}

static struct syscon *node_to_syscon(struct vmm_devtree_node *np)
{
	irq_flags_t flags;
	struct syscon *entry, *syscon = NULL;

	vmm_spin_lock_irqsave(&syscon_list_slock, flags);

	list_for_each_entry(entry, &syscon_list, list)
		if (entry->np == np) {
			syscon = entry;
			break;
		}

	vmm_spin_unlock_irqrestore(&syscon_list_slock, flags);

	if (!syscon)
		syscon = of_syscon_register(np);

	return syscon;
}

struct regmap *syscon_node_to_regmap(struct vmm_devtree_node *np)
{
	struct syscon *syscon = node_to_syscon(np);

	if (VMM_IS_ERR(syscon))
		return VMM_ERR_CAST(syscon);

	return syscon->regmap;
}
VMM_EXPORT_SYMBOL(syscon_node_to_regmap);

struct regmap *syscon_regmap_lookup_by_compatible(const char *s)
{
	struct vmm_devtree_node *syscon_np;
	struct regmap *regmap;

	syscon_np = vmm_devtree_find_compatible(NULL, NULL, s);
	if (!syscon_np)
		return VMM_ERR_PTR(VMM_ENODEV);

	regmap = syscon_node_to_regmap(syscon_np);
	vmm_devtree_dref_node(syscon_np);

	return regmap;
}
VMM_EXPORT_SYMBOL(syscon_regmap_lookup_by_compatible);

static int syscon_match_pdevname(struct vmm_device *dev, void *data)
{
	return !strcmp(dev->name, (const char *)data) &&
		(dev->driver == &syscon_driver);
}

struct regmap *syscon_regmap_lookup_by_pdevname(const char *s)
{
	struct vmm_device *dev;
	struct syscon *syscon;

	dev = vmm_devdrv_bus_find_device(&platform_bus, NULL, (void *)s,
				 syscon_match_pdevname);
	if (!dev)
		return VMM_ERR_PTR(VMM_EPROBE_DEFER);

	syscon = vmm_devdrv_get_data(dev);

	return syscon->regmap;
}
VMM_EXPORT_SYMBOL(syscon_regmap_lookup_by_pdevname);

struct regmap *syscon_regmap_lookup_by_phandle(struct vmm_devtree_node *np,
					const char *property)
{
	struct vmm_devtree_node *syscon_np;
	struct regmap *regmap;

	if (property)
		syscon_np = vmm_devtree_parse_phandle(np, property, 0);
	else
		syscon_np = np;

	if (!syscon_np)
		return VMM_ERR_PTR(VMM_ENODEV);

	regmap = syscon_node_to_regmap(syscon_np);
	vmm_devtree_dref_node(syscon_np);

	return regmap;
}
VMM_EXPORT_SYMBOL(syscon_regmap_lookup_by_phandle);

static int syscon_probe(struct vmm_device *dev)
{
	struct syscon *syscon = node_to_syscon(dev->of_node);

	if (VMM_IS_ERR(syscon))
		return VMM_PTR_ERR(syscon);

	vmm_devdrv_set_data(dev, syscon);

	return VMM_OK;
}

static int syscon_remove(struct vmm_device *dev)
{
	struct syscon *syscon = vmm_devdrv_get_data(dev);

	of_syscon_unregister(syscon);

	return VMM_OK;
}

static const struct vmm_devtree_nodeid syscon_match[] = {
	{ .compatible = "syscon", },
	{},
};

static struct vmm_driver syscon_driver = {
	.name = "syscon",
	.match_table = syscon_match,
	.probe = syscon_probe,
	.remove = syscon_remove,
};

static int __init syscon_init(void)
{
	return vmm_devdrv_register_driver(&syscon_driver);
}

static void __exit syscon_exit(void)
{
	vmm_devdrv_unregister_driver(&syscon_driver);
}

VMM_DECLARE_MODULE(MODULE_DESC,
		   MODULE_AUTHOR,
		   MODULE_LICENSE,
		   MODULE_IPRIORITY,
		   MODULE_INIT,
		   MODULE_EXIT);
