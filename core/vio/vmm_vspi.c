/**
 * Copyright (c) 2017 Anup Patel.
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
 * @file vmm_vspi.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief source code for virtual spi framework
 */

#include <vmm_error.h>
#include <vmm_compiler.h>
#include <vmm_heap.h>
#include <vmm_threads.h>
#include <vmm_devemu.h>
#include <vmm_modules.h>
#include <vio/vmm_vspi.h>
#include <libs/stringlib.h>

#define MODULE_DESC			"Virtual SPI Framework"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(VMM_VSPI_IPRIORITY)
#define	MODULE_INIT			vmm_vspi_init
#define	MODULE_EXIT			vmm_vspi_exit

struct vmm_vspi_ctrl {
	struct vmm_mutex vsh_list_lock;
	struct dlist vsh_list;
};

static struct vmm_vspi_ctrl vsctrl;

struct vmm_vspihost *vmm_vspislave_get_host(struct vmm_vspislave *vss)
{
	return (vss) ? vss->vsh : NULL;
}
VMM_EXPORT_SYMBOL(vmm_vspislave_get_host);

const char *vmm_vspislave_get_name(struct vmm_vspislave *vss)
{
	return (vss) ? vss->name : NULL;
}
VMM_EXPORT_SYMBOL(vmm_vspislave_get_name);

u32 vmm_vspislave_get_chip_select(struct vmm_vspislave *vss)
{
	return (vss) ? vss->chip_select : U32_MAX;
}
VMM_EXPORT_SYMBOL(vmm_vspislave_get_chip_select);

struct vmm_vspislave *vmm_vspislave_create(struct vmm_emudev *edev,
			u32 chip_select,
			u32 (*xfer) (struct vmm_vspislave *, u32, void *),
			void *priv)
{
	struct vmm_vspihost *vsh;
	struct vmm_vspislave *vss = NULL;

	if (!edev || !xfer) {
		return NULL;
	}

	vsh = vmm_vspihost_find(edev->parent);
	if (!vsh) {
		return NULL;
	}

	if (vsh->chip_select_count <= chip_select) {
		return NULL;
	}

	vmm_mutex_lock(&vsh->slaves_lock);

	if (vsh->slaves[chip_select]) {
		vmm_mutex_unlock(&vsh->slaves_lock);
		return NULL;
	}

	vss = vmm_zalloc(sizeof(*vss));
	if (!vss) {
		vmm_mutex_unlock(&vsh->slaves_lock);
		return NULL;
	}
	vss->edev = edev;
	vss->vsh = vsh;
	vss->name[0] = '\0';
	vss->chip_select = chip_select;
	vss->xfer = xfer;
	vss->priv = priv;

	strlcpy(vss->name, vsh->name, sizeof(vss->name));
	strlcat(vss->name, "/", sizeof(vss->name));
	if (strlcat(vss->name, edev->node->name, sizeof(vss->name)) >=
	    sizeof(vss->name)) {
		vmm_free(vss);
		vmm_mutex_unlock(&vsh->slaves_lock);
		return NULL;
	}

	vsh->slaves[vss->chip_select] = vss;

	vmm_mutex_unlock(&vsh->slaves_lock);

	return vss;
}
VMM_EXPORT_SYMBOL(vmm_vspislave_create);

