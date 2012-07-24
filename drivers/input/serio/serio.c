/**
 * Copyright (c) 2012 Anup Patel.
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
 * @file serio.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief The Serio abstraction module
 *
 * The source has been largely adapted from Linux 3.x or higher:
 * drivers/input/serio/serio.c
 *
 *  Copyright (c) 1999-2004 Vojtech Pavlik
 *  Copyright (c) 2004 Dmitry Torokhov
 *  Copyright (c) 2003 Daniele Bellucci
 *
 * The original code is licensed under the GPL.
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <linux/serio.h>

#define MODULE_VARID			serio_framework_module
#define MODULE_NAME			"Serial IO Framework"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_IPRIORITY		SERIO_IPRIORITY
#define	MODULE_INIT			serio_init
#define	MODULE_EXIT			serio_exit

/*
 * serio_lock protects entire serio subsystem and is taken every time
 * serio port or driver registered or unregistered.
 */
static spinlock_t serio_lock;

static LIST_HEAD(serio_list);

static LIST_HEAD(serio_drv_list);

static void serio_add_port(struct serio *serio);
static int serio_reconnect_port(struct serio *serio);
static void serio_disconnect_port(struct serio *serio);
static void serio_reconnect_subtree(struct serio *serio);
static void serio_attach_driver(struct serio_driver *drv);

static int serio_connect_driver(struct serio *serio, struct serio_driver *drv)
{
	int retval;
	irq_flags_t flags;

	spin_lock_irqsave(&serio->drv_lock, flags);
	retval = drv->connect(serio, drv);
	spin_unlock_irqrestore(&serio->drv_lock, flags);

	return retval;
}

static int serio_reconnect_driver(struct serio *serio)
{
	int retval = -1;
	irq_flags_t flags;

	spin_lock_irqsave(&serio->drv_lock, flags);
	if (serio->drv && serio->drv->reconnect)
		retval = serio->drv->reconnect(serio);
	spin_unlock_irqrestore(&serio->drv_lock, flags);

	return retval;
}

static void serio_disconnect_driver(struct serio *serio)
{
	irq_flags_t flags;

	spin_lock_irqsave(&serio->drv_lock, flags);
	if (serio->drv)
		serio->drv->disconnect(serio);
	spin_unlock_irqrestore(&serio->drv_lock, flags);
}

static int serio_match_port(const struct serio_device_id *ids, struct serio *serio)
{
	while (ids->type || ids->proto) {
		if ((ids->type == SERIO_ANY || ids->type == serio->id.type) &&
		    (ids->proto == SERIO_ANY || ids->proto == serio->id.proto) &&
		    (ids->extra == SERIO_ANY || ids->extra == serio->id.extra) &&
		    (ids->id == SERIO_ANY || ids->id == serio->id.id))
			return 1;
		ids++;
	}
	return 0;
}

/*
 * Basic serio -> driver core mappings
 */

static int serio_bind_driver(struct serio *serio, struct serio_driver *drv)
{
	if (serio_match_port(drv->id_table, serio)) {
		serio->drv = drv;
		if (serio_connect_driver(serio, drv)) {
			serio->drv = NULL;
			return VMM_ENODEV;
		}
	}
	return 0;
}

static void serio_find_driver(struct serio *serio)
{
	struct list_head *l;
	struct serio_driver *drv;

	if (!serio || serio->drv) {
		return;
	}

	list_for_each(l, &serio_drv_list) {
		drv = list_entry(l, struct serio_driver, node);
		if (!serio_bind_driver(serio, drv)) {
			break;
		}
	}
}

static void serio_attach_driver(struct serio_driver *drv)
{
	struct list_head *l;
	struct serio *serio;

	if (!drv) {
		return;
	}

	list_for_each(l, &serio_list) {
		serio = list_entry(l, struct serio, node);
		if (!serio->drv) {
			serio_bind_driver(serio, drv);
		}
	}
}

/*
 * Prepare serio port for registration.
 */
static void serio_init_port(struct serio *serio)
{
	INIT_LIST_HEAD(&serio->node);
	INIT_LIST_HEAD(&serio->child_node);
	INIT_LIST_HEAD(&serio->children);
	INIT_SPIN_LOCK(&serio->lock);
	INIT_SPIN_LOCK(&serio->drv_lock);
	serio->drv = NULL;
	if (serio->parent) {
		serio->depth = serio->parent->depth + 1;
	} else {
		serio->depth = 0;
	}
}

