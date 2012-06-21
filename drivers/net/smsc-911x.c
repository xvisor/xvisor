/**
 * Copyright (c) 2012 Pranav Sawargaonkar.
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
 * @file smsc-911x.c
 * @author Pranav Sawargaonkar (pranav.sawargaonkar@gmail.com)
 * @brief Driver for SMSC-911x network controller.
 */

#include <vmm_heap.h>
#include <vmm_host_io.h>
#include <vmm_host_aspace.h>
#include <vmm_stdio.h>
#include <vmm_error.h>
#include <vmm_string.h>
#include <vmm_modules.h>
#include <vmm_devtree.h>
#include <vmm_devdrv.h>
#include <net/vmm_netdev.h>
#include <net/vmm_netswitch.h>

#define MODULE_VARID			smsc_911x_driver_module
#define MODULE_NAME			"SMSC 911x Ethernet Controller Driver"
#define MODULE_AUTHOR			"Pranav Sawargaonkar"
#define MODULE_IPRIORITY		(VMM_NETDEV_CLASS_IPRIORITY + 1)
#define	MODULE_INIT			smsc_911x_driver_init
#define	MODULE_EXIT			smsc_911x_driver_exit

static int smsc_911x_init(struct vmm_netdev *ndev)
{
	int rc = VMM_OK;

	vmm_printf("Inside %s\n", __func__);

	return rc;
}

static struct vmm_netdev_ops smsc_911x_vmm_netdev_ops = {
	.ndev_init = smsc_911x_init,
};

static int smsc_911x_driver_probe(struct vmm_device *dev,
				  const struct vmm_devid *devid)
{
	int rc;
	struct vmm_netdev *ndev;

	vmm_printf("Inside smsc_911x_driver_probe\n");

	ndev = vmm_netdev_alloc(dev->node->name);
	if (!ndev) {
		vmm_printf("%s Failed to allocate vmm_netdev for %s\n", __func__,
				dev->node->name);
		rc = VMM_EFAIL;
		goto free_nothing;
	}


	dev->priv = (void *) ndev;
	ndev->dev_ops = &smsc_911x_vmm_netdev_ops;

	rc = vmm_netdev_register(ndev);
	if (rc != VMM_OK) {
		vmm_printf("%s Failed to register net device %s\n", __func__,
				dev->node->name);
		goto free_ndev;

	}

	vmm_printf("Successfully registered Network Device %s\n", ndev->name);

	return VMM_OK;

free_ndev:
	vmm_free(ndev);
free_nothing:
	return rc;
}

static int smsc_911x_driver_remove(struct vmm_device *dev)
{
	int rc = VMM_OK;
	struct vmm_netdev *ndev = (struct vmm_netdev *) dev->priv;

	if (ndev) {
		rc = vmm_netdev_unregister(ndev);
		vmm_free(ndev->priv);
		vmm_free(ndev);
		dev->priv = NULL;
	}

	return rc;
}


static struct vmm_devid smsc_911x_devid_table[] = {
	{ .type = "nic", .compatible = "smsc911x"},
	{ /* end of list */ },
};

static struct vmm_driver smsc_911x_driver = {
	.name = "smsc_911x_driver",
	.match_table = smsc_911x_devid_table,
	.probe = smsc_911x_driver_probe,
	.remove = smsc_911x_driver_remove,
};

static int __init smsc_911x_driver_init(void)
{
	return vmm_devdrv_register_driver(&smsc_911x_driver);
}

static void smsc_911x_driver_exit(void)
{
	vmm_devdrv_unregister_driver(&smsc_911x_driver);
}

VMM_DECLARE_MODULE(MODULE_VARID,
		MODULE_NAME,
		MODULE_AUTHOR,
		MODULE_IPRIORITY,
		MODULE_INIT,
		MODULE_EXIT);
