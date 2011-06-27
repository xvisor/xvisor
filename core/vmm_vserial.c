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
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief source code for virtual serial port
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_string.h>
#include <vmm_vserial.h>

vmm_vserial_ctrl_t vsctrl;

u32 vmm_vserial_send(vmm_vserial_t * vser, u8 *src, u32 len)
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

u32 vmm_vserial_receive(vmm_vserial_t * vser, u8 *dst, u32 len)
{
	u32 i;

	if (!vser || !dst) {
		return 0;
	}
	if (!vser->can_recv || !vser->recv) {
		return 0;
	}

	for (i = 0; i < len ; i++) {
		if (!vser->can_recv(vser)) {
			break;
		}
		vser->recv(vser, &dst[i]);
	}

	return i;
}

int vmm_vserial_register(vmm_vserial_t * vser)
{
	bool found;
	struct dlist *l;
	vmm_vserial_t *vs;

	if (!vser) {
		return VMM_EFAIL;
	}

	vs = NULL;
	found = FALSE;
	list_for_each(l, &vsctrl.vser_list) {
		vs = list_entry(l, vmm_vserial_t, head);
		if (vmm_strcmp(vs->name, vser->name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (found) {
		return VMM_EINVALID;
	}

	INIT_LIST_HEAD(&vser->head);

	list_add_tail(&vsctrl.vser_list, &vser->head);

	return VMM_OK;
}

int vmm_vserial_unregister(vmm_vserial_t * vser)
{
	bool found;
	struct dlist *l;
	vmm_vserial_t *vs;

	if (!vser) {
		return VMM_EFAIL;
	}

	if (list_empty(&vsctrl.vser_list)) {
		return VMM_EFAIL;
	}

	vs = NULL;
	found = FALSE;
	list_for_each(l, &vsctrl.vser_list) {
		vs = list_entry(l, vmm_vserial_t, head);
		if (vmm_strcmp(vs->name, vser->name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (!found) {
		return VMM_ENOTAVAIL;
	}

	list_del(&vs->head);

	return VMM_OK;
}

vmm_vserial_t * vmm_vserial_find(const char *name)
{
	bool found;
	struct dlist *l;
	vmm_vserial_t *vs;

	if (!name) {
		return NULL;
	}

	found = FALSE;
	vs = NULL;
	list_for_each(l, &vsctrl.vser_list) {
		vs = list_entry(l, vmm_vserial_t, head);
		if (vmm_strcmp(vs->name, name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (!found) {
		return NULL;
	}

	return vs;
}

vmm_vserial_t * vmm_vserial_get(int index)
{
	bool found;
	struct dlist *l;
	vmm_vserial_t *retval;

	if (index < 0) {
		return NULL;
	}

	retval = NULL;
	found = FALSE;

	list_for_each(l, &vsctrl.vser_list) {
		retval = list_entry(l, vmm_vserial_t, head);
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

int vmm_vserial_init(void)
{
	vmm_memset(&vsctrl, 0, sizeof(vsctrl));

	INIT_LIST_HEAD(&vsctrl.vser_list);

	return VMM_OK;
}
