/**
 * Copyright (c) 2010 Anup Patel.
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
 * @file vmm_vserial.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief source code for virtual serial port
 */

#include <vmm_error.h>
#include <vmm_compiler.h>
#include <vmm_heap.h>
#include <vmm_mutex.h>
#include <vmm_modules.h>
#include <vio/vmm_vserial.h>
#include <libs/stringlib.h>

#define MODULE_DESC			"Virtual Serial Port Framework"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(VMM_VSERIAL_IPRIORITY)
#define	MODULE_INIT			vmm_vserial_init
#define	MODULE_EXIT			vmm_vserial_exit

struct vmm_vserial_ctrl {
	struct vmm_mutex vser_list_lock;
        struct dlist vser_list;
	struct vmm_blocking_notifier_chain notifier_chain;
};

static struct vmm_vserial_ctrl vsctrl;

int vmm_vserial_register_client(struct vmm_notifier_block *nb)
{
	return vmm_blocking_notifier_register(&vsctrl.notifier_chain, nb);
}
VMM_EXPORT_SYMBOL(vmm_vserial_register_client);

int vmm_vserial_unregister_client(struct vmm_notifier_block *nb)
{
	return vmm_blocking_notifier_unregister(&vsctrl.notifier_chain, nb);
}
VMM_EXPORT_SYMBOL(vmm_vserial_unregister_client);

u32 vmm_vserial_send(struct vmm_vserial *vser, u8 *src, u32 len)
{
	u32 i;

	if (!vser || !src) {
		return 0;
	}
	if (!vser->can_send || !vser->send) {
		return 0;
	}

	for (i = 0; i < len ; i++) {
		if (!vser->can_send(vser)) {
			break;
		}
		vser->send(vser, src[i]);
	}

	return i;
}
VMM_EXPORT_SYMBOL(vmm_vserial_send);

u32 vmm_vserial_receive(struct vmm_vserial *vser, u8 *dst, u32 len)
{
	u32 i;
	irq_flags_t flags;
	struct dlist *l;
	struct vmm_vserial_receiver *receiver;

	if (!vser || !dst) {
		return 0;
	}

	vmm_spin_lock_irqsave(&vser->receiver_list_lock, flags);

	if (list_empty(&vser->receiver_list)) {
		vmm_spin_unlock_irqrestore(&vser->receiver_list_lock, flags);

		for (i = 0; i < len ; i++) {
			fifo_enqueue(vser->receive_fifo, &dst[i], TRUE);
		}

		return i;
	}

	for (i = 0; i < len ; i++) {
		list_for_each(l, &vser->receiver_list) {
			receiver = 
			   list_entry(l, struct vmm_vserial_receiver, head);
			receiver->recv(vser, receiver->priv, dst[i]);
		}
	}

	vmm_spin_unlock_irqrestore(&vser->receiver_list_lock, flags);

	return i;
}
VMM_EXPORT_SYMBOL(vmm_vserial_receive);