/*
 * Complete serio port registration.
 * Driver core will attempt to find appropriate driver for the port.
 */
static void serio_add_port(struct serio *serio)
{
	struct serio *parent = serio->parent;

	if (parent) {
		serio_pause_rx(parent);
		list_add_tail(&serio->child_node, &parent->children);
		serio_continue_rx(parent);
	}

	list_add_tail(&serio->node, &serio_list);

	if (serio->start)
		serio->start(serio);

	serio_find_driver(serio);
}

/*
 * serio_destroy_port() completes unregistration process and removes
 * port from the system
 */
static void serio_destroy_port(struct serio *serio)
{
	if (serio->stop)
		serio->stop(serio);

	if (serio->parent) {
		serio_pause_rx(serio->parent);
		list_del(&serio->child_node);
		serio_continue_rx(serio->parent);
		serio->parent = NULL;
	}

	list_del(&serio->node);
}

/*
 * Reconnect serio port (re-initialize attached device).
 * If reconnect fails (old device is no longer attached or
 * there was no device to begin with) we do full rescan in
 * hope of finding a driver for the port.
 */
static int serio_reconnect_port(struct serio *serio)
{
	int error = serio_reconnect_driver(serio);

	if (error) {
		serio_disconnect_port(serio);
		serio_find_driver(serio);
	}

	return error;
}

/*
 * Reconnect serio port and all its children (re-initialize attached
 * devices).
 */
static void serio_reconnect_subtree(struct serio *root)
{
	struct serio *s = root;
	int error;

	do {
		error = serio_reconnect_port(s);
		if (!error) {
			/*
			 * Reconnect was successful, move on to do the
			 * first child.
			 */
			if (!list_empty(&s->children)) {
				s = list_first_entry(&s->children,
						     struct serio, child_node);
				continue;
			}
		}

		/*
		 * Either it was a leaf node or reconnect failed and it
		 * became a leaf node. Continue reconnecting starting with
		 * the next sibling of the parent node.
		 */
		while (s != root) {
			struct serio *parent = s->parent;

			if (!list_is_last(&s->child_node, &parent->children)) {
				s = list_entry(s->child_node.next,
					       struct serio, child_node);
				break;
			}

			s = parent;
		}
	} while (s != root);
}

/*
 * serio_disconnect_port() unbinds a port from its driver. As a side effect
 * all children ports are unbound and destroyed.
 */
static void serio_disconnect_port(struct serio *serio)
{
	struct serio *s = serio;

	/*
	 * Children ports should be disconnected and destroyed
	 * first; we travel the tree in depth-first order.
	 */
	while (!list_empty(&serio->children)) {

		/* Locate a leaf */
		while (!list_empty(&s->children))
			s = list_first_entry(&s->children,
					     struct serio, child_node);

		/*
		 * Prune this leaf node unless it is the one we
		 * started with.
		 */
		if (s != serio) {
			struct serio *parent = s->parent;

			serio_disconnect_driver(s);
			serio_destroy_port(s);

			s = parent;
		}
	}

	/*
	 * OK, no children left, now disconnect this port.
	 */
	serio_disconnect_driver(serio);
}

static void serio_set_drv(struct serio *serio, struct serio_driver *drv)
{
	serio_pause_rx(serio);
	serio->drv = drv;
	serio_continue_rx(serio);
}

#if 0
/* FIXME:
 * Cleanup serio driver when doing PM suspend()
 */
static void serio_cleanup(struct serio *serio)
{
	irq_flags_t flags;

	spin_lock_irqsave(&serio->drv_lock, flags);
	if (serio->drv && serio->drv->cleanup) {
		serio->drv->cleanup(serio);
	}
	spin_unlock_irqrestore(&serio->drv_lock, flags);
}
#endif

void serio_rescan(struct serio *serio)
{
	irq_flags_t flags;

	spin_lock_irqsave(&serio_lock, flags);
	serio_disconnect_port(serio);
	serio_find_driver(serio);
	spin_unlock_irqrestore(&serio_lock, flags);
}

void serio_reconnect(struct serio *serio)
{
	irq_flags_t flags;

	spin_lock_irqsave(&serio_lock, flags);
	serio_reconnect_subtree(serio);
	spin_unlock_irqrestore(&serio_lock, flags);
}

