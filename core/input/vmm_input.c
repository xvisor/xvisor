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
 * @file vmm_input.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Input device framework source
 *
 * The source has been largely adapted from Linux 3.x or higher:
 * drivers/input/input.c
 *
 * Copyright (c) 1999-2002 Vojtech Pavlik
 *
 * The original code is licensed under the GPL.
 */

#include <vmm_error.h>
#include <vmm_string.h>
#include <vmm_heap.h>
#include <vmm_stdio.h>
#include <vmm_modules.h>
#include <input/vmm_input.h>

#define MODULE_VARID			input_framework_module
#define MODULE_NAME			"Input Device Framework"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_IPRIORITY		VMM_INPUT_IPRIORITY
#define	MODULE_INIT			vmm_input_init
#define	MODULE_EXIT			vmm_input_exit

/* FIXME: */
void vmm_input_event(struct vmm_input_dev *idev, 
		     unsigned int type, unsigned int code, int value)
{
}

/* FIXME: */
void vmm_input_set_capability(struct vmm_input_dev *idev, 
			      unsigned int type, unsigned int code)
{
}

/* FIXME: */
int vmm_input_scancode_to_scalar(const struct vmm_input_keymap_entry *ke,
				 unsigned int *scancode)
{
	return VMM_OK;
}

/* FIXME: */
int vmm_input_get_keycode(struct vmm_input_dev *idev, 
			  struct vmm_input_keymap_entry *ke)
{
	return VMM_OK;
}

/* FIXME: */
int vmm_input_set_keycode(struct vmm_input_dev *idev,
			  const struct vmm_input_keymap_entry *ke)
{
	return VMM_OK;
}

/* FIXME: */
struct vmm_input_dev *vmm_input_alloc_device(void)
{
	struct vmm_input_dev *idev;

	idev = vmm_malloc(sizeof(struct vmm_input_dev));
	if (!idev) {
		return NULL;
	}

	vmm_memset(idev, 0, sizeof(struct vmm_input_dev));
	INIT_SPIN_LOCK(&idev->event_lock);

	return idev;
}

/* FIXME: */
void vmm_input_free_device(struct vmm_input_dev *idev)
{
	if (!idev) {
		vmm_free(idev);
	}
}

/* FIXME: */
int vmm_input_register(struct vmm_input_dev *idev)
{
	int rc;
	struct vmm_classdev *cd;

	if (!(idev && 
	      idev->name && 
	      idev->open && 
	      idev->close && 
	      idev->flush)) {
		return VMM_EFAIL;
	}

	cd = vmm_malloc(sizeof(struct vmm_classdev));
	if (!cd) {
		return VMM_EFAIL;
	}

	vmm_memset(cd, 0, sizeof(struct vmm_classdev));

	INIT_LIST_HEAD(&cd->head);
	vmm_strcpy(cd->name, idev->name);
	cd->dev = idev->dev;
	cd->priv = idev;

	rc = vmm_devdrv_register_classdev(VMM_INPUT_DEV_CLASS_NAME, cd);
	if (rc != VMM_OK) {
		vmm_free(cd);
	}

	return rc;
}

/* FIXME: */
int vmm_input_unregister_device(struct vmm_input_dev *idev)
{
	int rc;
	struct vmm_classdev *cd;

	if (!idev) {
		return VMM_EFAIL;
	}

	cd = vmm_devdrv_find_classdev(VMM_INPUT_DEV_CLASS_NAME, idev->name);
	if (!cd) {
		return VMM_EFAIL;
	}

	rc = vmm_devdrv_unregister_classdev(VMM_INPUT_DEV_CLASS_NAME, cd);
	if (rc == VMM_OK) {
		vmm_free(cd);
	}

	return rc;
}

/* FIXME: */
void vmm_input_reset_device(struct vmm_input_dev *idev)
{
}

/* FIXME: */
int vmm_input_flush_device(struct vmm_input_dev *idev)
{
	return VMM_OK;
}

struct vmm_input_dev *vmm_input_find_device(const char *name)
{
	struct vmm_classdev *cd;

	cd = vmm_devdrv_find_classdev(VMM_INPUT_DEV_CLASS_NAME, name);
	if (!cd) {
		return NULL;
	}

	return cd->priv;
}

struct vmm_input_dev *vmm_input_get_device(int num)
{
	struct vmm_classdev *cd;

	cd = vmm_devdrv_classdev(VMM_INPUT_DEV_CLASS_NAME, num);
	if (!cd) {
		return NULL;
	}

	return cd->priv;
}

u32 vmm_input_count_device(void)
{
	return vmm_devdrv_classdev_count(VMM_INPUT_DEV_CLASS_NAME);
}

/* FIXME: */
struct vmm_input_handler *vmm_input_alloc_handler(void)
{
	return NULL;
}

/* FIXME: */
void vmm_input_free_handler(struct vmm_input_handler *ihnd)
{
}

/* FIXME: */
int vmm_input_register_handler(struct vmm_input_handler *ihnd)
{
	return VMM_OK;
}

/* FIXME: */
int vmm_input_unregister_handler(struct vmm_input_handler *ihnd)
{
	return VMM_OK;
}

/* FIXME: */
int vmm_input_connect_handler(struct vmm_input_handler *ihnd)
{
	return VMM_OK;
}

/* FIXME: */
int vmm_input_disconnect_handler(struct vmm_input_handler *ihnd)
{
	return VMM_OK;
}

/* FIXME: */
struct vmm_input_handler *vmm_input_find_handler(const char *name)
{
	return NULL;
}

/* FIXME: */
struct vmm_input_handler *vmm_input_get_handler(int num)
{
	return NULL;
}

/* FIXME: */
u32 vmm_input_count_handler(void)
{
	return 0;
}

static int __init vmm_input_init(void)
{
	int rc;
	struct vmm_class *c;

	vmm_printf("Initialize Input Device Framework\n");

	c = vmm_malloc(sizeof(struct vmm_class));
	if (!c) {
		return VMM_EFAIL;
	}

	vmm_memset(c, 0, sizeof(struct vmm_class));

	INIT_LIST_HEAD(&c->head);
	vmm_strcpy(c->name, VMM_INPUT_DEV_CLASS_NAME);
	INIT_LIST_HEAD(&c->classdev_list);

	rc = vmm_devdrv_register_class(c);
	if (rc != VMM_OK) {
		vmm_free(c);
	}

	return rc;
}

static void vmm_input_exit(void)
{
	int rc;
	struct vmm_class *c;

	c = vmm_devdrv_find_class(VMM_INPUT_DEV_CLASS_NAME);
	if (!c) {
		return;
	}

	rc = vmm_devdrv_unregister_class(c);
	if (rc) {
		return;
	}

	vmm_free(c);
}

VMM_DECLARE_MODULE(MODULE_VARID,
		   MODULE_NAME,
		   MODULE_AUTHOR,
		   MODULE_IPRIORITY,
		   MODULE_INIT,
		   MODULE_EXIT);
