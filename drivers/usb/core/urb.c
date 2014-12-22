/**
 * Copyright (c) 2013 Anup Patel.
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
 * @file urb.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of urb managment APIs
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_stdio.h>
#include <vmm_modules.h>
#include <arch_atomic.h>
#include <drv/usb.h>
#include <drv/usb/hcd.h>

void usb_init_urb(struct urb *urb)
{
	if (urb) {
		memset(urb, 0, sizeof(*urb));
		arch_atomic_write(&urb->refcnt, 1);
		INIT_LIST_HEAD(&urb->urb_list);
	}
}
VMM_EXPORT_SYMBOL(usb_init_urb);

struct urb *usb_alloc_urb(void)
{
	struct urb *urb;

	urb = vmm_malloc(sizeof(struct urb));
	if (!urb) {
		vmm_printf("%s: vmm_malloc() failed\n", __func__);
		return NULL;
	}

	usb_init_urb(urb);

	return urb;
}
VMM_EXPORT_SYMBOL(usb_alloc_urb);

void usb_ref_urb(struct urb *urb)
{
	if (urb) {
		arch_atomic_add(&urb->refcnt, 1);
	}
}
VMM_EXPORT_SYMBOL(usb_ref_urb);

void usb_free_urb(struct urb *urb)
{
	if (urb) {
		if (arch_atomic_sub_return(&urb->refcnt, 1)) {
			return;
		}

		vmm_free(urb);
	}
}
VMM_EXPORT_SYMBOL(usb_free_urb);

int usb_submit_urb(struct urb *urb)
{
	if (!urb || urb->hcpriv)
		return VMM_EINVALID;

	urb->status = VMM_EBUSY;
	urb->actual_length = 0;

	return usb_hcd_submit_urb(urb);
}
VMM_EXPORT_SYMBOL(usb_submit_urb);

int usb_unlink_urb(struct urb *urb, int status)
{
	if (!urb)
		return VMM_EINVALID;
	if (!urb->dev)
		return VMM_ENODEV;
	return usb_hcd_unlink_urb(urb, VMM_EFAIL);
}
VMM_EXPORT_SYMBOL(usb_unlink_urb);