int vmm_vserial_register_receiver(struct vmm_vserial *vser, 
		void (*recv) (struct vmm_vserial *, void *, u8), void *priv)
{
	u8 chval;
	bool found;
	irq_flags_t flags;
	struct dlist *l;
	struct vmm_vserial_receiver *receiver;

	if (!vser || !recv) {
		return VMM_EFAIL;
	}

	receiver = NULL;
	found = FALSE;

	vmm_spin_lock_irqsave(&vser->receiver_list_lock, flags);

	list_for_each(l, &vser->receiver_list) {
		receiver = list_entry(l, struct vmm_vserial_receiver, head);
		if (receiver->recv == recv) {
			found = TRUE;
			break;
		}
	}

	if (found) {
		vmm_spin_unlock_irqrestore(&vser->receiver_list_lock, flags);
		return VMM_EINVALID;
	}

	receiver = vmm_malloc(sizeof(struct vmm_vserial_receiver));
	if (!receiver) {
		vmm_spin_unlock_irqrestore(&vser->receiver_list_lock, flags);
		return VMM_EFAIL;
	}

	INIT_LIST_HEAD(&receiver->head);
	receiver->recv = recv;
	receiver->priv = priv;

	list_add_tail(&receiver->head, &vser->receiver_list);

	vmm_spin_unlock_irqrestore(&vser->receiver_list_lock, flags);

	while (!fifo_isempty(vser->receive_fifo)) {
		if (!fifo_dequeue(vser->receive_fifo, &chval)) {
			break;
		}
		list_for_each(l, &vser->receiver_list) {
			receiver = 
			   list_entry(l, struct vmm_vserial_receiver, head);
			receiver->recv(vser, receiver->priv, chval);
		}
	}

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(vmm_vserial_register_receiver);

int vmm_vserial_unregister_receiver(struct vmm_vserial *vser, 
		void (*recv) (struct vmm_vserial *, void *, u8), void *priv)
{
	bool found;
	irq_flags_t flags;
	struct dlist *l;
	struct vmm_vserial_receiver *receiver;

	if (!vser || !recv) {
		return VMM_EFAIL;
	}

	receiver = NULL;
	found = FALSE;

	vmm_spin_lock_irqsave(&vser->receiver_list_lock, flags);

	list_for_each(l, &vser->receiver_list) {
		receiver = list_entry(l, struct vmm_vserial_receiver, head);
		if ((receiver->recv == recv) && (receiver->priv == priv)) {
			found = TRUE;
			break;
		}
	}

	if (!found) {
		vmm_spin_unlock_irqrestore(&vser->receiver_list_lock, flags);
		return VMM_EINVALID;
	}

	list_del(&receiver->head);

	vmm_spin_unlock_irqrestore(&vser->receiver_list_lock, flags);

	vmm_free(receiver);

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(vmm_vserial_unregister_receiver);

struct vmm_vserial *vmm_vserial_create(const char *name,
				       bool (*can_send) (struct vmm_vserial *),
				       int (*send) (struct vmm_vserial *, u8),
				       u32 receive_fifo_size, void *priv)
{
	bool found;
	struct dlist *l;
	struct vmm_vserial *vser;
	struct vmm_vserial_event event;

	if (!name) {
		return NULL;
	}

	vser = NULL;
	found = FALSE;

	vmm_mutex_lock(&vsctrl.vser_list_lock);

	list_for_each(l, &vsctrl.vser_list) {
		vser = list_entry(l, struct vmm_vserial, head);
		if (strcmp(name, vser->name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (found) {
		vmm_mutex_unlock(&vsctrl.vser_list_lock);
		return NULL;
	}

	vser = vmm_malloc(sizeof(struct vmm_vserial));
	if (!vser) {
		vmm_mutex_unlock(&vsctrl.vser_list_lock);
		return NULL;
	}

	vser->receive_fifo = fifo_alloc(1, receive_fifo_size);
	if (!(vser->receive_fifo)) {
		vmm_free(vser);
		vmm_mutex_unlock(&vsctrl.vser_list_lock);
		return NULL;
	}

	INIT_LIST_HEAD(&vser->head);
	if (strlcpy(vser->name, name, sizeof(vser->name)) >=
	    sizeof(vser->name)) {
		fifo_free(vser->receive_fifo);
		vmm_free(vser);
		vmm_mutex_unlock(&vsctrl.vser_list_lock);
		return NULL;
	}
	vser->can_send = can_send;
	vser->send = send;
	INIT_SPIN_LOCK(&vser->receiver_list_lock);
	INIT_LIST_HEAD(&vser->receiver_list);
	vser->priv = priv;

	list_add_tail(&vser->head, &vsctrl.vser_list);

	vmm_mutex_unlock(&vsctrl.vser_list_lock);

	/* Broadcast create event */
	event.vser = vser;
	event.data = NULL;
	vmm_blocking_notifier_call(&vsctrl.notifier_chain, 
				   VMM_VSERIAL_EVENT_CREATE, 
				   &event);

	return vser;
}
VMM_EXPORT_SYMBOL(vmm_vserial_create);

int vmm_vserial_destroy(struct vmm_vserial *vser)
{
	bool found;
	struct dlist *l;
	struct vmm_vserial *vs;
	struct vmm_vserial_event event;

	if (!vser) {
		return VMM_EFAIL;
	}

	/* Broadcast destroy event */
	event.vser = vser;
	event.data = NULL;
	vmm_blocking_notifier_call(&vsctrl.notifier_chain, 
				   VMM_VSERIAL_EVENT_DESTROY, 
				   &event);

	vmm_mutex_lock(&vsctrl.vser_list_lock);

	if (list_empty(&vsctrl.vser_list)) {
		vmm_mutex_unlock(&vsctrl.vser_list_lock);
		return VMM_EFAIL;
	}

	vs = NULL;
	found = FALSE;

	list_for_each(l, &vsctrl.vser_list) {
		vs = list_entry(l, struct vmm_vserial, head);
		if (strcmp(vs->name, vser->name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (!found) {
		vmm_mutex_unlock(&vsctrl.vser_list_lock);
		return VMM_ENOTAVAIL;
	}

	list_del(&vs->head);

	fifo_free(vs->receive_fifo);
	vmm_free(vs);

	vmm_mutex_unlock(&vsctrl.vser_list_lock);

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(vmm_vserial_destroy);

struct vmm_vserial *vmm_vserial_find(const char *name)
{
	bool found;
	struct dlist *l;
	struct vmm_vserial *vs;

	if (!name) {
		return NULL;
	}

	found = FALSE;
	vs = NULL;

	vmm_mutex_lock(&vsctrl.vser_list_lock);

	list_for_each(l, &vsctrl.vser_list) {
		vs = list_entry(l, struct vmm_vserial, head);
		if (strcmp(vs->name, name) == 0) {
			found = TRUE;
			break;
		}
	}

	vmm_mutex_unlock(&vsctrl.vser_list_lock);

	if (!found) {
		return NULL;
	}

	return vs;
}
VMM_EXPORT_SYMBOL(vmm_vserial_find);

struct vmm_vserial *vmm_vserial_get(int index)
{
	bool found;
	struct dlist *l;
	struct vmm_vserial *retval;

	if (index < 0) {
		return NULL;
	}

	retval = NULL;
	found = FALSE;

	vmm_mutex_lock(&vsctrl.vser_list_lock);

	list_for_each(l, &vsctrl.vser_list) {
		retval = list_entry(l, struct vmm_vserial, head);
		if (!index) {
			found = TRUE;
			break;
		}
		index--;
	}

	vmm_mutex_unlock(&vsctrl.vser_list_lock);

	if (!found) {
		return NULL;
	}

	return retval;
}
VMM_EXPORT_SYMBOL(vmm_vserial_get);

u32 vmm_vserial_count(void)
{
	u32 retval = 0;
	struct dlist *l;

	vmm_mutex_lock(&vsctrl.vser_list_lock);

	list_for_each(l, &vsctrl.vser_list) {
		retval++;
	}

	vmm_mutex_unlock(&vsctrl.vser_list_lock);

	return retval;
}
VMM_EXPORT_SYMBOL(vmm_vserial_count);

static int __init vmm_vserial_init(void)
{
	memset(&vsctrl, 0, sizeof(vsctrl));

	INIT_MUTEX(&vsctrl.vser_list_lock);
	INIT_LIST_HEAD(&vsctrl.vser_list);
	BLOCKING_INIT_NOTIFIER_CHAIN(&vsctrl.notifier_chain);

	return VMM_OK;
}

static void __exit vmm_vserial_exit(void)
{
	/* Nothing to do here. */
}

VMM_DECLARE_MODULE(MODULE_DESC, 
			MODULE_AUTHOR, 
			MODULE_LICENSE, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
