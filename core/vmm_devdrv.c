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
 * @file vmm_devdrv.c
 * @version 0.1
 * @author Anup Patel (anup@brainfault.org)
 * @brief Device driver framework source
 */

#include <vmm_error.h>
#include <vmm_string.h>
#include <vmm_stdio.h>
#include <vmm_heap.h>
#include <vmm_board.h>
#include <vmm_devtree.h>
#include <vmm_devdrv.h>
#include <vmm_host_aspace.h>

struct vmm_devdrv_ctrl {
	struct dlist driver_list;
	struct dlist class_list;
};

static struct vmm_devdrv_ctrl ddctrl;

int devdrv_device_is_compatible(vmm_devtree_node_t * node, const char *compat)
{
	const char *cp;
	int cplen, l;

	cp = vmm_devtree_attrval(node, VMM_DEVTREE_COMPATIBLE_ATTR_NAME);
	cplen = vmm_devtree_attrlen(node, VMM_DEVTREE_COMPATIBLE_ATTR_NAME);
	if (cp == NULL)
		return 0;
	while (cplen > 0) {
		if (vmm_strncmp(cp, compat, vmm_strlen(compat)) == 0)
			return 1;
		l = vmm_strlen(cp) + 1;
		cp += l;
		cplen -= l;
	}

	return 0;
}

const vmm_devid_t *devdrv_match_node(const vmm_devid_t * matches,
				     vmm_devtree_node_t * node)
{
	const char *node_type;

	if (!matches || !node) {
		return NULL;
	}

	node_type = vmm_devtree_attrval(node,
					VMM_DEVTREE_DEVICE_TYPE_ATTR_NAME);
	while (matches->name[0] || matches->type[0] || matches->compatible[0]) {
		int match = 1;
		if (matches->name[0])
			match &= node->name
			    && !vmm_strcmp(matches->name, node->name);
		if (matches->type[0])
			match &= node_type
			    && !vmm_strcmp(matches->type, node_type);
		if (matches->compatible[0])
			match &= devdrv_device_is_compatible(node,
							     matches->
							     compatible);
		if (match)
			return matches;
		matches++;
	}

	return NULL;
}

int vmm_devdrv_probe(vmm_devtree_node_t * node)
{
	int rc;
	struct dlist *l;
	vmm_devtree_node_t *child;
	vmm_driver_t *drv;
	vmm_device_t *dinst;
	const vmm_devid_t *matches;
	const vmm_devid_t *match;

	if (!node) {
		return VMM_EFAIL;
	}

	rc = VMM_OK;
	if (node->type == VMM_DEVTREE_NODETYPE_UNKNOWN && node->priv == NULL) {
		list_for_each(l, &ddctrl.driver_list) {
			drv = list_entry(l, vmm_driver_t, head);
			matches = drv->match_table;
			match = devdrv_match_node(matches, node);
			if (match) {
				dinst = vmm_malloc(sizeof(vmm_device_t));
				INIT_SPIN_LOCK(&dinst->lock);
				dinst->node = node;
				dinst->class = NULL;
				dinst->classdev = NULL;
				dinst->priv = NULL;
				node->type = VMM_DEVTREE_NODETYPE_DEVICE;
				node->priv = dinst;
#if defined(CONFIG_VERBOSE_MODE)
				vmm_printf("Probe device %s\n", node->name);
#endif
				rc = drv->probe(dinst, match);
				if (rc) {
					vmm_printf("%s: %s probe error %d\n", 
						__func__, node->name, rc);
					vmm_free(dinst);
					node->type =
					    VMM_DEVTREE_NODETYPE_UNKNOWN;
					node->priv = NULL;
				}
				break;
			}
		}
	}

	list_for_each(l, &node->child_list) {
		child = list_entry(l, vmm_devtree_node_t, head);
		vmm_devdrv_probe(child);
	}

	return rc;
}

int vmm_devdrv_remove(vmm_devtree_node_t * node)
{
	int rc;
	struct dlist *l;
	vmm_devtree_node_t *child;
	vmm_driver_t *drv;
	vmm_device_t *dinst;
	const vmm_devid_t *matches;
	const vmm_devid_t *match;

	if (!node) {
		return VMM_EFAIL;
	}

	list_for_each(l, &node->child_list) {
		child = list_entry(l, vmm_devtree_node_t, head);
		rc = vmm_devdrv_remove(child);
		if (rc) {
			return rc;
		}
	}

	rc = VMM_OK;
	if (node->type == VMM_DEVTREE_NODETYPE_DEVICE && node->priv != NULL) {
		list_for_each(l, &ddctrl.driver_list) {
			drv = list_entry(l, vmm_driver_t, head);
			matches = drv->match_table;
			match = devdrv_match_node(matches, node);
			if (match) {
				dinst = node->priv;
#if defined(CONFIG_VERBOSE_MODE)
				vmm_printf("Remove device %s\n", node->name);
#endif
				rc = drv->remove(dinst);
				if (!rc) {
					vmm_free(dinst);
					node->type =
					    VMM_DEVTREE_NODETYPE_UNKNOWN;
					node->priv = NULL;
				} else {
					vmm_printf("%s: %s remove error %d\n", 
						__func__, node->name, rc);
				}
				break;
			}
		}
	}

	return rc;
}

