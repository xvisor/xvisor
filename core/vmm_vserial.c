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
#include <vmm_vserial.h>
#include <stringlib.h>

struct vmm_vserial_ctrl {
        struct dlist vser_list;
};

static struct vmm_vserial_ctrl vsctrl;

u32 vmm_vserial_send(struct vmm_vserial * vser, u8 *src, u32 len)
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

u32 vmm_vserial_receive(struct vmm_vserial * vser, u8 *dst, u32 len)
{
	u32 i;
	struct dlist * l;
	struct vmm_vserial_receiver * receiver;

	if (!vser || !dst) {
		return 0;
	}

	if (list_empty(&vser->receiver_list)) {
		for (i = 0; i < len ; i++) {
			vmm_ringbuf_enqueue(vser->receive_buf, &dst[i], TRUE);
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

	return i;
}

int vmm_vserial_register_receiver(struct vmm_vserial * vser, 
				  vmm_vserial_recv_t recv, 
				  void * priv)
{
	u8 chval;
	bool found;
	struct dlist *l;
	struct vmm_vserial_receiver * receiver;

	if (!vser || !recv) {
		return VMM_EFAIL;
	}

	receiver = NULL;
	found = FALSE;
	list_for_each(l, &vser->receiver_list) {
		receiver = list_entry(l, struct vmm_vserial_receiver, head);
		if ((receiver->recv == recv) && (receiver->priv == priv)) {
			found = TRUE;
			break;
		}
	}

	if (found) {
		return VMM_EINVALID;
	}

	receiver = vmm_malloc(sizeof(struct vmm_vserial_receiver));
	if (!receiver) {
		return VMM_EFAIL;
	}

	INIT_LIST_HEAD(&receiver->head);
	receiver->recv = recv;
	receiver->priv = priv;

	list_add_tail(&receiver->head, &vser->receiver_list);

	while (!vmm_ringbuf_isempty(vser->receive_buf)) {
		if (!vmm_ringbuf_dequeue(vser->receive_buf, &chval)) {
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

int vmm_vserial_unregister_receiver(struct vmm_vserial * vser, 
				    vmm_vserial_recv_t recv, 
				    void * priv)
{
	bool found;
	struct dlist *l;
	struct vmm_vserial_receiver * receiver;

	if (!vser || !recv) {
		return VMM_EFAIL;
	}

	receiver = NULL;
	found = FALSE;
	list_for_each(l, &vser->receiver_list) {
		receiver = list_entry(l, struct vmm_vserial_receiver, head);
		if ((receiver->recv == recv) && (receiver->priv == priv)) {
			found = TRUE;
			break;
		}
	}

	if (!found) {
		return VMM_EINVALID;
	}

	list_del(&receiver->head);

	vmm_free(receiver);

	return VMM_OK;
}

struct vmm_vserial * vmm_vserial_alloc(const char * name,
					vmm_vserial_can_send_t can_send,
					vmm_vserial_send_t send,
					u32 receive_buf_size,
					void * priv)
{
	bool found;
	struct dlist *l;
	struct vmm_vserial *vser;

	if (!name) {
		return NULL;
	}

	vser = NULL;
	found = FALSE;
	list_for_each(l, &vsctrl.vser_list) {
		vser = list_entry(l, struct vmm_vserial, head);
		if (strcmp(name, vser->name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (found) {
		return NULL;
	}

	vser = vmm_malloc(sizeof(struct vmm_vserial));
	if (!vser) {
		return NULL;
	}

	vser->receive_buf = vmm_ringbuf_alloc(1, receive_buf_size);
	if (!(vser->receive_buf)) {
		vmm_free(vser);
		return NULL;
	}

	INIT_LIST_HEAD(&vser->head);
	strcpy(vser->name, name);
	vser->can_send = can_send;
	vser->send = send;
	INIT_LIST_HEAD(&vser->receiver_list);
	vser->priv = priv;

	list_add_tail(&vser->head, &vsctrl.vser_list);

	return vser;
}

int vmm_vserial_free(struct vmm_vserial * vser)
{
	bool found;
	struct dlist *l;
	struct vmm_vserial *vs;

	if (!vser) {
		return VMM_EFAIL;
	}

	if (list_empty(&vsctrl.vser_list)) {
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
		return VMM_ENOTAVAIL;
	}

	list_del(&vs->head);

	vmm_ringbuf_free(vs->receive_buf);
	vmm_free(vs);

	return VMM_OK;
}

struct vmm_vserial * vmm_vserial_find(const char *name)
{
	bool found;
	struct dlist *l;
	struct vmm_vserial *vs;

	if (!name) {
		return NULL;
	}

	found = FALSE;
	vs = NULL;
	list_for_each(l, &vsctrl.vser_list) {
		vs = list_entry(l, struct vmm_vserial, head);
		if (strcmp(vs->name, name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (!found) {
		return NULL;
	}

	return vs;
}

struct vmm_vserial * vmm_vserial_get(int index)
{
	bool found;
	struct dlist *l;
	struct vmm_vserial *retval;

	if (index < 0) {
		return NULL;
	}

	retval = NULL;
	found = FALSE;

	list_for_each(l, &vsctrl.vser_list) {
		retval = list_entry(l, struct vmm_vserial, head);
		if (!index) {
			found = TRUE;
			break;
		}
		index--;
	}

	if (!found) {
		return NULL;
	}

	return retval;
}

u32 vmm_vserial_count(void)
{
	u32 retval = 0;
	struct dlist *l;

	list_for_each(l, &vsctrl.vser_list) {
		retval++;
	}

	return retval;
}

int __init vmm_vserial_init(void)
{
	memset(&vsctrl, 0, sizeof(vsctrl));

	INIT_LIST_HEAD(&vsctrl.vser_list);

	return VMM_OK;
}
