/**
 * Copyright (c) 2016 Pramod Kanni.
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
 * @file sdio_bus.c
 * @author Pramod Kanni (kanni.pramod@gmail.com)
 * @brief SDIO Functions Driver Model
 *
 * The source has been largely adapted from linux:
 * drivers/mmc/core/sdio_bus.c
 *
 * Copyright 2007 Pierre Ossman
 *
 * This linux code was extracted from:
 * git://github.com/raspberrypi/linux.git master
 *
 * The original code is licensed under the GPL.
 */
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_delay.h>
#include <vmm_timer.h>
#include <vmm_host_io.h>
#include <vmm_modules.h>
#include <libs/stringlib.h>
#include <libs/mathlib.h>

#include <drv/mmc/mmc_core.h>
#include <drv/mmc/sdio.h>
#include <drv/mmc/sdio_func.h>
#include <drv/mmc/sdio_ids.h>

#include "core.h"
#include "sdio_io.h"
#include "sdio_bus.h"

#define to_sdio_driver(d)	container_of(d, struct sdio_driver, drv)

static const struct sdio_device_id *sdio_match_one(struct sdio_func *func,
	const struct sdio_device_id *id)
{
	if (id->class != (__u8)SDIO_ANY_ID && id->class != func->class)
		return NULL;
	if (id->vendor != (__u16)SDIO_ANY_ID && id->vendor != func->vendor)
		return NULL;
	if (id->device != (__u16)SDIO_ANY_ID && id->device != func->device)
		return NULL;
	return id;
}

static const struct sdio_device_id *sdio_match_device(struct sdio_func *func,
	struct sdio_driver *drv)
{
	const struct sdio_device_id *ids;

	if (drv == NULL || func == NULL) {
		return NULL;
	}

	ids = drv->id_table;

	if (ids) {
		while (ids->class || ids->vendor || ids->device) {
			if (sdio_match_one(func, ids))
				return ids;
			ids++;
		}
	}

	return NULL;
}

static int sdio_bus_match(struct vmm_device *dev, struct vmm_driver *drv)
{
	struct sdio_func *func;
	struct sdio_driver *sdrv;

	if (dev->type != &sdio_func_type) {
		return 0;
	}

	func = dev_to_sdio_func(dev);
	sdrv = to_sdio_driver(drv);

	if (sdio_match_device(func, sdrv)) {
		return 1;
	}

	return 0;
}

static int sdio_bus_probe(struct vmm_device *dev)
{
	struct sdio_driver *drv;
	struct sdio_func *func;
	const struct sdio_device_id *id;
	int ret;

	if (dev->type != &sdio_func_type) {
		return VMM_ENODEV;
	}

	func = dev_to_sdio_func(dev);
	drv = to_sdio_driver(dev->driver);

	id = sdio_match_device(func, drv);
	if (!id) {
		return VMM_ENODEV;
	}

	/* Set the default block size so the driver is sure it's something
	 * sensible. */
	ret = sdio_set_block_size(func, 0);
	if (ret) {
		return ret;
	}

	if (drv->probe) {
		ret = drv->probe(func, id);
	} else {
		ret = VMM_ENODEV;
	}

	return ret;
}

static int sdio_bus_remove(struct vmm_device *dev)
{
	struct sdio_driver *drv;
	struct sdio_func *func;
	int ret = 0;

	if (dev->type != &sdio_func_type) {
		return VMM_ENODEV;
	}

	func = dev_to_sdio_func(dev);
	drv = to_sdio_driver(dev->driver);

	if (drv->remove) {
		drv->remove(func);
		ret = VMM_OK;
	} else {
		ret = VMM_ENODEV;
	}

	return ret;
}

static void sdio_release_device(struct vmm_device *ddev)
{
	/* TODO : Nothing to do here as of now */
	return;
}

static void sdio_release_func(struct vmm_device *dev)
{
	struct sdio_func *func = dev_to_sdio_func(dev);

	vmm_free(func);
}

struct vmm_device_type sdio_device_type = {
	.name = "sdio_device",
	.release = sdio_release_device,
};
VMM_EXPORT_SYMBOL(sdio_device_type);

struct vmm_device_type sdio_func_type = {
	.name = "sdio_func",
	.release = sdio_release_func,
};
VMM_EXPORT_SYMBOL(sdio_func_type);

struct vmm_bus sdio_bus_type = {
	.name = "sdio",
	.match = sdio_bus_match,
	.probe = sdio_bus_probe,
	.remove = sdio_bus_remove,
};
VMM_EXPORT_SYMBOL(sdio_bus_type);

/**
 *	sdio_register_driver - register a function driver
 *	@drv: SDIO function driver
 */
int sdio_register_driver(struct sdio_driver *drv)
{
	strncpy(drv->drv.name, drv->name, sizeof(drv->drv.name));
	drv->drv.bus = &sdio_bus_type;
	return vmm_devdrv_register_driver(&drv->drv);
}
VMM_EXPORT_SYMBOL(sdio_register_driver);

/**
 *	sdio_unregister_driver - unregister a function driver
 *	@drv: SDIO function driver
 */
void sdio_unregister_driver(struct sdio_driver *drv)
{
	drv->drv.bus = &sdio_bus_type;
	vmm_devdrv_unregister_driver(&drv->drv);
}
VMM_EXPORT_SYMBOL(sdio_unregister_driver);

/*
 * Allocate and initialise a new SDIO function structure.
 */
struct sdio_func *sdio_alloc_func(struct mmc_card *card)
{
	struct sdio_func *func;

	func = vmm_zalloc(sizeof(struct sdio_func));
	if (!func)
		return VMM_ERR_PTR(VMM_ENOMEM);

	func->card = card;

	vmm_devdrv_initialize_device(&func->dev);

	func->dev.parent = &card->dev;
	func->dev.bus = &sdio_bus_type;
	func->dev.release = sdio_release_func;

	return func;
}

/*
 * Register a new SDIO function with the driver model.
 */
int sdio_add_func(struct sdio_func *func)
{
	int ret;

	ret = vmm_devdrv_register_device(&func->dev);
	if (ret == 0)
		sdio_func_set_present(func);

	return ret;
}

/*
 * Unregister a SDIO function with the driver model, and
 * (eventually) free it.
 * This function can be called through error paths where sdio_add_func() was
 * never executed (because a failure occurred at an earlier point).
 */
void sdio_remove_func(struct sdio_func *func)
{
	if (!sdio_func_present(func))
		return;

	vmm_devdrv_unregister_device(&func->dev);
}