int vmm_devdrv_ioremap(vmm_device_t * dev, virtual_addr_t * addr, int regset)
{
	const char *attrval;

	if (!dev || !addr || regset < 0) {
		return VMM_EFAIL;
	}

	attrval = vmm_devtree_attrval(dev->node,
				      VMM_DEVTREE_VIRTUAL_REG_ATTR_NAME);

	if (attrval) {
		/* Directly return the "virtual-reg" attribute */
		*addr = ((virtual_addr_t *) attrval)[regset];
	} else {
		attrval = vmm_devtree_attrval(dev->node,
					      VMM_DEVTREE_REG_ATTR_NAME);
		if (attrval) {
			*addr =
			    vmm_host_iomap(((virtual_addr_t *) attrval)[regset],
					   ((virtual_addr_t *) attrval)[regset +
									1]);
		} else {
			return VMM_EFAIL;
		}
	}

	return VMM_OK;
}

int vmm_devdrv_getclock(vmm_device_t * dev, u32 * clock)
{
	const char *attrval;

	if (!dev || !clock) {
		return VMM_EFAIL;
	}

	attrval = vmm_devtree_attrval(dev->node,
				      VMM_DEVTREE_CLOCK_FREQ_ATTR_NAME);

	if (attrval) {
		*clock = *((u32 *) attrval);
	} else {
		return vmm_board_getclock(dev->node, clock);
	}

	return VMM_OK;
}

