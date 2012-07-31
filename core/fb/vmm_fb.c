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
 * @file vmm_fb.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Frame buffer framework source
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_string.h>
#include <vmm_stdio.h>
#include <vmm_modules.h>
#include <vmm_devdrv.h>
#include <fb/vmm_fb.h>

#define MODULE_NAME			"Frame Buffer Framework"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_IPRIORITY		VMM_FB_CLASS_IPRIORITY
#define	MODULE_INIT			vmm_fb_init
#define	MODULE_EXIT			vmm_fb_exit

int vmm_fb_register(struct vmm_fb *fb)
{
	struct vmm_classdev *cd;

	if (fb == NULL) {
		return VMM_EFAIL;
	}
	if (fb->fbops == NULL) {
		return VMM_EFAIL;
	}

	cd = vmm_malloc(sizeof(struct vmm_classdev));
	if (!cd) {
		return VMM_EFAIL;
	}

	INIT_LIST_HEAD(&cd->head);
	vmm_strcpy(cd->name, fb->dev->node->name);
	cd->dev = fb->dev;
	cd->priv = fb;

	vmm_devdrv_register_classdev(VMM_FB_CLASS_NAME, cd);

	return VMM_OK;
}

int vmm_fb_unregister(struct vmm_fb *fb)
{
	int rc;
	struct vmm_classdev *cd;

	if (fb == NULL) {
		return VMM_EFAIL;
	}
	if (fb->dev == NULL) {
		return VMM_EFAIL;
	}

	cd = vmm_devdrv_find_classdev(VMM_FB_CLASS_NAME, fb->dev->node->name);
	if (!cd) {
		return VMM_EFAIL;
	}

	rc = vmm_devdrv_unregister_classdev(VMM_FB_CLASS_NAME, cd);
	if (!rc) {
		vmm_free(cd);
	}

	return rc;
}

struct vmm_fb *vmm_fb_find(const char *name)
{
	struct vmm_classdev *cd;

	cd = vmm_devdrv_find_classdev(VMM_FB_CLASS_NAME, name);

	if (!cd) {
		return NULL;
	}

	return cd->priv;
}

struct vmm_fb *vmm_fb_get(int num)
{
	struct vmm_classdev *cd;

	cd = vmm_devdrv_classdev(VMM_FB_CLASS_NAME, num);

	if (!cd) {
		return NULL;
	}

	return cd->priv;
}

u32 vmm_fb_count(void)
{
	return vmm_devdrv_classdev_count(VMM_FB_CLASS_NAME);
}

static int __init vmm_fb_init(void)
{
	int rc;
	struct vmm_class *c;

	vmm_printf("Initialize Frame Buffer Framework\n");

	c = vmm_malloc(sizeof(struct vmm_class));
	if (!c) {
		return VMM_EFAIL;
	}

	INIT_LIST_HEAD(&c->head);
	vmm_strcpy(c->name, VMM_FB_CLASS_NAME);
	INIT_LIST_HEAD(&c->classdev_list);

	rc = vmm_devdrv_register_class(c);
	if (rc) {
		vmm_free(c);
		return rc;
	}

	return VMM_OK;
}

static void __exit vmm_fb_exit(void)
{
	int rc;
	struct vmm_class *c;

	c = vmm_devdrv_find_class(VMM_FB_CLASS_NAME);
	if (!c) {
		return;
	}

	rc = vmm_devdrv_unregister_class(c);
	if (rc) {
		return;
	}

	vmm_free(c);
}

VMM_DECLARE_MODULE(MODULE_NAME,
			MODULE_AUTHOR,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
