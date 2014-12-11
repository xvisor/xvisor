/*
 * Copyright (C) 2011-2013 Freescale Semiconductor, Inc. All Rights Reserved.
 * Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWide.
 * All rights reserved.
 * Modified by Jimmy Durand Wesolowski <jimmy.durand-wesolowski@openwide.fr>
 * for Xvisor.
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 *
 * @file mxc_dispdrv.c
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @brief mxc display driver framework.
 */

/*
 * A display device driver could call mxc_dispdrv_register(drv) in its dev_probe() function.
 * Move all dev_probe() things into mxc_dispdrv_driver->init(), init() function should init
 * and feedback setting;
 * Necessary deferred operations can be done in mxc_dispdrv_driver->post_init(),
 * after dev_id and disp_id pass usage check;
 * Move all dev_remove() things into mxc_dispdrv_driver->deinit();
 * Move all dev_suspend() things into fb_notifier for SUSPEND, if there is;
 * Move all dev_resume() things into fb_notifier for RESUME, if there is;
 *
 * ipuv3 fb driver could call mxc_dispdrv_gethandle(name, setting) before a fb
 * need be added, with fbi param passing by setting, after
 * mxc_dispdrv_gethandle() return, FB driver should get the basic setting
 * about fbi info and ipuv3-hw (ipu_id and disp_id).
 *
 * @ingroup Framebuffer
 */
#include <vmm_modules.h>
#include <vmm_mutex.h>
#include <vmm_heap.h>
#include <libs/list.h>

#include <mxc_dispdrv.h>

static LIST_HEAD(dispdrv_list);
static DEFINE_MUTEX(dispdrv_lock);

struct mxc_dispdrv_entry {
	/* Note: drv always the first element */
	struct mxc_dispdrv_driver *drv;
	bool active;
	void *priv;
	struct list_head list;
};

struct mxc_dispdrv_handle *mxc_dispdrv_register(struct mxc_dispdrv_driver *drv)
{
	struct mxc_dispdrv_entry *new;

	vmm_mutex_lock(&dispdrv_lock);

	new = vmm_zalloc(sizeof(struct mxc_dispdrv_entry));
	if (!new) {
		vmm_mutex_unlock(&dispdrv_lock);
		return VMM_ERR_PTR(VMM_ENOMEM);
	}

	new->drv = drv;
	list_add_tail(&new->list, &dispdrv_list);

	vmm_mutex_unlock(&dispdrv_lock);

	return (struct mxc_dispdrv_handle *)new;
}

int mxc_dispdrv_unregister(struct mxc_dispdrv_handle *handle)
{
	struct mxc_dispdrv_entry *entry = (struct mxc_dispdrv_entry *)handle;

	if (entry) {
		vmm_mutex_lock(&dispdrv_lock);
		list_del(&entry->list);
		vmm_mutex_unlock(&dispdrv_lock);
		vmm_free(entry);
		return 0;
	} else
		return VMM_EINVALID;
}

struct mxc_dispdrv_handle *mxc_dispdrv_gethandle(char *name,
	struct mxc_dispdrv_setting *setting)
{
	int ret, found = 0;
	struct mxc_dispdrv_entry *entry;

	vmm_mutex_lock(&dispdrv_lock);
	list_for_each_entry(entry, &dispdrv_list, list) {
		if (!strcmp(entry->drv->name, name) && (entry->drv->init)) {
			ret = entry->drv->init((struct mxc_dispdrv_handle *)
				entry, setting);
			if (ret >= 0) {
				entry->active = true;
				found = 1;
				break;
			}
		}
	}
	vmm_mutex_unlock(&dispdrv_lock);

	if (found)
		return (struct mxc_dispdrv_handle *)entry;
	else
		return VMM_ERR_PTR(VMM_ENODEV);
}

void mxc_dispdrv_puthandle(struct mxc_dispdrv_handle *handle)
{
	struct mxc_dispdrv_entry *entry = (struct mxc_dispdrv_entry *)handle;

	vmm_mutex_lock(&dispdrv_lock);
	if (entry && entry->active && entry->drv->deinit) {
		entry->drv->deinit(handle);
		entry->active = false;
	}
	vmm_mutex_unlock(&dispdrv_lock);

}

int mxc_dispdrv_setdata(struct mxc_dispdrv_handle *handle, void *data)
{
	struct mxc_dispdrv_entry *entry = (struct mxc_dispdrv_entry *)handle;

	if (entry) {
		entry->priv = data;
		return 0;
	} else
		return VMM_EINVALID;
}

void *mxc_dispdrv_getdata(struct mxc_dispdrv_handle *handle)
{
	struct mxc_dispdrv_entry *entry = (struct mxc_dispdrv_entry *)handle;

	if (entry) {
		return entry->priv;
	} else
		return VMM_ERR_PTR(VMM_EINVALID);
}
