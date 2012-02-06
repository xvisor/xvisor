/**
 * Copyright (c) 2010 Himanshu Chauhan.
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
 * @file vmm_netdev.c
 * @version 1.0
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief Network Device framework source
 */

#include <vmm_error.h>
#include <vmm_list.h>
#include <vmm_heap.h>
#include <vmm_string.h>
#include <vmm_stdio.h>
#include <vmm_modules.h>
#include <vmm_devdrv.h>
#include <net/vmm_netdev.h>

#define MODULE_VARID			netdev_framework_module
#define MODULE_NAME			"Network Device Framework"
#define MODULE_AUTHOR			"Himanshu Chauhan"
#define MODULE_IPRIORITY		VMM_NETDEV_CLASS_IPRIORITY
#define	MODULE_INIT			vmm_netdev_init
#define	MODULE_EXIT			vmm_netdev_exit

int vmm_netdev_doioctl(struct vmm_netdev * ndev, 
			int cmd, void *buf, size_t buf_len)
{
	int ret;

	if (!ndev) {
		return VMM_EFAIL;
	}
	if (!(ndev->ioctl)) {
		return VMM_EFAIL;
	}

	ret = ndev->ioctl(ndev, cmd, buf, buf_len);

	return ret;
}

int vmm_netdev_doread(struct vmm_netdev * ndev,
		      char *dest, size_t offset, size_t len)
{
	int ret;

	if (!ndev) {
		return 0;
	}
	if (!(ndev->read)) {
		return 0;
	}

	ret = ndev->read(ndev, dest, offset, len);

	return ret;
}

int vmm_netdev_dowrite(struct vmm_netdev * ndev,
		       char *src, size_t offset, size_t len)
{
	int ret;

	if (!ndev) {
		return 0;
	}
	if (!(ndev->write)) {
		return 0;
	}

	ret = ndev->write(ndev, src, offset, len);

	return ret;
}

int vmm_netdev_register(struct vmm_netdev * ndev)
{
	struct vmm_classdev *cd;

	if (ndev == NULL) {
		return VMM_EFAIL;
	}
	if (ndev->read == NULL || ndev->write == NULL) {
		return VMM_EFAIL;
	}

	cd = vmm_malloc(sizeof(struct vmm_classdev));
	if (!cd) {
		return VMM_EFAIL;
	}

	INIT_LIST_HEAD(&cd->head);
	vmm_strcpy(cd->name, ndev->name);
	cd->dev = ndev->dev;
	cd->priv = ndev;

	vmm_devdrv_register_classdev(VMM_NETDEV_CLASS_NAME, cd);

	return VMM_OK;
}

int vmm_netdev_unregister(struct vmm_netdev * ndev)
{
	int rc;
	struct vmm_classdev *cd;

	if (ndev == NULL) {
		return VMM_EFAIL;
	}

	cd = vmm_devdrv_find_classdev(VMM_NETDEV_CLASS_NAME, ndev->name);
	if (!cd) {
		return VMM_EFAIL;
	}

	rc = vmm_devdrv_unregister_classdev(VMM_NETDEV_CLASS_NAME, cd);

	if (!rc) {
		vmm_free(cd);
	}

	return rc;
}

struct vmm_netdev *vmm_netdev_find(const char *name)
{
	struct vmm_classdev *cd;

	cd = vmm_devdrv_find_classdev(VMM_NETDEV_CLASS_NAME, name);

	if (!cd) {
		return NULL;
	}

	return cd->priv;
}

struct vmm_netdev *vmm_netdev_get(int num)
{
	struct vmm_classdev *cd;

	cd = vmm_devdrv_classdev(VMM_NETDEV_CLASS_NAME, num);

	if (!cd) {
		return NULL;
	}

	return cd->priv;
}

u32 vmm_netdev_count(void)
{
	return vmm_devdrv_classdev_count(VMM_NETDEV_CLASS_NAME);
}

static int __init vmm_netdev_init(void)
{
	int rc;
	struct vmm_class *c;

	vmm_printf("Initialize Networking Device Framework\n");

	c = vmm_malloc(sizeof(struct vmm_class));
	if (!c) {
		return VMM_EFAIL;
	}

	INIT_LIST_HEAD(&c->head);
	vmm_strcpy(c->name, VMM_NETDEV_CLASS_NAME);
	INIT_LIST_HEAD(&c->classdev_list);

	rc = vmm_devdrv_register_class(c);
	if (rc) {
		vmm_free(c);
		return rc;
	}

	return VMM_OK;
}

static void vmm_netdev_exit(void)
{
	int rc;
	struct vmm_class *c;

	c = vmm_devdrv_find_class(VMM_NETDEV_CLASS_NAME);
	if (!c) {
		return;
	}

	rc = vmm_devdrv_unregister_class(c);
	if (rc) {
		return;
	}

	vmm_free(c);
}

VMM_DECLARE_MODULE(MODULE_VARID,
		   MODULE_NAME,
		   MODULE_AUTHOR,
		   MODULE_IPRIORITY,
		   MODULE_INIT,
		   MODULE_EXIT);
