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
 * @file vmm_vinput.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief source file for virtual input subsystem
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_mutex.h>
#include <vmm_modules.h>
#include <vio/vmm_vinput.h>
#include <libs/stringlib.h>

#define MODULE_DESC			"Virtual Input Framework"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(VMM_VINPUT_IPRIORITY)
#define	MODULE_INIT			vmm_vinput_init
#define	MODULE_EXIT			vmm_vinput_exit

struct vmm_vinput_ctrl {
	struct vmm_mutex vkbd_list_lock;
        struct dlist vkbd_list;
	struct vmm_mutex vmou_list_lock;
        struct dlist vmou_list;
	struct vmm_blocking_notifier_chain notifier_chain;
};

static struct vmm_vinput_ctrl victrl;

int vmm_vinput_register_client(struct vmm_notifier_block *nb)
{
	return vmm_blocking_notifier_register(&victrl.notifier_chain, nb);
}
VMM_EXPORT_SYMBOL(vmm_vinput_register_client);

int vmm_vinput_unregister_client(struct vmm_notifier_block *nb)
{
	return vmm_blocking_notifier_unregister(&victrl.notifier_chain, nb);
}
VMM_EXPORT_SYMBOL(vmm_vinput_unregister_client);

struct vmm_vkeyboard *vmm_vkeyboard_create(const char *name,
			void (*kbd_event) (struct vmm_vkeyboard *, int),
			void *priv)
{
	bool found;
	struct vmm_vkeyboard *vkbd;
	struct vmm_vinput_event event;

	if (!name) {
		return NULL;
	}

	vkbd = NULL;
	found = FALSE;

	vmm_mutex_lock(&victrl.vkbd_list_lock);

	list_for_each_entry(vkbd, &victrl.vkbd_list, head) {
		if (strcmp(name, vkbd->name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (found) {
		vmm_mutex_unlock(&victrl.vkbd_list_lock);
		return NULL;
	}

	vkbd = vmm_malloc(sizeof(struct vmm_vkeyboard));
	if (!vkbd) {
		vmm_mutex_unlock(&victrl.vkbd_list_lock);
		return NULL;
	}

	INIT_LIST_HEAD(&vkbd->head);
	if (strlcpy(vkbd->name, name, sizeof(vkbd->name)) >=
	    sizeof(vkbd->name)) {
		vmm_free(vkbd);
		vmm_mutex_unlock(&victrl.vkbd_list_lock);
		return NULL;
	}
	INIT_SPIN_LOCK(&vkbd->ledstate_lock);
	vkbd->ledstate = 0;
	INIT_LIST_HEAD(&vkbd->led_handler_list);
	vkbd->kbd_event = kbd_event;
	vkbd->priv = priv;

	list_add_tail(&vkbd->head, &victrl.vkbd_list);

	vmm_mutex_unlock(&victrl.vkbd_list_lock);

	/* Broadcast create event */
	event.data = vkbd;
	vmm_blocking_notifier_call(&victrl.notifier_chain, 
				   VMM_VINPUT_EVENT_CREATE_KEYBOARD, 
				   &event);

	return vkbd;
}
VMM_EXPORT_SYMBOL(vmm_vkeyboard_create);

int vmm_vkeyboard_destroy(struct vmm_vkeyboard *vkbd)
{
	bool found;
	irq_flags_t flags;
	struct vmm_vkeyboard *vk;
	struct vmm_vkeyboard_led_handler *vklh;
	struct vmm_vinput_event event;

	if (!vkbd) {
		return VMM_EFAIL;
	}

	/* Broadcast destroy event */
	event.data = vkbd;
	vmm_blocking_notifier_call(&victrl.notifier_chain, 
				   VMM_VINPUT_EVENT_DESTROY_KEYBOARD, 
				   &event);

	vmm_spin_lock_irqsave(&vkbd->ledstate_lock, flags);
	while (!list_empty(&vkbd->led_handler_list)) {
		vklh = list_first_entry(&vkbd->led_handler_list,
				struct vmm_vkeyboard_led_handler, head);
		list_del(&vklh->head);
		vmm_free(vklh);
	}
	vmm_spin_unlock_irqrestore(&vkbd->ledstate_lock, flags);

	vmm_mutex_lock(&victrl.vkbd_list_lock);

	if (list_empty(&victrl.vkbd_list)) {
		vmm_mutex_unlock(&victrl.vkbd_list_lock);
		return VMM_EFAIL;
	}

	vk = NULL;
	found = FALSE;
	list_for_each_entry(vk, &victrl.vkbd_list, head) {
		if (strcmp(vk->name, vkbd->name) == 0) {
			found = TRUE;
			break;
		}
	}
	if (!found) {
		vmm_mutex_unlock(&victrl.vkbd_list_lock);
		return VMM_ENOTAVAIL;
	}

	list_del(&vk->head);
	vmm_free(vk);

	vmm_mutex_unlock(&victrl.vkbd_list_lock);

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(vmm_vkeyboard_destroy);

int vmm_vkeyboard_event(struct vmm_vkeyboard *vkbd, int keycode)
{
	if (!vkbd || !vkbd->kbd_event) {
		return VMM_EINVALID;
	}

	vkbd->kbd_event(vkbd, keycode);

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(vmm_vkeyboard_event);

int vmm_vkeyboard_add_led_handler(struct vmm_vkeyboard *vkbd,
		void (*led_change) (struct vmm_vkeyboard *, int, void *),
		void *priv)
{
	bool found;
	irq_flags_t flags;
	struct vmm_vkeyboard_led_handler *vklh;

	if (!vkbd || !led_change) {
		return VMM_EINVALID;
	}

	vmm_spin_lock_irqsave(&vkbd->ledstate_lock, flags);

	vklh = NULL;
	found = FALSE;
	list_for_each_entry(vklh, &vkbd->led_handler_list, head) {
		if ((vklh->led_change == led_change) &&
		    (vklh->priv == priv)) {
			found = TRUE;
			break;
		}
	}
	if (found) {
		vmm_spin_unlock_irqrestore(&vkbd->ledstate_lock, flags);
		return VMM_EEXIST;
	}

	vklh = vmm_zalloc(sizeof(struct vmm_vkeyboard_led_handler));
	if (!vklh) {
		vmm_spin_unlock_irqrestore(&vkbd->ledstate_lock, flags);
		return VMM_ENOMEM;
	}

	INIT_LIST_HEAD(&vklh->head);
	vklh->led_change = led_change;
	vklh->priv = priv;
	list_add_tail(&vklh->head, &vkbd->led_handler_list);

	vmm_spin_unlock_irqrestore(&vkbd->ledstate_lock, flags);

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(vmm_vkeyboard_add_led_handler);

int vmm_vkeyboard_del_led_handler(struct vmm_vkeyboard *vkbd,
		void (*led_change) (struct vmm_vkeyboard *, int, void *),
		void *priv)
{
	bool found;
	irq_flags_t flags;
	struct vmm_vkeyboard_led_handler *vklh;

	if (!vkbd || !led_change) {
		return VMM_EINVALID;
	}

	vmm_spin_lock_irqsave(&vkbd->ledstate_lock, flags);

	vklh = NULL;
	found = FALSE;
	list_for_each_entry(vklh, &vkbd->led_handler_list, head) {
		if ((vklh->led_change == led_change) &&
		    (vklh->priv == priv)) {
			found = TRUE;
			break;
		}
	}
	if (!found) {
		vmm_spin_unlock_irqrestore(&vkbd->ledstate_lock, flags);
		return VMM_ENOTAVAIL;
	}

	list_del(&vklh->head);
	vmm_free(vklh);

	vmm_spin_unlock_irqrestore(&vkbd->ledstate_lock, flags);

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(vmm_vkeyboard_del_led_handler);

void vmm_vkeyboard_set_ledstate(struct vmm_vkeyboard *vkbd, int ledstate)
{
	irq_flags_t flags;
	struct vmm_vkeyboard_led_handler *vklh;

	if (!vkbd) {
		return;
	}

	vmm_spin_lock_irqsave(&vkbd->ledstate_lock, flags);
	vkbd->ledstate = ledstate;
	list_for_each_entry(vklh, &vkbd->led_handler_list, head) {
		vklh->led_change(vkbd, ledstate, vklh->priv);
	}
	vmm_spin_unlock_irqrestore(&vkbd->ledstate_lock, flags);
}
VMM_EXPORT_SYMBOL(vmm_vkeyboard_set_ledstate);

int vmm_vkeyboard_get_ledstate(struct vmm_vkeyboard *vkbd)
{
	int ret;
	irq_flags_t flags;

	if (!vkbd) {
		return 0;
	}

	vmm_spin_lock_irqsave(&vkbd->ledstate_lock, flags);
	ret = vkbd->ledstate;
	vmm_spin_unlock_irqrestore(&vkbd->ledstate_lock, flags);

	return ret;
}
VMM_EXPORT_SYMBOL(vmm_vkeyboard_get_ledstate);

struct vmm_vkeyboard *vmm_vkeyboard_find(const char *name)
{
	bool found;
	struct vmm_vkeyboard *vk;

	if (!name) {
		return NULL;
	}

	found = FALSE;
	vk = NULL;

	vmm_mutex_lock(&victrl.vkbd_list_lock);

	list_for_each_entry(vk, &victrl.vkbd_list, head) {
		if (strcmp(vk->name, name) == 0) {
			found = TRUE;
			break;
		}
	}

	vmm_mutex_unlock(&victrl.vkbd_list_lock);

	if (!found) {
		return NULL;
	}

	return vk;
}
VMM_EXPORT_SYMBOL(vmm_vkeyboard_find);

int vmm_vkeyboard_iterate(struct vmm_vkeyboard *start, void *data,
			  int (*fn)(struct vmm_vkeyboard *vk, void *data))
{
	int rc = VMM_OK;
	bool start_found = (start) ? FALSE : TRUE;
	struct vmm_vkeyboard *vk = NULL;

	if (!fn) {
		return VMM_EINVALID;
	}

	vmm_mutex_lock(&victrl.vkbd_list_lock);

	list_for_each_entry(vk, &victrl.vkbd_list, head) {
		if (!start_found) {
			if (start && start == vk) {
				start_found = TRUE;
			} else {
				continue;
			}
		}

		rc = fn(vk, data);
		if (rc) {
			break;
		}
	}

	vmm_mutex_unlock(&victrl.vkbd_list_lock);

	return rc;
}
VMM_EXPORT_SYMBOL(vmm_vkeyboard_iterate);

u32 vmm_vkeyboard_count(void)
{
	u32 retval = 0;
	struct vmm_vkeyboard *vk;

	vmm_mutex_lock(&victrl.vkbd_list_lock);

	list_for_each_entry(vk, &victrl.vkbd_list, head) {
		retval++;
	}

	vmm_mutex_unlock(&victrl.vkbd_list_lock);

	return retval;
}
VMM_EXPORT_SYMBOL(vmm_vkeyboard_count);

struct vmm_vmouse *vmm_vmouse_create(const char *name,
				     bool absolute,
				     void (*mouse_event) (
						   struct vmm_vmouse *vmou, 
						   int dx, int dy, int dz,
						   int buttons_state),
				     void *priv)
{
	bool found;
	struct vmm_vmouse *vmou;
	struct vmm_vinput_event event;

	if (!name) {
		return NULL;
	}

	vmou = NULL;
	found = FALSE;

	vmm_mutex_lock(&victrl.vmou_list_lock);

	list_for_each_entry(vmou, &victrl.vmou_list, head) {
		if (strcmp(name, vmou->name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (found) {
		vmm_mutex_unlock(&victrl.vmou_list_lock);
		return NULL;
	}

	vmou = vmm_malloc(sizeof(struct vmm_vmouse));
	if (!vmou) {
		vmm_mutex_unlock(&victrl.vmou_list_lock);
		return NULL;
	}

	INIT_LIST_HEAD(&vmou->head);
	if (strlcpy(vmou->name, name, sizeof(vmou->name)) >=
	    sizeof(vmou->name)) {
		vmm_free(vmou);
		vmm_mutex_unlock(&victrl.vmou_list_lock);
		return NULL;
	}
	vmou->absolute = absolute;
	vmou->graphics_width = 0;
	vmou->graphics_height = 0;
	vmou->graphics_rotation = 0;
	vmou->mouse_event = mouse_event;
	vmou->priv = priv;

	list_add_tail(&vmou->head, &victrl.vmou_list);

	vmm_mutex_unlock(&victrl.vmou_list_lock);

	/* Broadcast create event */
	event.data = vmou;
	vmm_blocking_notifier_call(&victrl.notifier_chain, 
				   VMM_VINPUT_EVENT_CREATE_MOUSE, 
				   &event);

	return vmou;
}
VMM_EXPORT_SYMBOL(vmm_vmouse_create);

int vmm_vmouse_destroy(struct vmm_vmouse *vmou)
{
	bool found;
	struct vmm_vmouse *vm;
	struct vmm_vinput_event event;

	if (!vmou) {
		return VMM_EFAIL;
	}

	/* Broadcast destroy event */
	event.data = vmou;
	vmm_blocking_notifier_call(&victrl.notifier_chain, 
				   VMM_VINPUT_EVENT_DESTROY_MOUSE, 
				   &event);

	vmm_mutex_lock(&victrl.vmou_list_lock);

	if (list_empty(&victrl.vmou_list)) {
		vmm_mutex_unlock(&victrl.vmou_list_lock);
		return VMM_EFAIL;
	}

	vm = NULL;
	found = FALSE;
	list_for_each_entry(vm, &victrl.vmou_list, head) {
		if (strcmp(vm->name, vmou->name) == 0) {
			found = TRUE;
			break;
		}
	}
	if (!found) {
		vmm_mutex_unlock(&victrl.vmou_list_lock);
		return VMM_ENOTAVAIL;
	}

	list_del(&vm->head);
	vmm_free(vm);

	vmm_mutex_unlock(&victrl.vmou_list_lock);

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(vmm_vmouse_destroy);

int vmm_vmouse_event(struct vmm_vmouse *vmou,
		     int dx, int dy, int dz, int buttons_state)
{
	int w, h;

	if (!vmou) {
		return VMM_EINVALID;
	}
	if (!vmou->mouse_event) {
		return VMM_OK;
	}

	if (vmou->absolute) {
		w = 0x7fff;
		h = 0x7fff;
	} else {
		w = (int)vmou->graphics_width - 1;
		h = (int)vmou->graphics_height - 1;
	}

	switch (vmou->graphics_rotation) {
	case 0:
		vmou->mouse_event(vmou, dx, dy, dz, buttons_state);
		break;
	case 90:
		vmou->mouse_event(vmou, w - dy, dx, dz, buttons_state);
		break;
	case 180:
		vmou->mouse_event(vmou, w - dx, h - dy, dz, buttons_state);
		break;
	case 270:
		vmou->mouse_event(vmou, dy, h - dx, dz, buttons_state);
		break;
	};

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(vmm_vmouse_event);

bool vmm_vmouse_is_absolute(struct vmm_vmouse *vmou)
{
	return (vmou) ? vmou->absolute : TRUE;
}
VMM_EXPORT_SYMBOL(vmm_vmouse_is_absolute);

void vmm_vmouse_set_graphics_width(struct vmm_vmouse *vmou, u32 width)
{
	if (vmou) {
		vmou->graphics_width = width;
	}
}
VMM_EXPORT_SYMBOL(vmm_vmouse_set_graphics_width);

u32 vmm_vmouse_get_graphics_width(struct vmm_vmouse *vmou)
{
	return (vmou) ? vmou->graphics_width : 0;
}
VMM_EXPORT_SYMBOL(vmm_vmouse_get_graphics_width);

void vmm_vmouse_set_graphics_height(struct vmm_vmouse *vmou, u32 height)
{
	if (vmou) {
		vmou->graphics_height = height;
	}
}
VMM_EXPORT_SYMBOL(vmm_vmouse_set_graphics_height);

u32 vmm_vmouse_get_graphics_height(struct vmm_vmouse *vmou)
{
	return (vmou) ? vmou->graphics_height : 0;
}
VMM_EXPORT_SYMBOL(vmm_vmouse_get_graphics_height);

void vmm_vmouse_set_graphics_rotation(struct vmm_vmouse *vmou, u32 rotation)
{
	if (vmou && 
	    ((rotation == 0) || (rotation == 90) ||
	     (rotation == 180) || (rotation == 270))) {
		vmou->graphics_rotation = rotation;
	}
}
VMM_EXPORT_SYMBOL(vmm_vmouse_set_graphics_rotation);

u32 vmm_vmouse_get_graphics_rotation(struct vmm_vmouse *vmou)
{
	return (vmou) ? vmou->graphics_rotation : 0;
}
VMM_EXPORT_SYMBOL(vmm_vmouse_get_graphics_rotation);

struct vmm_vmouse *vmm_vmouse_find(const char *name)
{
	bool found;
	struct vmm_vmouse *vm;

	if (!name) {
		return NULL;
	}

	found = FALSE;
	vm = NULL;

	vmm_mutex_lock(&victrl.vmou_list_lock);

	list_for_each_entry(vm, &victrl.vmou_list, head) {
		if (strcmp(vm->name, name) == 0) {
			found = TRUE;
			break;
		}
	}

	vmm_mutex_unlock(&victrl.vmou_list_lock);

	if (!found) {
		return NULL;
	}

	return vm;
}
VMM_EXPORT_SYMBOL(vmm_vmouse_find);

struct vmm_vmouse *vmm_vmouse_get(int index)
{
	bool found;
	struct vmm_vmouse *vm;

	if (index < 0) {
		return NULL;
	}

	vm = NULL;
	found = FALSE;

	vmm_mutex_lock(&victrl.vmou_list_lock);

	list_for_each_entry(vm, &victrl.vmou_list, head) {
		if (!index) {
			found = TRUE;
			break;
		}
		index--;
	}

	vmm_mutex_unlock(&victrl.vmou_list_lock);

	if (!found) {
		return NULL;
	}

	return vm;
}
VMM_EXPORT_SYMBOL(vmm_vmouse_get);

u32 vmm_vmouse_count(void)
{
	u32 retval = 0;
	struct vmm_vmouse *vm;

	vmm_mutex_lock(&victrl.vmou_list_lock);

	list_for_each_entry(vm, &victrl.vmou_list, head) {
		retval++;
	}

	vmm_mutex_unlock(&victrl.vmou_list_lock);

	return retval;
}
VMM_EXPORT_SYMBOL(vmm_vmouse_count);

static int __init vmm_vinput_init(void)
{
	memset(&victrl, 0, sizeof(victrl));

	INIT_MUTEX(&victrl.vkbd_list_lock);
	INIT_LIST_HEAD(&victrl.vkbd_list);
	INIT_MUTEX(&victrl.vmou_list_lock);
	INIT_LIST_HEAD(&victrl.vmou_list);
	BLOCKING_INIT_NOTIFIER_CHAIN(&victrl.notifier_chain);

	return VMM_OK;
}

static void __exit vmm_vinput_exit(void)
{
	/* Nothing to do here. */
}

VMM_DECLARE_MODULE(MODULE_DESC, 
			MODULE_AUTHOR, 
			MODULE_LICENSE, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);