/*
 * Submits register request to kseriod for subsequent execution.
 * Note that port registration is always asynchronous.
 */
void __serio_register_port(struct serio *serio)
{
	irq_flags_t flags;

	serio_init_port(serio);
	spin_lock_irqsave(&serio_lock, flags);
	serio_add_port(serio);
	spin_unlock_irqrestore(&serio_lock, flags);
}

/*
 * Synchronously unregisters serio port.
 */
void serio_unregister_port(struct serio *serio)
{
	irq_flags_t flags;

	spin_lock_irqsave(&serio_lock, flags);
	serio_disconnect_port(serio);
	serio_destroy_port(serio);
	spin_unlock_irqrestore(&serio_lock, flags);
}

/*
 * Safely unregisters children ports if they are present.
 */
void serio_unregister_child_port(struct serio *serio)
{
	irq_flags_t flags;
	struct serio *s, *next;

	spin_lock_irqsave(&serio_lock, flags);
	list_for_each_entry_safe(s, next, &serio->children, child_node) {
		serio_disconnect_port(s);
		serio_destroy_port(s);
	}
	spin_unlock_irqrestore(&serio_lock, flags);
}

int __serio_register_driver(struct serio_driver *drv, const char *name)
{
	bool found;
	irq_flags_t flags;
	struct dlist *l;
	struct serio_driver *sdrv;

	if (!(drv && name)) {
		return VMM_EFAIL;
	}

	sdrv = NULL;
	found = FALSE;

	spin_lock_irqsave(&serio_lock, flags);

	list_for_each(l, &serio_drv_list) {
		sdrv = list_entry(l, struct serio_driver, node);
		if (vmm_strcmp(sdrv->name, drv->name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (found) {
		spin_unlock_irqrestore(&serio_lock, flags);
		return VMM_EFAIL;
	}

	drv->name = name;
	drv->manual_bind = FALSE; /* Xvisor ignores this */
	INIT_LIST_HEAD(&drv->node);
	list_add_tail(&drv->node, &serio_drv_list);

	serio_attach_driver(drv);

	spin_unlock_irqrestore(&serio_lock, flags);

	return VMM_OK;
}

void serio_unregister_driver(struct serio_driver *drv)
{
	irq_flags_t flags;
	struct serio *serio;

	spin_lock_irqsave(&serio_lock, flags);

start_over:
	list_for_each_entry(serio, &serio_list, node) {
		if (serio->drv == drv) {
			serio_disconnect_port(serio);
			serio_find_driver(serio);
			/* we could've deleted some ports, restart */
			goto start_over;
		}
	}

	list_del(&drv->node);

	spin_unlock_irqrestore(&serio_lock, flags);
}

/* called from serio_driver->connect/disconnect methods under serio_mutex */
int serio_open(struct serio *serio, struct serio_driver *drv)
{
	serio_set_drv(serio, drv);

	if (serio->open && serio->open(serio)) {
		serio_set_drv(serio, NULL);
		return -1;
	}
	return 0;
}

/* called from serio_driver->connect/disconnect methods under serio_mutex */
void serio_close(struct serio *serio)
{
	if (serio->close)
		serio->close(serio);

	serio_set_drv(serio, NULL);
}

irqreturn_t serio_interrupt(struct serio *serio,
		unsigned char data, unsigned int dfl)
{
	irq_flags_t flags;
	irqreturn_t ret = IRQ_NONE;

	spin_lock_irqsave(&serio->lock, flags);

        if (likely(!!serio->drv)) {
                ret = serio->drv->interrupt(serio, data, dfl);
	} else if (!dfl) {
		/* FIXME: Linux will rescan serio port at this point */
		ret = IRQ_HANDLED;
	}

	spin_unlock_irqrestore(&serio->lock, flags);

	return ret;
}

static int __init serio_init(void)
{
	INIT_LIST_HEAD(&serio_list);
	INIT_LIST_HEAD(&serio_drv_list);
	INIT_SPIN_LOCK(&serio_lock);

	return VMM_OK;
}

static void serio_exit(void)
{
}

VMM_DECLARE_MODULE(MODULE_VARID,
		   MODULE_NAME,
		   MODULE_AUTHOR,
		   MODULE_IPRIORITY,
		   MODULE_INIT,
		   MODULE_EXIT);
