#ifndef _LINUX_DEVICE_H
#define _LINUX_DEVICE_H

#include <vmm_stdio.h>
#include <vmm_devtree.h>
#include <vmm_devdrv.h>

#include <linux/err.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/cache.h>
#include <linux/printk.h>
#include <linux/module.h>

#define bus_type			vmm_bus
#define device				vmm_device
#define device_driver			vmm_driver
#define device_node			vmm_devtree_node

#define bus_register(bus)		vmm_devdrv_register_bus(bus)
#define bus_unregister(bus)		vmm_devdrv_unregister_bus(bus)

#define get_device(dev)			vmm_devdrv_ref_device(dev)
#define put_device(dev)			vmm_devdrv_free_device(dev)
#define device_is_registered(dev)	vmm_devdrv_isregistered_device(dev)
#define dev_name(dev)			(dev)->name
#define dev_set_name(dev, msg...)	vmm_sprintf((dev)->name, msg)
#define device_initialize(dev)		vmm_devdrv_initialize_device(dev)
#define device_add(dev)			vmm_devdrv_register_device(dev)
#define device_attach(dev)		vmm_devdrv_attach_device(dev)
#define device_bind_driver(dev)		vmm_devdrv_attach_device(dev)
#define device_release_driver(dev)	vmm_devdrv_dettach_device(dev)
#define device_del(dev)			vmm_devdrv_unregister_device(dev)

#define driver_register(drv)		vmm_devdrv_register_driver(drv)
#define driver_attach(drv)		vmm_devdrv_attach_driver(drv)
#define driver_dettach(drv)		vmm_devdrv_dettach_driver(drv)
#define driver_unregister(drv)		vmm_devdrv_unregister_driver(drv)

#define dev_get_drvdata(dev)		vmm_devdrv_get_data(dev)
#define dev_set_drvdata(dev, data)	vmm_devdrv_set_data(dev, data)

/* FIXME: This file is just a place holder in most cases. */

#endif /* _LINUX_DEVICE_H */
