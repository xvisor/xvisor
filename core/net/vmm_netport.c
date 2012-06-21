/**
 * Copyright (c) 2012 Sukanto Ghosh.
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
 * @file vmm_netport.c
 * @author Sukanto Ghosh <sukantoghosh@gmail.com>
 * @brief Netswitch port framework.
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_string.h>
#include <vmm_stdio.h>
#include <vmm_modules.h>
#include <vmm_devdrv.h>
#include <net/vmm_netport.h>

struct vmm_netport *vmm_netport_alloc(char *name)
{
	struct vmm_netport *port;

	port = vmm_malloc(sizeof(struct vmm_netport));
	if (!port) {
		vmm_printf("%s Failed to allocate net port\n", __func__);
		return NULL;
	}
	vmm_memset(port, 0, sizeof(struct vmm_netport));
	port->name = vmm_malloc(vmm_strlen(name)+1);
	if (!port->name) {
		vmm_printf("%s Failed to allocate for net port\n", __func__);
		return NULL;
	}

	vmm_strcpy(port->name, name);

	INIT_LIST_HEAD(&port->head);

	return port;
}

int vmm_netport_register(struct vmm_netport *port)
{
	struct vmm_classdev *cd;
	int rc;

	if (port == NULL)
		return VMM_EFAIL;

	cd = vmm_malloc(sizeof(struct vmm_classdev));
	if (!cd) {
		rc = VMM_EFAIL;
		goto ret;
	}

	INIT_LIST_HEAD(&cd->head);
	vmm_strcpy(cd->name, port->name);
	cd->dev = port->dev;
	cd->priv = port;

	rc = vmm_devdrv_register_classdev(VMM_NETPORT_CLASS_NAME, cd);
	if (rc != VMM_OK) {
		vmm_printf("%s: Failed to register %s %s "
			   "with err 0x%x\n", __func__, 
			   VMM_NETPORT_CLASS_NAME,
			   port->name, rc);
		goto fail_port_reg;
	}

#ifdef CONFIG_VERBOSE_MODE
	vmm_printf("Successfully registered VMM netport: %s\n", port->name);
#endif

	return rc;

fail_port_reg:
	cd->dev = NULL;
	cd->priv = NULL;
	vmm_free(cd);
ret:
	return rc;
}

int vmm_netport_unregister(struct vmm_netport *port)
{
	int rc;
	struct vmm_classdev *cd;

	if (port == NULL)
		return VMM_EFAIL;

	cd = vmm_devdrv_find_classdev(VMM_NETPORT_CLASS_NAME, port->name);
	if (!cd)
		return VMM_EFAIL;

	rc = vmm_devdrv_unregister_classdev(VMM_NETPORT_CLASS_NAME, cd);

	if (!rc)
		vmm_free(cd);

	return rc;
}

struct vmm_netport *vmm_netport_find(const char *name)
{
	struct vmm_classdev *cd;

	cd = vmm_devdrv_find_classdev(VMM_NETPORT_CLASS_NAME, name);

	if (!cd)
		return NULL;

	return cd->priv;
}

struct vmm_netport *vmm_netport_get(int num)
{
	struct vmm_classdev *cd;

	cd = vmm_devdrv_classdev(VMM_NETPORT_CLASS_NAME, num);

	if (!cd)
		return NULL;

	return cd->priv;
}

u32 vmm_netport_count(void)
{
	return vmm_devdrv_classdev_count(VMM_NETPORT_CLASS_NAME);
}

int vmm_netport_init(void)
{
	int rc;
	struct vmm_class *c;

	vmm_printf("Initialize Network Port Framework\n");

	c = vmm_malloc(sizeof(struct vmm_class));
	if (!c)
		return VMM_EFAIL;

	INIT_LIST_HEAD(&c->head);
	vmm_strcpy(c->name, VMM_NETPORT_CLASS_NAME);
	INIT_LIST_HEAD(&c->classdev_list);

	rc = vmm_devdrv_register_class(c);
	if (rc) {
		vmm_printf("Failed to register %s class\n",
			VMM_NETPORT_CLASS_NAME);
		vmm_free(c);
		return rc;
	}

	return VMM_OK;
}

int vmm_netport_exit(void)
{
	int rc;
	struct vmm_class *c;

	c = vmm_devdrv_find_class(VMM_NETPORT_CLASS_NAME);
	if (!c)
		return VMM_OK;

	rc = vmm_devdrv_unregister_class(c);
	if (rc) {
		vmm_printf("Failed to unregister %s class",
			VMM_NETPORT_CLASS_NAME);
		return rc;
	}

	vmm_free(c);

	return VMM_OK;
}

