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
 * @file vmm_netswitch.c
 * @author Pranav Sawargaonkar <pranav.sawargaonkar@gmail.com>
 * @brief Switch/Bridge layer for packet switching.
 */

#include <vmm_error.h>
#include <vmm_list.h>
#include <vmm_heap.h>
#include <vmm_string.h>
#include <vmm_stdio.h>
#include <vmm_modules.h>
#include <vmm_devdrv.h>
#include <net/vmm_netswitch.h>

struct vmm_netswitch *vmm_netswitch_alloc(char *name)
{
	struct vmm_netswitch *nsw;

	nsw = vmm_malloc(sizeof(struct vmm_netswitch));

	if (!nsw) {
		vmm_printf("%s Failed to allocate net switch\n", __func__);
		return NULL;
	}

	vmm_memset(nsw, 0, sizeof(struct vmm_netswitch));
	vmm_strcpy(nsw->name, name);

	INIT_LIST_HEAD(&nsw->port_list);

	return nsw;
}

int vmm_netswitch_register(struct vmm_netswitch *nsw)
{
	struct vmm_classdev *cd;
	int rc;

	if (nsw == NULL)
		return VMM_EFAIL;

	cd = vmm_malloc(sizeof(struct vmm_classdev));
	if (!cd) {
		rc = VMM_EFAIL;
		goto ret;
	}

	INIT_LIST_HEAD(&cd->head);
	vmm_strcpy(cd->name, nsw->name);
	cd->dev = nsw->dev;
	cd->priv = nsw;

	rc = vmm_devdrv_register_classdev(VMM_NETSWITCH_CLASS_NAME, cd);
	if (rc != VMM_OK) {
		vmm_printf("%s: Failed to class register network switch %s "
			   "with err 0x%x\n", __func__, nsw->name, rc);
		goto fail_nsw_reg;
	}

	vmm_printf("Successfully registered VMM net switch: %s\n", nsw->name);

	return rc;

fail_nsw_reg:
	cd->dev = NULL;
	cd->priv = NULL;
	vmm_free(cd);
ret:
	return rc;
}

int vmm_netswitch_unregister(struct vmm_netswitch *nsw)
{
	int rc;
	struct vmm_classdev *cd;

	if (nsw == NULL)
		return VMM_EFAIL;

	cd = vmm_devdrv_find_classdev(VMM_NETSWITCH_CLASS_NAME, nsw->name);
	if (!cd)
		return VMM_EFAIL;

	rc = vmm_devdrv_unregister_classdev(VMM_NETSWITCH_CLASS_NAME, cd);

	if (!rc)
		vmm_free(cd);

	return rc;
}

struct vmm_netswitch *vmm_netswitch_find(const char *name)
{
	struct vmm_classdev *cd;

	cd = vmm_devdrv_find_classdev(VMM_NETSWITCH_CLASS_NAME, name);

	if (!cd)
		return NULL;

	return cd->priv;
}

struct vmm_netswitch *vmm_netswitch_get(int num)
{
	struct vmm_classdev *cd;

	cd = vmm_devdrv_classdev(VMM_NETSWITCH_CLASS_NAME, num);

	if (!cd)
		return NULL;

	return cd->priv;
}

u32 vmm_netswitch_count(void)
{
	return vmm_devdrv_classdev_count(VMM_NETSWITCH_CLASS_NAME);
}

int vmm_netswitch_init(void)
{
	int rc;
	struct vmm_class *c;

	vmm_printf("Initialize Networking Patcket Switch layer Framework\n");

	c = vmm_malloc(sizeof(struct vmm_class));
	if (!c)
		return VMM_EFAIL;

	INIT_LIST_HEAD(&c->head);
	vmm_strcpy(c->name, VMM_NETSWITCH_CLASS_NAME);
	INIT_LIST_HEAD(&c->classdev_list);

	rc = vmm_devdrv_register_class(c);
	if (rc) {
		vmm_free(c);
		return rc;
	}

	return VMM_OK;
}

int vmm_netswitch_exit(void)
{
	int rc;
	struct vmm_class *c;

	c = vmm_devdrv_find_class(VMM_NETSWITCH_CLASS_NAME);
	if (!c)
		return VMM_OK;

	rc = vmm_devdrv_unregister_class(c);
	if (rc) {
		vmm_printf("Failed to unregister %s class",
			VMM_NETSWITCH_CLASS_NAME);
		return rc;
	}

	vmm_free(c);

	return VMM_OK;
}
