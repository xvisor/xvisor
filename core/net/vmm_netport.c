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
#include <net/vmm_protocol.h>
#include <net/vmm_netswitch.h>
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

	return list_entry(l, struct vmm_netport_xfer, head);
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

	INIT_LIST_HEAD(&port->head);
	if (strlcpy(port->name, name, sizeof(port->name)) >=
	    sizeof(port->name)) {
		vmm_free(port);
		return NULL;
	};

	port->queue_size = (queue_size < VMM_NETPORT_MAX_QUEUE_SIZE) ?
				queue_size : VMM_NETPORT_MAX_QUEUE_SIZE;

	port->free_count = port->queue_size;
	INIT_SPIN_LOCK(&port->free_list_lock);
	INIT_LIST_HEAD(&port->free_list);

	for(i = 0; i < port->queue_size; i++) {
		l = &((port->xfer_pool + i)->head);
		list_add_tail(l, &port->free_list);
	}

	INIT_SPIN_LOCK(&port->switch2port_xfer_lock);

	return port;
}
VMM_EXPORT_SYMBOL(vmm_netport_alloc);

int vmm_netport_free(struct vmm_netport *port)
{
	if (!port) {
		return VMM_EFAIL;
	}

	vmm_free(port);

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(vmm_netport_free);

static struct vmm_class netport_class = {
	.name = VMM_NETPORT_CLASS_NAME,
};

int vmm_netport_register(struct vmm_netport *port)
{
	int rc;

	if (port == NULL)
		return VMM_EFAIL;

	/* If port has invalid mac, assign a random one */
	if (!is_valid_ether_addr(port->macaddr)) {
		random_ether_addr(port->macaddr);
	}

	vmm_devdrv_initialize_device(&port->dev);
	if (strlcpy(port->dev.name, port->name, sizeof(port->dev.name)) >=
	    sizeof(port->dev.name)) {
		return VMM_EOVERFLOW;
	}
	port->dev.class = &netport_class;
	vmm_devdrv_set_data(&port->dev, port);

	rc = vmm_devdrv_register_device(&port->dev);
	if (rc != VMM_OK) {
		vmm_printf("%s: Failed to register %s %s (error %d)\n",
			   __func__, VMM_NETPORT_CLASS_NAME, port->name, rc);
		return rc;
	}

#ifdef CONFIG_VERBOSE_MODE
	vmm_printf("%s: Registered netport %s\n", __func__, port->name);
#endif

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(vmm_netport_register);

int vmm_netport_unregister(struct vmm_netport *port)
{
	int rc;

	if (!port) {
		return VMM_EFAIL;
	}

	rc = vmm_netswitch_port_remove(port);
	if (rc) {
		return rc;
	}

	return vmm_devdrv_unregister_device(&port->dev);
}
VMM_EXPORT_SYMBOL(vmm_netport_unregister);

struct vmm_netport *vmm_netport_find(const char *name)
{
	struct vmm_device *dev;

	dev = vmm_devdrv_class_find_device(&netport_class, name);
	if (!dev) {
		return NULL;
	}

	return vmm_devdrv_get_data(dev);
}
VMM_EXPORT_SYMBOL(vmm_netport_find);

struct vmm_netport *vmm_netport_get(int num)
{
	struct vmm_device *dev;

	dev = vmm_devdrv_class_device(&netport_class, num);
	if (!dev) {
		return NULL;
	}

	return vmm_devdrv_get_data(dev);
}
VMM_EXPORT_SYMBOL(vmm_netport_get);

u32 vmm_netport_count(void)
{
	return vmm_devdrv_class_device_count(&netport_class);
}
VMM_EXPORT_SYMBOL(vmm_netport_count);

int __init vmm_netport_init(void)
{
	int rc;

	vmm_printf("Initialize Network Port Framework\n");

	rc = vmm_devdrv_register_class(&netport_class);
	if (rc) {
		vmm_printf("Failed to register %s class\n",
			VMM_NETPORT_CLASS_NAME);
		return rc;
	}

	return VMM_OK;
}

int __exit vmm_netport_exit(void)
{
	int rc;

	rc = vmm_devdrv_unregister_class(&netport_class);
	if (rc) {
		vmm_printf("Failed to unregister %s class",
			VMM_NETPORT_CLASS_NAME);
		return rc;
	}

	return VMM_OK;
}