int vmm_vspislave_destroy(struct vmm_vspislave *vss)
{
	struct vmm_vspihost *vsh;

	if (!vss || !vss->vsh) {
		return VMM_EINVALID;
	}
	vsh = vss->vsh;

	vmm_mutex_lock(&vsh->slaves_lock);

	vsh->slaves[vss->chip_select] = NULL;
	vmm_free(vss);

	vmm_mutex_unlock(&vsh->slaves_lock);

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(vmm_vspislave_destroy);

u32 vmm_vspihost_xfer_data(struct vmm_vspihost *vsh,
			   u32 chip_select, u32 data)
{
	u32 ret = 0;
	struct vmm_vspislave *vss = NULL;

	if (vsh && (chip_select < vsh->chip_select_count)) {
		vmm_mutex_lock(&vsh->slaves_lock);

		vss = vsh->slaves[chip_select];
		if (vss && vss->xfer) {
			ret = vss->xfer(vss, data, vss->priv);
		}

		vmm_mutex_unlock(&vsh->slaves_lock);
	}

	return ret;
}
VMM_EXPORT_SYMBOL(vmm_vspihost_xfer_data)

void vmm_vspihost_schedule_xfer(struct vmm_vspihost *vsh)
{
	if (vsh) {
		vmm_completion_complete(&vsh->xfer_avail);
	}
}
VMM_EXPORT_SYMBOL(vmm_vspihost_schedule_xfer);

const char *vmm_vspihost_get_name(struct vmm_vspihost *vsh)
{
	return (vsh) ? vsh->name : NULL;
}
VMM_EXPORT_SYMBOL(vmm_vspihost_get_name);

u32 vmm_vspihost_get_chip_select_count(struct vmm_vspihost *vsh)
{
	return (vsh) ? vsh->chip_select_count : 0;
}
VMM_EXPORT_SYMBOL(vmm_vspihost_get_chip_select_count);

int vmm_vspihost_iterate_slaves(struct vmm_vspihost *vsh, void *data,
	int (*fn)(struct vmm_vspihost *, struct vmm_vspislave *, void *))
{
	u32 i;

	if (!vsh || !fn) {
		return VMM_EINVALID;
	}

	vmm_mutex_lock(&vsh->slaves_lock);

	for (i = 0; i < vsh->chip_select_count; i++) {
		fn(vsh, vsh->slaves[i], data);
	}

	vmm_mutex_unlock(&vsh->slaves_lock);

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(vmm_vspihost_iterate_slaves);

static int vspihost_xfer_worker(void *udata)
{
	struct vmm_vspihost *vsh = udata;

	if (!vsh) {
		return VMM_EFAIL;
	}

	while (1) {
		vmm_completion_wait(&vsh->xfer_avail);

		if (vsh->xfer) {
			vsh->xfer(vsh, vsh->priv);
		}
	}

	return VMM_OK;
}

struct vmm_vspihost *vmm_vspihost_create(const char *name_prefix,
				struct vmm_emudev *edev,
				void (*xfer) (struct vmm_vspihost *, void *),
				u32 chip_select_count, void *priv)
{
	bool found;
	int rc = VMM_OK;
	struct vmm_vspihost *vsh;

	if (!name_prefix || !edev || !xfer || !chip_select_count) {
		return NULL;
	}

	vsh = NULL;
	found = FALSE;

	vmm_mutex_lock(&vsctrl.vsh_list_lock);

	list_for_each_entry(vsh, &vsctrl.vsh_list, head) {
		if (vsh->edev == edev) {
			found = TRUE;
			break;
		}
	}

	if (found) {
		vmm_mutex_unlock(&vsctrl.vsh_list_lock);
		return NULL;
	}

	vsh = vmm_zalloc(sizeof(struct vmm_vspihost));
	if (!vsh) {
		vmm_mutex_unlock(&vsctrl.vsh_list_lock);
		return NULL;
	}

	INIT_LIST_HEAD(&vsh->head);
	vsh->edev = edev;
	strlcpy(vsh->name, name_prefix, sizeof(vsh->name));
	strlcat(vsh->name, "/", sizeof(vsh->name));
	if (strlcat(vsh->name, edev->node->name, sizeof(vsh->name)) >=
	    sizeof(vsh->name)) {
		vmm_free(vsh);
		vmm_mutex_unlock(&vsctrl.vsh_list_lock);
		return NULL;
	}
	vsh->xfer = xfer;
	INIT_COMPLETION(&vsh->xfer_avail);
	vsh->xfer_worker = vmm_threads_create(vsh->name,
					vspihost_xfer_worker, vsh,
					VMM_THREAD_DEF_PRIORITY,
					VMM_THREAD_DEF_TIME_SLICE);
	if (!vsh->xfer_worker) {
		vmm_free(vsh);
		vmm_mutex_unlock(&vsctrl.vsh_list_lock);
		return NULL;
	}
	vsh->chip_select_count = chip_select_count;
	INIT_MUTEX(&vsh->slaves_lock);
	vsh->slaves =
	vmm_zalloc(sizeof(struct vmm_vspislave *) * chip_select_count);
	if (!vsh->slaves) {
		vmm_threads_destroy(vsh->xfer_worker);
		vmm_free(vsh);
		vmm_mutex_unlock(&vsctrl.vsh_list_lock);
		return NULL;
	}
	vsh->priv = priv;

	rc = vmm_threads_start(vsh->xfer_worker);
	if (rc) {
		vmm_free(vsh->slaves);
		vmm_threads_destroy(vsh->xfer_worker);
		vmm_free(vsh);
		vmm_mutex_unlock(&vsctrl.vsh_list_lock);
		return NULL;
	}

	list_add_tail(&vsh->head, &vsctrl.vsh_list);

	vmm_mutex_unlock(&vsctrl.vsh_list_lock);

	return vsh;
}
VMM_EXPORT_SYMBOL(vmm_vspihost_create);

int vmm_vspihost_destroy(struct vmm_vspihost *vsh)
{
	bool found;
	int rc = VMM_OK;
	int rc1 = VMM_OK;
	struct vmm_vspihost *vs;

	if (!vsh) {
		return VMM_EFAIL;
	}

	vmm_mutex_lock(&vsctrl.vsh_list_lock);

	if (list_empty(&vsctrl.vsh_list)) {
		vmm_mutex_unlock(&vsctrl.vsh_list_lock);
		return VMM_EFAIL;
	}

	vs = NULL;
	found = FALSE;

	list_for_each_entry(vs, &vsctrl.vsh_list, head) {
		if (vs->edev == vsh->edev) {
			found = TRUE;
			break;
		}
	}

	if (!found) {
		vmm_mutex_unlock(&vsctrl.vsh_list_lock);
		return VMM_ENOTAVAIL;
	}

	list_del(&vs->head);

	rc = vmm_threads_stop(vs->xfer_worker);
	vmm_free(vs->slaves);
	rc1 = vmm_threads_destroy(vs->xfer_worker);
	vmm_free(vs);

	vmm_mutex_unlock(&vsctrl.vsh_list_lock);

	return (rc) ? rc : rc1;
}
VMM_EXPORT_SYMBOL(vmm_vspihost_destroy);

struct vmm_vspihost *vmm_vspihost_find(struct vmm_emudev *edev)
{
	bool found;
	struct vmm_vspihost *vsh;

	if (!edev) {
		return NULL;
	}

	found = FALSE;
	vsh = NULL;

	vmm_mutex_lock(&vsctrl.vsh_list_lock);

	list_for_each_entry(vsh, &vsctrl.vsh_list, head) {
		if (vsh->edev == edev) {
			found = TRUE;
			break;
		}
	}

	vmm_mutex_unlock(&vsctrl.vsh_list_lock);

	if (!found) {
		return NULL;
	}

	return vsh;
}
VMM_EXPORT_SYMBOL(vmm_vspihost_find);

int vmm_vspihost_iterate(struct vmm_vspihost *start, void *data,
			 int (*fn)(struct vmm_vspihost *vsh, void *data))
{
	int rc = VMM_OK;
	bool start_found = (start) ? FALSE : TRUE;
	struct vmm_vspihost *vsh = NULL;

	if (!fn) {
		return VMM_EINVALID;
	}

	vmm_mutex_lock(&vsctrl.vsh_list_lock);

	list_for_each_entry(vsh, &vsctrl.vsh_list, head) {
		if (!start_found) {
			if (start && start == vsh) {
				start_found = TRUE;
			} else {
				continue;
			}
		}

		rc = fn(vsh, data);
		if (rc) {
			break;
		}
	}

	vmm_mutex_unlock(&vsctrl.vsh_list_lock);

	return rc;
}
VMM_EXPORT_SYMBOL(vmm_vspihost_iterate);

u32 vmm_vspihost_count(void)
{
	u32 retval = 0;
	struct vmm_vspihost *vsh;

	vmm_mutex_lock(&vsctrl.vsh_list_lock);

	list_for_each_entry(vsh, &vsctrl.vsh_list, head) {
		retval++;
	}

	vmm_mutex_unlock(&vsctrl.vsh_list_lock);

	return retval;
}
VMM_EXPORT_SYMBOL(vmm_vspihost_count);

static int __init vmm_vspi_init(void)
{
	memset(&vsctrl, 0, sizeof(vsctrl));

	INIT_MUTEX(&vsctrl.vsh_list_lock);
	INIT_LIST_HEAD(&vsctrl.vsh_list);

	return VMM_OK;
}

static void __exit vmm_vspi_exit(void)
{
	/* Nothing to do here. */
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
