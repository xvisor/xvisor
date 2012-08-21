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

#include <vmm_modules.h>
#include <linux/errno.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/serio.h>

#define MODULE_DESC			"Serial IO Framework"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		SERIO_IPRIORITY
#define	MODULE_INIT			serio_init
#define	MODULE_EXIT			serio_exit

/*
 * serio_mutex protects entire serio subsystem and is taken every time
 * serio port or driver registered or unregistered.
 */
static DEFINE_MUTEX(serio_mutex);

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

	mutex_lock(&serio->drv_mutex);
	retval = drv->connect(serio, drv);
	mutex_unlock(&serio->drv_mutex);

	return retval;
}

static int serio_reconnect_driver(struct serio *serio)
{
	int retval = -1;

	mutex_lock(&serio->drv_mutex);
	if (serio->drv && serio->drv->reconnect)
		retval = serio->drv->reconnect(serio);
	mutex_unlock(&serio->drv_mutex);

	return retval;
}

static void serio_disconnect_driver(struct serio *serio)
{
	mutex_lock(&serio->drv_mutex);
	if (serio->drv)
		serio->drv->disconnect(serio);
	mutex_unlock(&serio->drv_mutex);
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
			return -ENODEV;
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

/*
 * Serio event processing.
 */

enum serio_event_type {
	SERIO_RESCAN_PORT,
	SERIO_RECONNECT_PORT,
	SERIO_RECONNECT_SUBTREE,
	SERIO_REGISTER_PORT,
	SERIO_ATTACH_DRIVER,
};

struct serio_event {
	enum serio_event_type type;
	void *object;
	struct list_head node;
};

static DEFINE_SPINLOCK(serio_event_lock);	/* protects serio_event_list */
static LIST_HEAD(serio_event_list);

static struct serio_event *serio_get_event(void)
{
	struct serio_event *event = NULL;
	irq_flags_t flags;

	spin_lock_irqsave(&serio_event_lock, flags);

	if (!list_empty(&serio_event_list)) {
		event = list_first_entry(&serio_event_list,
					 struct serio_event, node);
		list_del_init(&event->node);
	}

	spin_unlock_irqrestore(&serio_event_lock, flags);
	return event;
}

static void serio_free_event(struct serio_event *event)
{
	kfree(event);
}

static void serio_remove_duplicate_events(void *object,
					  enum serio_event_type type)
{
	struct serio_event *e, *next;
	irq_flags_t flags;

	spin_lock_irqsave(&serio_event_lock, flags);

	list_for_each_entry_safe(e, next, &serio_event_list, node) {
		if (object == e->object) {
			/*
			 * If this event is of different type we should not
			 * look further - we only suppress duplicate events
			 * that were sent back-to-back.
			 */
			if (type != e->type)
				break;

			list_del_init(&e->node);
			serio_free_event(e);
		}
	}

	spin_unlock_irqrestore(&serio_event_lock, flags);
}

static void serio_handle_event(struct vmm_work *work)
{
	struct serio_event *event;

	mutex_lock(&serio_mutex);

	while ((event = serio_get_event())) {

		switch (event->type) {

		case SERIO_REGISTER_PORT:
			serio_add_port(event->object);
			break;

		case SERIO_RECONNECT_PORT:
			serio_reconnect_port(event->object);
			break;

		case SERIO_RESCAN_PORT:
			serio_disconnect_port(event->object);
			serio_find_driver(event->object);
			break;

		case SERIO_RECONNECT_SUBTREE:
			serio_reconnect_subtree(event->object);
			break;

		case SERIO_ATTACH_DRIVER:
			serio_attach_driver(event->object);
			break;
		}

		serio_remove_duplicate_events(event->object, event->type);
		serio_free_event(event);
	}

	mutex_unlock(&serio_mutex);
}

static DECLARE_WORK(serio_event_work, serio_handle_event);

static int serio_queue_event(void *object,
			     enum serio_event_type event_type)
{
	unsigned long flags;
	struct serio_event *event;
	int retval = 0;

	spin_lock_irqsave(&serio_event_lock, flags);

	/*
	 * Scan event list for the other events for the same serio port,
	 * starting with the most recent one. If event is the same we
	 * do not need add new one. If event is of different type we
	 * need to add this event and should not look further because
	 * we need to preseve sequence of distinct events.
	 */
	list_for_each_entry_reverse(event, &serio_event_list, node) {
		if (event->object == object) {
			if (event->type == event_type)
				goto out;
			break;
		}
	}

	event = kmalloc(sizeof(struct serio_event), GFP_ATOMIC);
	if (!event) {
		printk("Not enough memory to queue event %d\n", event_type);
		retval = -ENOMEM;
		goto out;
	}

	event->type = event_type;
	event->object = object;

	list_add_tail(&event->node, &serio_event_list);
	queue_work(system_long_wq, &serio_event_work);

out:
	spin_unlock_irqrestore(&serio_event_lock, flags);
	return retval;
}

/*
 * Remove all events that have been submitted for a given
 * object, be it serio port or driver.
 */
static void serio_remove_pending_events(void *object)
{
	struct serio_event *event, *next;
	unsigned long flags;

	spin_lock_irqsave(&serio_event_lock, flags);

	list_for_each_entry_safe(event, next, &serio_event_list, node) {
		if (event->object == object) {
			list_del_init(&event->node);
			serio_free_event(event);
		}
	}

	spin_unlock_irqrestore(&serio_event_lock, flags);
}

/*
 * Locate child serio port (if any) that has not been fully registered yet.
 *
 * Children are registered by driver's connect() handler so there can't be a
 * grandchild pending registration together with a child.
 */
static struct serio *serio_get_pending_child(struct serio *parent)
{
	struct serio_event *event;
	struct serio *serio, *child = NULL;
	unsigned long flags;

	spin_lock_irqsave(&serio_event_lock, flags);

	list_for_each_entry(event, &serio_event_list, node) {
		if (event->type == SERIO_REGISTER_PORT) {
			serio = event->object;
			if (serio->parent == parent) {
				child = serio;
				break;
			}
		}
	}

	spin_unlock_irqrestore(&serio_event_lock, flags);
	return child;
}

#if 0
static void serio_release_port(struct device *dev)
{
	struct serio *serio = to_serio_port(dev);

	kfree(serio);
	module_put(THIS_MODULE);
}
#endif

/*
 * Prepare serio port for registration.
 */
static void serio_init_port(struct serio *serio)
{
	INIT_LIST_HEAD(&serio->node);
	INIT_LIST_HEAD(&serio->child_node);
	INIT_LIST_HEAD(&serio->children);
	spin_lock_init(&serio->lock);
	mutex_init(&serio->drv_mutex);
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
	struct serio *child;

	while ((child = serio_get_pending_child(serio)) != NULL) {
		serio_remove_pending_events(child);
	}

	if (serio->stop)
		serio->stop(serio);

	if (serio->parent) {
		serio_pause_rx(serio->parent);
		list_del(&serio->child_node);
		serio_continue_rx(serio->parent);
		serio->parent = NULL;
	}

	list_del(&serio->node);
	serio_remove_pending_events(serio);
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

void serio_rescan(struct serio *serio)
{
	serio_queue_event(serio, SERIO_RESCAN_PORT);
}

void serio_reconnect(struct serio *serio)
{
	serio_queue_event(serio, SERIO_RECONNECT_SUBTREE);
}

/*
 * Submits register request to kseriod for subsequent execution.
 * Note that port registration is always asynchronous.
 */
void __serio_register_port(struct serio *serio)
{
	serio_init_port(serio);
	serio_queue_event(serio, SERIO_REGISTER_PORT);
}

/*
 * Synchronously unregisters serio port.
 */
void serio_unregister_port(struct serio *serio)
{
	mutex_lock(&serio_mutex);
	serio_disconnect_port(serio);
	serio_destroy_port(serio);
	mutex_unlock(&serio_mutex);
}

/*
 * Safely unregisters children ports if they are present.
 */
void serio_unregister_child_port(struct serio *serio)
{
	struct serio *s, *next;

	mutex_lock(&serio_mutex);
	list_for_each_entry_safe(s, next, &serio->children, child_node) {
		serio_disconnect_port(s);
		serio_destroy_port(s);
	}
	mutex_unlock(&serio_mutex);
}

#if 0
/* FIXME:
 * Cleanup serio driver when doing PM suspend()
 */
static void serio_cleanup(struct serio *serio)
{
	mutex_lock(&serio->drv_mutex);
	if (serio->drv && serio->drv->cleanup)
		serio->drv->cleanup(serio);
	mutex_unlock(&serio->drv_mutex);
}
#endif

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

int __serio_register_driver(struct serio_driver *drv)
{
	bool found;
	struct dlist *l;
	struct serio_driver *sdrv;

	if (!drv) {
		return -EFAIL;
	}

	sdrv = NULL;
	found = FALSE;

	mutex_lock(&serio_mutex);

	list_for_each(l, &serio_drv_list) {
		sdrv = list_entry(l, struct serio_driver, node);
		if (strcmp(sdrv->name, drv->name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (found) {
		mutex_unlock(&serio_mutex);
		return -EFAIL;
	}

	drv->manual_bind = FALSE; /* Xvisor ignores this */
	INIT_LIST_HEAD(&drv->node);
	list_add_tail(&drv->node, &serio_drv_list);

	mutex_unlock(&serio_mutex);

	return serio_queue_event(drv, SERIO_ATTACH_DRIVER);
}

void serio_unregister_driver(struct serio_driver *drv)
{
	struct serio *serio;

	mutex_lock(&serio_mutex);

	serio_remove_pending_events(drv);

	list_del(&drv->node);

start_over:
	list_for_each_entry(serio, &serio_list, node) {
		if (serio->drv == drv) {
			serio_disconnect_port(serio);
			serio_find_driver(serio);
			/* we could've deleted some ports, restart */
			goto start_over;
		}
	}

	mutex_unlock(&serio_mutex);
}

static void serio_set_drv(struct serio *serio, struct serio_driver *drv)
{
	serio_pause_rx(serio);
	serio->drv = drv;
	serio_continue_rx(serio);
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
	return 0;
}

static void __exit serio_exit(void)
{
	/*
	 * There should not be any outstanding events but work may
	 * still be scheduled so simply cancel it.
	 */
	cancel_work_sync(&serio_event_work);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
