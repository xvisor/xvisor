/**
 * Copyright (c) 2016 Anup Patel.
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
 * @file vmm_platform.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Platform bus implementation
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_devtree.h>
#include <vmm_devdrv.h>
#include <vmm_platform.h>

static int platform_bus_match(struct vmm_device *dev, struct vmm_driver *drv)
{
	const struct vmm_devtree_nodeid *match;

	if (!dev || !dev->of_node || !drv || !drv->match_table) {
		return 0;
	}

	if (!vmm_devtree_is_available(dev->of_node)) {
		return 0;
	}

	if (dev->parent && (dev->of_node == dev->parent->of_node)) {
		return 0;
	}

	match = vmm_devtree_match_node(drv->match_table, dev->of_node);
	if (!match) {
		return 0;
	}

	return 1;
}

static int platform_bus_probe(struct vmm_device *dev)
{
	int rc;
	struct vmm_driver *drv;
	const struct vmm_devtree_nodeid *match;

	if (!dev || !dev->of_node || !dev->driver) {
		return VMM_EFAIL;
	}
	drv = dev->driver;

	if (!drv->match_table) {
		return VMM_EFAIL;
	}

	rc = vmm_devdrv_pinctrl_bind(dev);
	if (rc == VMM_EPROBE_DEFER) {
		return rc;
	}

	match = vmm_devtree_match_node(drv->match_table, dev->of_node);
	if (match) {
		return drv->probe(dev, match);
	}

	return VMM_OK;
}

static int platform_bus_remove(struct vmm_device *dev)
{
	struct vmm_driver *drv;

	if (!dev || !dev->of_node || !dev->driver) {
		return VMM_EFAIL;
	}
	drv = dev->driver;

	return drv->remove(dev);
}

static void platform_device_release(struct vmm_device *dev)
{
	vmm_devtree_dref_node(dev->of_node);
	dev->of_node = NULL;
	vmm_free(dev);
}

static int platform_probe(struct vmm_devtree_node *node,
			  struct vmm_device *parent)
{
	int rc;
	struct vmm_device *dev;
	struct vmm_devtree_node *child;

	if (!node) {
		return VMM_EFAIL;
	}

	dev = vmm_zalloc(sizeof(struct vmm_device));
	if (!dev) {
		return VMM_ENOMEM;
	}

	vmm_devdrv_initialize_device(dev);

	if (strlcpy(dev->name, node->name, sizeof(dev->name)) >=
	    sizeof(dev->name)) {
		vmm_free(dev);
		return VMM_EOVERFLOW;
	}
	vmm_devtree_ref_node(node);
	dev->of_node = node;
	dev->parent = parent;
	dev->bus = &platform_bus;
	dev->release = platform_device_release;
	dev->priv = NULL;

	rc = vmm_devdrv_register_device(dev);
	if (rc) {
		vmm_free(dev);
		return rc;
	}

	vmm_devtree_for_each_child(child, node) {
		platform_probe(child, dev);
	}

	return VMM_OK;
}

struct vmm_bus platform_bus = {
	.name = "platform",
	.match = platform_bus_match,
	.probe = platform_bus_probe,
	.remove = platform_bus_remove,
};

int vmm_platform_probe(struct vmm_devtree_node *node)
{
	return platform_probe(node, NULL);
}