int vmm_devdrv_register_class(vmm_class_t * cls)
{
	bool found;
	struct dlist *l;
	vmm_class_t *c;

	if (cls == NULL) {
		return VMM_EFAIL;
	}

	c = NULL;
	found = FALSE;
	list_for_each(l, &ddctrl.class_list) {
		c = list_entry(l, vmm_class_t, head);
		if (vmm_strcmp(c->name, cls->name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (found) {
		return VMM_EINVALID;
	}

	INIT_LIST_HEAD(&cls->head);

	list_add_tail(&ddctrl.class_list, &cls->head);

	return VMM_OK;
}

int vmm_devdrv_unregister_class(vmm_class_t * cls)
{
	bool found;
	struct dlist *l;
	vmm_class_t *c;

	if (cls == NULL || list_empty(&ddctrl.class_list)) {
		return VMM_EFAIL;
	}

	c = NULL;
	found = FALSE;
	list_for_each(l, &ddctrl.class_list) {
		c = list_entry(l, vmm_class_t, head);
		if (vmm_strcmp(c->name, cls->name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (!found) {
		return VMM_ENOTAVAIL;
	}

	list_del(&c->head);

	return VMM_OK;
}

vmm_class_t *vmm_devdrv_find_class(const char *cname)
{
	bool found;
	struct dlist *l;
	vmm_class_t *cls;

	if (!cname) {
		return NULL;
	}

	found = FALSE;
	cls = NULL;

	list_for_each(l, &ddctrl.class_list) {
		cls = list_entry(l, vmm_class_t, head);
		if (vmm_strcmp(cls->name, cname) == 0) {
			found = TRUE;
			break;
		}
	}

	if (!found) {
		return NULL;
	}

	return cls;
}

vmm_class_t *vmm_devdrv_class(int index)
{
	bool found;
	struct dlist *l;
	vmm_class_t *retval;

	if (index < 0) {
		return NULL;
	}

	retval = NULL;
	found = FALSE;

	list_for_each(l, &ddctrl.class_list) {
		retval = list_entry(l, vmm_class_t, head);
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

u32 vmm_devdrv_class_count(void)
{
	u32 retval;
	struct dlist *l;

	retval = 0;

	list_for_each(l, &ddctrl.class_list) {
		retval++;
	}

	return retval;
}

int vmm_devdrv_register_classdev(const char *cname, vmm_classdev_t * cdev)
{
	bool found;
	struct dlist *l;
	vmm_class_t *c;
	vmm_classdev_t *cd;

	c = vmm_devdrv_find_class(cname);

	if (c == NULL || cdev == NULL) {
		return VMM_EFAIL;
	}

	cd = NULL;
	found = FALSE;
	list_for_each(l, &c->classdev_list) {
		cd = list_entry(l, vmm_classdev_t, head);
		if (vmm_strcmp(cd->name, cdev->name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (found) {
		return VMM_EINVALID;
	}

	if (cdev->dev) {
		vmm_spin_lock(&cdev->dev->lock);
		cdev->dev->class = c;
		cdev->dev->classdev = cdev;
		vmm_spin_unlock(&cdev->dev->lock);
	}
	INIT_LIST_HEAD(&cdev->head);
	list_add_tail(&c->classdev_list, &cdev->head);

	return VMM_OK;
}

int vmm_devdrv_unregister_classdev(const char *cname, vmm_classdev_t * cdev)
{
	bool found;
	struct dlist *l;
	vmm_class_t *c;
	vmm_classdev_t *cd;

	c = vmm_devdrv_find_class(cname);

	if (c == NULL || cdev == NULL) {
		return VMM_EFAIL;
	}
	if (list_empty(&c->classdev_list)) {
		return VMM_EFAIL;
	}

	cd = NULL;
	found = FALSE;
	list_for_each(l, &c->classdev_list) {
		cd = list_entry(l, vmm_classdev_t, head);
		if (vmm_strcmp(cd->name, cdev->name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (!found) {
		return VMM_ENOTAVAIL;
	}

	if (cd->dev) {
		vmm_spin_lock(&cd->dev->lock);
		cd->dev->class = NULL;
		cd->dev->classdev = NULL;
		vmm_spin_unlock(&cd->dev->lock);
	}
	list_del(&cd->head);

	return VMM_OK;
}

vmm_classdev_t *vmm_devdrv_find_classdev(const char *cname,
					 const char *cdev_name)
{
	bool found;
	struct dlist *l;
	vmm_class_t *c;
	vmm_classdev_t *cd;

	c = vmm_devdrv_find_class(cname);

	if (c == NULL || cdev_name == NULL) {
		return NULL;
	}

	found = FALSE;
	cd = NULL;

	list_for_each(l, &c->classdev_list) {
		cd = list_entry(l, vmm_classdev_t, head);
		if (vmm_strcmp(cd->name, cdev_name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (!found) {
		return NULL;
	}

	return cd;
}

vmm_classdev_t *vmm_devdrv_classdev(const char *cname, int index)
{
	bool found;
	struct dlist *l;
	vmm_class_t *c;
	vmm_classdev_t *retval;

	c = vmm_devdrv_find_class(cname);

	if (c == NULL || index < 0) {
		return NULL;
	}

	retval = NULL;
	found = FALSE;

	list_for_each(l, &c->classdev_list) {
		retval = list_entry(l, vmm_classdev_t, head);
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

u32 vmm_devdrv_classdev_count(const char *cname)
{
	u32 retval;
	struct dlist *l;
	vmm_class_t *c;

	c = vmm_devdrv_find_class(cname);

	if (c == NULL) {
		return VMM_EFAIL;
	}

	retval = 0;

	list_for_each(l, &c->classdev_list) {
		retval++;
	}

	return retval;
}

int vmm_devdrv_register_driver(vmm_driver_t * drv)
{
	bool found;
	struct dlist *l;
	vmm_driver_t *d;

	if (drv == NULL) {
		return VMM_EFAIL;
	}

	d = NULL;
	found = FALSE;
	list_for_each(l, &ddctrl.driver_list) {
		d = list_entry(l, vmm_driver_t, head);
		if (vmm_strcmp(d->name, drv->name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (found) {
		return VMM_EINVALID;
	}

	INIT_LIST_HEAD(&drv->head);

	list_add_tail(&ddctrl.driver_list, &drv->head);

	return VMM_OK;
}

int vmm_devdrv_unregister_driver(vmm_driver_t * drv)
{
	bool found;
	struct dlist *l;
	vmm_driver_t *d;

	if (drv == NULL || list_empty(&ddctrl.driver_list)) {
		return VMM_EFAIL;
	}

	d = NULL;
	found = FALSE;
	list_for_each(l, &ddctrl.driver_list) {
		d = list_entry(l, vmm_driver_t, head);
		if (vmm_strcmp(d->name, drv->name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (!found) {
		return VMM_ENOTAVAIL;
	}

	list_del(&d->head);

	return VMM_OK;
}

vmm_driver_t *vmm_devdrv_find_driver(const char *name)
{
	bool found;
	struct dlist *l;
	vmm_driver_t *drv;

	if (!name) {
		return NULL;
	}

	found = FALSE;
	drv = NULL;

	list_for_each(l, &ddctrl.driver_list) {
		drv = list_entry(l, vmm_driver_t, head);
		if (vmm_strcmp(drv->name, name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (!found) {
		return NULL;
	}

	return drv;
}

vmm_driver_t *vmm_devdrv_driver(int index)
{
	bool found;
	struct dlist *l;
	vmm_driver_t *retval;

	if (index < 0) {
		return NULL;
	}

	retval = NULL;
	found = FALSE;

	list_for_each(l, &ddctrl.driver_list) {
		retval = list_entry(l, vmm_driver_t, head);
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

u32 vmm_devdrv_driver_count(void)
{
	u32 retval;
	struct dlist *l;

	retval = 0;

	list_for_each(l, &ddctrl.driver_list) {
		retval++;
	}

	return retval;
}

int __init_section vmm_devdrv_init(void)
{
	vmm_memset(&ddctrl, 0, sizeof(ddctrl));

	INIT_LIST_HEAD(&ddctrl.driver_list);
	INIT_LIST_HEAD(&ddctrl.class_list);

	return VMM_OK;
}
