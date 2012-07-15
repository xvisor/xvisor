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
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @author Pranav Sawargaonkar <pranav.sawargaonkar@gmail.com>
 * @brief Network Device framework source
 */

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

struct vmm_netdev *vmm_netdev_alloc(const char *name)
{
	struct vmm_netdev *ndev;

	ndev = vmm_malloc(sizeof(struct vmm_netdev));

	if (!ndev) {
		vmm_printf("%s Failed to allocate net device\n", __func__);
		return NULL;
	}

	vmm_memset(ndev, 0, sizeof(struct vmm_netdev));
	vmm_strcpy(ndev->name, name);
	ndev->state = VMM_NETDEV_UNINITIALIZED;

	return ndev;
}

int vmm_netdev_register(struct vmm_netdev * ndev)
{
	struct vmm_classdev *cd;
	int rc;

	if (ndev == NULL) {
		return VMM_EFAIL;
	}

	cd = vmm_malloc(sizeof(struct vmm_classdev));
	if (!cd) {
		rc = VMM_EFAIL;
		goto ret_ndev_reg;
	}

	INIT_LIST_HEAD(&cd->head);
	vmm_strcpy(cd->name, ndev->name);
	cd->dev = ndev->dev;
	cd->priv = ndev;

	rc = vmm_devdrv_register_classdev(VMM_NETDEV_CLASS_NAME, cd);
	if (rc != VMM_OK) {
		vmm_printf("%s: Failed to class register network device %s "
			   "with err 0x%x\n", __func__, ndev->name, rc);
		goto fail_ndev_reg;
	}

	if (ndev->dev_ops && ndev->dev_ops->ndev_init) {
		rc = ndev->dev_ops->ndev_init(ndev);
		if (rc != VMM_OK) {
			vmm_printf("%s: Device %s Failed during initializaion"
				   "with err %d!!!!\n", __func__ ,
				   ndev->name, rc);
			rc = vmm_devdrv_unregister_classdev(
						VMM_NETDEV_CLASS_NAME, cd);
			if (rc != VMM_OK) {
				vmm_printf("%s: Failed to class unregister "
					   "network device %s with err "
					   "0x%x", __func__, ndev->name, rc);
			}
			goto fail_ndev_reg;
		}
	}

	ndev->state &= ~VMM_NETDEV_UNINITIALIZED;
	ndev->state |= VMM_NETDEV_REGISTERED;

	return rc;

fail_ndev_reg:
	cd->dev = NULL;
	cd->priv = NULL;
	vmm_free(cd);
ret_ndev_reg:
	return rc;
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

	ndev->state &= ~(VMM_NETDEV_REGISTERED | VMM_NETDEV_TX_ALLOWED);

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

void vmm_netdev_set_link(struct vmm_netport *port)
{
	struct vmm_netdev *dev = (struct vmm_netdev *)port->priv;

	if (port->flags & VMM_NETPORT_LINK_UP) {
		dev->dev_ops->ndev_open(dev);
	} else {
		dev->dev_ops->ndev_close(dev);
	}
}

int vmm_netdev_can_receive(struct vmm_netport *port)
{
	struct vmm_netdev *dev = (struct vmm_netdev *) port->priv;

	if (vmm_netif_queue_stopped(dev))
		return 0;

	return 1;
}

int vmm_netdev_switch2port_xfer(struct vmm_netport *port,
		struct vmm_mbuf *mbuf)
{
	int rc = VMM_OK;
	struct vmm_netdev *dev = (struct vmm_netdev *) port->priv;
	char *buf;
	int len;

	if(mbuf->m_next) {
		/* Cannot avoid a copy in case of fragmented mbuf data */
		len = min(dev->mtu, (unsigned int)mbuf->m_pktlen);
		buf = vmm_malloc(len);
		m_copydata(mbuf, 0, len, buf);
		m_freem(mbuf);
		MGETHDR(mbuf, 0, 0);
		MEXTADD(mbuf, buf, len, 0, 0);
	}

	dev->dev_ops->ndev_xmit(mbuf, dev);

	return rc;
}


int __init vmm_netdev_init(void)
{
	int rc;
	struct vmm_class *c;

	vmm_printf("Initialize Network Device Framework\n");

	c = vmm_malloc(sizeof(struct vmm_class));
	if (!c) {
		return VMM_EFAIL;
	}

	INIT_LIST_HEAD(&c->head);
	vmm_strcpy(c->name, VMM_NETDEV_CLASS_NAME);
	INIT_LIST_HEAD(&c->classdev_list);

	rc = vmm_devdrv_register_class(c);
	if (rc) {
		vmm_printf("Failed to register %s class\n",
			VMM_NETDEV_CLASS_NAME);
		vmm_free(c);
		return rc;
	}

	return VMM_OK;
}

void vmm_netdev_exit(void)
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

