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
#include <vmm_stdio.h>
#include <vmm_modules.h>
#include <vmm_devdrv.h>
#include <net/vmm_netport.h>
#include <libs/stringlib.h>

struct vmm_netport_xfer *vmm_netport_alloc_xfer(struct vmm_netport *port)
{
	struct dlist *l;
	irq_flags_t flags;

	if (!port) {
		return NULL;
	}

	vmm_spin_lock_irqsave(&port->free_list_lock, flags);
	if (list_empty(&port->free_list)) {
		vmm_spin_unlock_irqrestore(&port->free_list_lock, flags);
		return NULL;
	}
	l = list_pop(&port->free_list);
	port->free_count--;
	vmm_spin_unlock_irqrestore(&port->free_list_lock, flags);

	return list_entry(l, struct vmm_netport_xfer, head);;
}
VMM_EXPORT_SYMBOL(vmm_netport_alloc_xfer);

void vmm_netport_free_xfer(struct vmm_netport *port, 
			   struct vmm_netport_xfer *xfer)
{
	irq_flags_t flags;

	if (!port || !xfer) {
		return;
	}

	vmm_spin_lock_irqsave(&port->free_list_lock, flags);
	list_add_tail(&xfer->head, &port->free_list);
	port->free_count++;
	vmm_spin_unlock_irqrestore(&port->free_list_lock, flags);
}
VMM_EXPORT_SYMBOL(vmm_netport_free_xfer);

struct vmm_netport *vmm_netport_alloc(char *name, u32 queue_size)
{
	u32 i;
	struct dlist *l;
	struct vmm_netport *port;

	port = vmm_zalloc(sizeof(struct vmm_netport));
	if (!port) {
		vmm_printf("%s Failed to allocate net port\n", __func__);
		return NULL;
	}

	port->name = vmm_malloc(strlen(name)+1);
	if (!port->name) {
		vmm_printf("%s Failed to allocate for net port\n", __func__);
		return NULL;
	}

	INIT_LIST_HEAD(&port->head);
	strcpy(port->name, name);
	port->queue_size = (queue_size < VMM_NETPORT_MAX_QUEUE_SIZE) ?
				queue_size : VMM_NETPORT_MAX_QUEUE_SIZE;

	port->xfer_pool = vmm_malloc(sizeof(struct vmm_netport_xfer) * 
					port->queue_size);
	if (!port->xfer_pool) {
		vmm_printf("%s Failed to allocate for netport xfer pool\n", __func__);
		return NULL;
	}

	port->free_count = port->queue_size;
	INIT_SPIN_LOCK(&port->free_list_lock);
	INIT_LIST_HEAD(&port->free_list);

	for(i = 0; i < port->queue_size; i++) {
		l = &((port->xfer_pool + i)->head);
		list_add_tail(l, &port->free_list);
	}

	return port;
}
VMM_EXPORT_SYMBOL(vmm_netport_alloc);

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
	strcpy(cd->name, port->name);
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
VMM_EXPORT_SYMBOL(vmm_netport_register);

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
VMM_EXPORT_SYMBOL(vmm_netport_unregister);

struct vmm_netport *vmm_netport_find(const char *name)
{
	struct vmm_classdev *cd;

	cd = vmm_devdrv_find_classdev(VMM_NETPORT_CLASS_NAME, name);

	if (!cd)
		return NULL;

	return cd->priv;
}
VMM_EXPORT_SYMBOL(vmm_netport_find);

struct vmm_netport *vmm_netport_get(int num)
{
	struct vmm_classdev *cd;

	cd = vmm_devdrv_classdev(VMM_NETPORT_CLASS_NAME, num);

	if (!cd)
		return NULL;

	return cd->priv;
}
VMM_EXPORT_SYMBOL(vmm_netport_get);

u32 vmm_netport_count(void)
{
	return vmm_devdrv_classdev_count(VMM_NETPORT_CLASS_NAME);
}
VMM_EXPORT_SYMBOL(vmm_netport_count);

int __init vmm_netport_init(void)
{
	int rc;
	struct vmm_class *c;

	vmm_printf("Initialize Network Port Framework\n");

	c = vmm_malloc(sizeof(struct vmm_class));
	if (!c)
		return VMM_EFAIL;

	INIT_LIST_HEAD(&c->head);
	strcpy(c->name, VMM_NETPORT_CLASS_NAME);
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

int __exit vmm_netport_exit(void)
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

