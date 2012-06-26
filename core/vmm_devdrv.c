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
 * @author Anup Patel (anup@brainfault.org)
 * @brief Device driver framework source
 */

#include <arch_board.h>
#include <vmm_error.h>
#include <vmm_string.h>
#include <vmm_stdio.h>
#include <vmm_heap.h>
#include <vmm_devtree.h>
#include <vmm_devdrv.h>
#include <vmm_host_aspace.h>

struct vmm_devdrv_ctrl {
	struct dlist driver_list;
	struct dlist class_list;
};

static struct vmm_devdrv_ctrl ddctrl;

int devdrv_device_is_compatible(struct vmm_devtree_node * node, const char *compat)
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

const struct vmm_devid *devdrv_match_node(const struct vmm_devid * matches,
					  struct vmm_devtree_node * node)
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

int vmm_devdrv_probe(struct vmm_devtree_node * node, 
		     vmm_devdrv_getclk_t getclk)
{
	int rc;
	struct dlist *l;
	struct vmm_devtree_node *child;
	struct vmm_driver *drv;
	struct vmm_device *dinst;
	const struct vmm_devid *matches;
	const struct vmm_devid *match;

	if (!node) {
		return VMM_EFAIL;
	}

	rc = VMM_OK;
	if (node->type == VMM_DEVTREE_NODETYPE_UNKNOWN && node->priv == NULL) {
		list_for_each(l, &ddctrl.driver_list) {
			drv = list_entry(l, struct vmm_driver, head);
			matches = drv->match_table;
			match = devdrv_match_node(matches, node);
			if (match) {
				dinst = vmm_malloc(sizeof(struct vmm_device));
				INIT_SPIN_LOCK(&dinst->lock);
				dinst->clk = (getclk) ? getclk(node) : NULL;
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
		child = list_entry(l, struct vmm_devtree_node, head);
		vmm_devdrv_probe(child, getclk);
	}

	return rc;
}

int vmm_devdrv_remove(struct vmm_devtree_node * node,
		      vmm_devdrv_putclk_t putclk)
{
	int rc;
	struct dlist *l;
	struct vmm_devtree_node *child;
	struct vmm_driver *drv;
	struct vmm_device *dinst;
	const struct vmm_devid *matches;
	const struct vmm_devid *match;

	if (!node) {
		return VMM_EFAIL;
	}

	list_for_each(l, &node->child_list) {
		child = list_entry(l, struct vmm_devtree_node, head);
		rc = vmm_devdrv_remove(child, putclk);
		if (rc) {
			return rc;
		}
	}

	rc = VMM_OK;
	if (node->type == VMM_DEVTREE_NODETYPE_DEVICE && node->priv != NULL) {
		list_for_each(l, &ddctrl.driver_list) {
			drv = list_entry(l, struct vmm_driver, head);
			matches = drv->match_table;
			match = devdrv_match_node(matches, node);
			if (match) {
				dinst = node->priv;
#if defined(CONFIG_VERBOSE_MODE)
				vmm_printf("Remove device %s\n", node->name);
#endif
				rc = drv->remove(dinst);
				if (!rc) {
					if (dinst->clk && putclk) {
						putclk(dinst->clk);
					}
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

int vmm_devdrv_regmap(struct vmm_device * dev, 
		      virtual_addr_t * addr, int regset)
{
	const char *aval;
	physical_addr_t pa;
	virtual_size_t sz;

	if (!dev || !addr || regset < 0) {
		return VMM_EFAIL;
	}

	aval = vmm_devtree_attrval(dev->node,
				   VMM_DEVTREE_VIRTUAL_REG_ATTR_NAME);

	if (aval) {
		/* Directly return the "virtual-reg" attribute */
		*addr = ((virtual_addr_t *) aval)[regset];
	} else {
		aval = vmm_devtree_attrval(dev->node,
					   VMM_DEVTREE_REG_ATTR_NAME);
		if (aval) {
			aval += regset * (sizeof(pa) + sizeof(sz));
			pa = *((physical_addr_t *)aval);
			aval += sizeof(pa);
			sz = *((virtual_size_t *)aval);
			*addr = vmm_host_iomap(pa, sz);
		} else {
			return VMM_EFAIL;
		}
	}

	return VMM_OK;
}

bool vmm_devdrv_clock_isenabled(struct vmm_device * dev)
{
	const char *clkattr;

	if (!dev) {
		return FALSE;
	}

	clkattr = vmm_devtree_attrval(dev->node,
				      VMM_DEVTREE_CLOCK_RATE_ATTR_NAME);
	if (clkattr) {
		return TRUE;
	}

	if (dev->clk && dev->clk->isenabled) {
		return dev->clk->isenabled(dev->clk);
	}

	return FALSE;
}

static int devdrv_clock_enable(struct vmm_devclk * clk)
{
	int rc = VMM_OK;
	bool enabled = FALSE;

	if (clk->isenabled) {
		enabled = clk->isenabled(clk);
	}

	if (!enabled) {
		if (clk->parent) {
			rc = devdrv_clock_enable(clk->parent);
			if (rc) {
				return rc;
			}
		}
		if (clk->enable) {
			return clk->enable(clk);
		} else {
			return VMM_EFAIL;
		}
	}

	return VMM_OK;
}

int vmm_devdrv_clock_enable(struct vmm_device * dev)
{
	const char *clkattr;

	if (!dev) {
		return VMM_EFAIL;
	}

	clkattr = vmm_devtree_attrval(dev->node,
				      VMM_DEVTREE_CLOCK_RATE_ATTR_NAME);
	if (clkattr) {
		return VMM_OK;
	}

	if (dev->clk) {
		return devdrv_clock_enable(dev->clk);
	}

	return VMM_EFAIL;
}

int vmm_devdrv_clock_disable(struct vmm_device * dev)
{
	const char *clkattr;

	if (!dev) {
		return VMM_EFAIL;
	}

	clkattr = vmm_devtree_attrval(dev->node,
				      VMM_DEVTREE_CLOCK_RATE_ATTR_NAME);
	if (clkattr) {
		return VMM_OK;
	}

	if (dev->clk && dev->clk->disable) {
		return dev->clk->disable(dev->clk);
	}

	return VMM_EFAIL;
}

u32 vmm_devdrv_clock_rate(struct vmm_device * dev)
{
	const char *clkattr;

	if (!dev) {
		return 0;
	}

	clkattr = vmm_devtree_attrval(dev->node,
				      VMM_DEVTREE_CLOCK_RATE_ATTR_NAME);
	if (clkattr) {
		return *((u32 *)clkattr);
	}

	if (dev->clk && dev->clk->getrate) {
		if (!vmm_devdrv_clock_isenabled(dev)) {
			if (vmm_devdrv_clock_enable(dev)) {
				return 0;
			}
		}
		return dev->clk->getrate(dev->clk);
	}

	return 0;
}

int vmm_devdrv_register_class(struct vmm_class * cls)
{
	bool found;
	struct dlist *l;
	struct vmm_class *c;

	if (cls == NULL) {
		return VMM_EFAIL;
	}

	c = NULL;
	found = FALSE;
	list_for_each(l, &ddctrl.class_list) {
		c = list_entry(l, struct vmm_class, head);
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

int vmm_devdrv_unregister_class(struct vmm_class * cls)
{
	bool found;
	struct dlist *l;
	struct vmm_class *c;

	if (cls == NULL || list_empty(&ddctrl.class_list)) {
		return VMM_EFAIL;
	}

	c = NULL;
	found = FALSE;
	list_for_each(l, &ddctrl.class_list) {
		c = list_entry(l, struct vmm_class, head);
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

struct vmm_class *vmm_devdrv_find_class(const char *cname)
{
	bool found;
	struct dlist *l;
	struct vmm_class *cls;

	if (!cname) {
		return NULL;
	}

	found = FALSE;
	cls = NULL;

	list_for_each(l, &ddctrl.class_list) {
		cls = list_entry(l, struct vmm_class, head);
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

struct vmm_class *vmm_devdrv_class(int index)
{
	bool found;
	struct dlist *l;
	struct vmm_class *retval;

	if (index < 0) {
		return NULL;
	}

	retval = NULL;
	found = FALSE;

	list_for_each(l, &ddctrl.class_list) {
		retval = list_entry(l, struct vmm_class, head);
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

int vmm_devdrv_register_classdev(const char *cname, struct vmm_classdev * cdev)
{
	bool found;
	struct dlist *l;
	struct vmm_class *c;
	struct vmm_classdev *cd;

	c = vmm_devdrv_find_class(cname);

	if (c == NULL || cdev == NULL) {
		return VMM_EFAIL;
	}

	cd = NULL;
	found = FALSE;
	list_for_each(l, &c->classdev_list) {
		cd = list_entry(l, struct vmm_classdev, head);
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

int vmm_devdrv_unregister_classdev(const char *cname, struct vmm_classdev * cdev)
{
	bool found;
	struct dlist *l;
	struct vmm_class *c;
	struct vmm_classdev *cd;

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
		cd = list_entry(l, struct vmm_classdev, head);
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

struct vmm_classdev *vmm_devdrv_find_classdev(const char *cname,
					 const char *cdev_name)
{
	bool found;
	struct dlist *l;
	struct vmm_class *c;
	struct vmm_classdev *cd;

	c = vmm_devdrv_find_class(cname);

	if (c == NULL || cdev_name == NULL) {
		return NULL;
	}

	found = FALSE;
	cd = NULL;

	list_for_each(l, &c->classdev_list) {
		cd = list_entry(l, struct vmm_classdev, head);
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

struct vmm_classdev *vmm_devdrv_classdev(const char *cname, int index)
{
	bool found;
	struct dlist *l;
	struct vmm_class *c;
	struct vmm_classdev *retval;

	c = vmm_devdrv_find_class(cname);

	if (c == NULL || index < 0) {
		return NULL;
	}

	retval = NULL;
	found = FALSE;

	list_for_each(l, &c->classdev_list) {
		retval = list_entry(l, struct vmm_classdev, head);
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
	struct vmm_class *c;

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

int vmm_devdrv_register_driver(struct vmm_driver * drv)
{
	bool found;
	struct dlist *l;
	struct vmm_driver *d;

	if (drv == NULL) {
		return VMM_EFAIL;
	}

	d = NULL;
	found = FALSE;
	list_for_each(l, &ddctrl.driver_list) {
		d = list_entry(l, struct vmm_driver, head);
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

int vmm_devdrv_unregister_driver(struct vmm_driver * drv)
{
	bool found;
	struct dlist *l;
	struct vmm_driver *d;

	if (drv == NULL || list_empty(&ddctrl.driver_list)) {
		return VMM_EFAIL;
	}

	d = NULL;
	found = FALSE;
	list_for_each(l, &ddctrl.driver_list) {
		d = list_entry(l, struct vmm_driver, head);
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

struct vmm_driver *vmm_devdrv_find_driver(const char *name)
{
	bool found;
	struct dlist *l;
	struct vmm_driver *drv;

	if (!name) {
		return NULL;
	}

	found = FALSE;
	drv = NULL;

	list_for_each(l, &ddctrl.driver_list) {
		drv = list_entry(l, struct vmm_driver, head);
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

struct vmm_driver *vmm_devdrv_driver(int index)
{
	bool found;
	struct dlist *l;
	struct vmm_driver *retval;

	if (index < 0) {
		return NULL;
	}

	retval = NULL;
	found = FALSE;

	list_for_each(l, &ddctrl.driver_list) {
		retval = list_entry(l, struct vmm_driver, head);
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

int __init vmm_devdrv_init(void)
{
	vmm_memset(&ddctrl, 0, sizeof(ddctrl));

	INIT_LIST_HEAD(&ddctrl.driver_list);
	INIT_LIST_HEAD(&ddctrl.class_list);

	return VMM_OK;
}
