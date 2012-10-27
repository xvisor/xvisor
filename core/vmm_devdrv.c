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

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_heap.h>
#include <vmm_devtree.h>
#include <vmm_devdrv.h>
#include <vmm_mutex.h>
#include <libs/stringlib.h>

struct probe_node {
	struct dlist head;
	struct vmm_devtree_node *node;
	struct vmm_devclk *(*getclk) (struct vmm_devtree_node *);
	void (*putclk) (struct vmm_devclk *);
};

struct vmm_devdrv_ctrl {
	struct vmm_mutex device_lock;
	struct dlist device_list;
	struct vmm_mutex driver_lock;
	struct dlist driver_list;
	struct vmm_mutex class_lock;
	struct dlist class_list;
	struct vmm_mutex pnode_lock;
	struct dlist pnode_list;
};

static struct vmm_devdrv_ctrl ddctrl;

static int devdrv_device_is_compatible(struct vmm_devtree_node *node,
					const char *compat)
{
	const char *cp;
	int cplen, l;

	cp = vmm_devtree_attrval(node, VMM_DEVTREE_COMPATIBLE_ATTR_NAME);
	cplen = vmm_devtree_attrlen(node, VMM_DEVTREE_COMPATIBLE_ATTR_NAME);
	if (cp == NULL)
		return 0;
	while (cplen > 0) {
		if (strncmp(cp, compat, strlen(compat)) == 0)
			return 1;
		l = strlen(cp) + 1;
		cp += l;
		cplen -= l;
	}

	return 0;
}

static const struct vmm_devid *devdrv_match_node(
					  const struct vmm_devid *matches,
					  struct vmm_devtree_node *node)
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
			    && !strcmp(matches->name, node->name);
		if (matches->type[0])
			match &= node_type
			    && !strcmp(matches->type, node_type);
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

/* Must be called with 'ddctrl.device_lock' held */
static void devdrv_probe(struct vmm_devtree_node *node, 
		struct vmm_driver *drv,
		struct vmm_devclk *(*getclk) (struct vmm_devtree_node *))
{
	int rc;
	bool found;
	struct dlist *l;
	struct vmm_devtree_node *child;
	struct vmm_device *dinst;
	const struct vmm_devid *matches;
	const struct vmm_devid *match;

	found = FALSE;
	list_for_each(l, &ddctrl.device_list) {
		dinst = list_entry(l, struct vmm_device, head);
		if (dinst->node == node) {
			found = TRUE;
			break;
		}
	}

	if (!found) {
		matches = drv->match_table;
		match = devdrv_match_node(matches, node);
		if (match) {
			dinst = vmm_malloc(sizeof(struct vmm_device));
			INIT_LIST_HEAD(&dinst->head);
			INIT_SPIN_LOCK(&dinst->lock);
			dinst->clk = (getclk) ? getclk(node) : NULL;
			dinst->node = node;
			dinst->class = NULL;
			dinst->classdev = NULL;
			dinst->drv = drv;
			dinst->priv = NULL;
#if defined(CONFIG_VERBOSE_MODE)
			vmm_printf("Probe device %s\n", node->name);
#endif
			rc = drv->probe(dinst, match);
			if (rc) {
				vmm_printf("%s: %s probe error %d\n", 
					   __func__, node->name, rc);
				vmm_free(dinst);
			} else {
				list_add_tail(&dinst->head, &ddctrl.device_list);
			}
		}
	}

	list_for_each(l, &node->child_list) {
		child = list_entry(l, struct vmm_devtree_node, head);
		devdrv_probe(child, drv, getclk);
	}
}

/* Must be called with 'ddctrl.device_lock' held */
static void devdrv_remove(struct vmm_devtree_node *node,
			  struct vmm_driver *drv,
			  void (*putclk) (struct vmm_devclk *))
{
	int rc;
	bool found;
	struct dlist *l;
	struct vmm_devtree_node *child;
	struct vmm_device *dinst;

	list_for_each(l, &node->child_list) {
		child = list_entry(l, struct vmm_devtree_node, head);
		devdrv_remove(child, drv, putclk);
	}

	found = FALSE;
	list_for_each(l, &ddctrl.device_list) {
		dinst = list_entry(l, struct vmm_device, head);
		if (dinst->node == node) {
			found = TRUE;
			break;
		}
	}

	if (found) {
		if (dinst->drv != drv) {
			return;
		}
#if defined(CONFIG_VERBOSE_MODE)
		vmm_printf("Remove device %s\n", node->name);
#endif
		rc = drv->remove(dinst);
		if (rc) {
			vmm_printf("%s: %s remove error %d\n", 
				   __func__, node->name, rc);
		}
		if (dinst->clk && putclk) {
			putclk(dinst->clk);
		}
		vmm_free(dinst);
		list_del(&dinst->head);
	}
}

int vmm_devdrv_probe(struct vmm_devtree_node *node, 
		     struct vmm_devclk *(*getclk) (struct vmm_devtree_node *),
		     void (*putclk) (struct vmm_devclk *))
{
	bool found;
	struct dlist *l;
	struct vmm_driver *drv;
	struct probe_node *pnode;

	if (!node) {
		return VMM_EFAIL;
	}

	vmm_mutex_lock(&ddctrl.pnode_lock);

	found = FALSE;
	list_for_each(l, &ddctrl.pnode_list) {
		pnode = list_entry(l, struct probe_node, head);
		if (pnode->node == node) {
			found = TRUE;
			break;
		}
	}

	if (!found) {
		pnode = vmm_malloc(sizeof(*pnode));
		if (!pnode) {
			vmm_mutex_unlock(&ddctrl.pnode_lock);
			return VMM_ENOMEM;
		}

		INIT_LIST_HEAD(&pnode->head);
		pnode->node = node;
		pnode->getclk = getclk;
		pnode->putclk = putclk;

		list_add_tail(&pnode->head, &ddctrl.pnode_list);
	}

	vmm_mutex_unlock(&ddctrl.pnode_lock);

	vmm_mutex_lock(&ddctrl.device_lock);
	vmm_mutex_lock(&ddctrl.driver_lock);

	list_for_each(l, &ddctrl.driver_list) {
		drv = list_entry(l, struct vmm_driver, head);
		devdrv_probe(node, drv, pnode->getclk);
	}

	vmm_mutex_unlock(&ddctrl.driver_lock);
	vmm_mutex_unlock(&ddctrl.device_lock);

	return VMM_OK;
}

int vmm_devdrv_remove(struct vmm_devtree_node *node)
{
	bool found;
	struct dlist *l;
	struct vmm_driver *drv;
	struct probe_node *pnode;

	if (!node) {
		return VMM_EFAIL;
	}

	vmm_mutex_lock(&ddctrl.pnode_lock);

	found = FALSE;
	list_for_each(l, &ddctrl.pnode_list) {
		pnode = list_entry(l, struct probe_node, head);
		if (pnode->node == node) {
			found = TRUE;
			break;
		}
	}

	if (!found) {
		vmm_mutex_unlock(&ddctrl.pnode_lock);
		return VMM_EFAIL;
	}

	list_del(&pnode->head);

	vmm_mutex_unlock(&ddctrl.pnode_lock);

	vmm_mutex_lock(&ddctrl.device_lock);
	vmm_mutex_lock(&ddctrl.driver_lock);

	list_for_each(l, &ddctrl.driver_list) {
		drv = list_entry(l, struct vmm_driver, head);
		devdrv_remove(node, drv, pnode->putclk);
	}

	vmm_mutex_unlock(&ddctrl.driver_lock);
	vmm_mutex_unlock(&ddctrl.device_lock);

	vmm_free(pnode);

	return VMM_OK;
}

bool vmm_devdrv_clock_isenabled(struct vmm_device *dev)
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

int vmm_devdrv_clock_enable(struct vmm_device *dev)
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

	if (dev->clk && dev->clk->enable) {
		return dev->clk->enable(dev->clk);
	}

	return VMM_EFAIL;
}

int vmm_devdrv_clock_disable(struct vmm_device *dev)
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
		dev->clk->disable(dev->clk);
		return VMM_OK;
	}

	return VMM_EFAIL;
}

long vmm_devdrv_clock_round_rate(struct vmm_device *dev, 
					  unsigned long rate)
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

	if (dev->clk && dev->clk->round_rate) {
		return dev->clk->round_rate(dev->clk, rate);
	}

	return rate;
}

unsigned long vmm_devdrv_clock_get_rate(struct vmm_device *dev)
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

	if (dev->clk && dev->clk->get_rate) {
		if (!vmm_devdrv_clock_isenabled(dev)) {
			if (vmm_devdrv_clock_enable(dev)) {
				return 0;
			}
		}
		return dev->clk->get_rate(dev->clk);
	}

	return 0;
}

int vmm_devdrv_clock_set_rate(struct vmm_device *dev, unsigned long rate)
{
	const char *clkattr;

	if (!dev) {
		return 0;
	}

	clkattr = vmm_devtree_attrval(dev->node,
				      VMM_DEVTREE_CLOCK_RATE_ATTR_NAME);
	if (clkattr) {
		return VMM_OK;
	}

	if (dev->clk && dev->clk->set_rate) {
		if (!vmm_devdrv_clock_isenabled(dev)) {
			if (vmm_devdrv_clock_enable(dev)) {
				return 0;
			}
		}
		return dev->clk->set_rate(dev->clk, rate);
	}

	return VMM_OK;
}

int vmm_devdrv_register_class(struct vmm_class *cls)
{
	bool found;
	struct dlist *l;
	struct vmm_class *c;

	if (cls == NULL) {
		return VMM_EFAIL;
	}

	c = NULL;
	found = FALSE;

	vmm_mutex_lock(&ddctrl.class_lock);

	list_for_each(l, &ddctrl.class_list) {
		c = list_entry(l, struct vmm_class, head);
		if (strcmp(c->name, cls->name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (found) {
		vmm_mutex_unlock(&ddctrl.class_lock);
		return VMM_EINVALID;
	}

	INIT_LIST_HEAD(&cls->head);

	list_add_tail(&cls->head, &ddctrl.class_list);

	vmm_mutex_unlock(&ddctrl.class_lock);

	return VMM_OK;
}

int vmm_devdrv_unregister_class(struct vmm_class *cls)
{
	bool found;
	struct dlist *l;
	struct vmm_class *c;

	vmm_mutex_lock(&ddctrl.class_lock);

	if (cls == NULL || list_empty(&ddctrl.class_list)) {
		vmm_mutex_unlock(&ddctrl.class_lock);
		return VMM_EFAIL;
	}

	c = NULL;
	found = FALSE;
	list_for_each(l, &ddctrl.class_list) {
		c = list_entry(l, struct vmm_class, head);
		if (strcmp(c->name, cls->name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (!found) {
		vmm_mutex_unlock(&ddctrl.class_lock);
		return VMM_ENOTAVAIL;
	}

	list_del(&c->head);

	vmm_mutex_unlock(&ddctrl.class_lock);

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

	vmm_mutex_lock(&ddctrl.class_lock);

	list_for_each(l, &ddctrl.class_list) {
		cls = list_entry(l, struct vmm_class, head);
		if (strcmp(cls->name, cname) == 0) {
			found = TRUE;
			break;
		}
	}

	vmm_mutex_unlock(&ddctrl.class_lock);

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

	vmm_mutex_lock(&ddctrl.class_lock);

	list_for_each(l, &ddctrl.class_list) {
		retval = list_entry(l, struct vmm_class, head);
		if (!index) {
			found = TRUE;
			break;
		}
		index--;
	}

	vmm_mutex_unlock(&ddctrl.class_lock);

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

	vmm_mutex_lock(&ddctrl.class_lock);

	list_for_each(l, &ddctrl.class_list) {
		retval++;
	}

	vmm_mutex_unlock(&ddctrl.class_lock);

	return retval;
}

int vmm_devdrv_register_classdev(const char *cname, struct vmm_classdev *cdev)
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
		if (strcmp(cd->name, cdev->name) == 0) {
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
	list_add_tail(&cdev->head, &c->classdev_list);

	return VMM_OK;
}

int vmm_devdrv_unregister_classdev(const char *cname, struct vmm_classdev *cdev)
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
		if (strcmp(cd->name, cdev->name) == 0) {
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
		if (strcmp(cd->name, cdev_name) == 0) {
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

int vmm_devdrv_register_driver(struct vmm_driver *drv)
{
	bool found;
	struct dlist *l;
	struct vmm_driver *d;
	struct probe_node *pnode;

	if (drv == NULL) {
		return VMM_EFAIL;
	}

	d = NULL;
	found = FALSE;

	vmm_mutex_lock(&ddctrl.driver_lock);

	list_for_each(l, &ddctrl.driver_list) {
		d = list_entry(l, struct vmm_driver, head);
		if (strcmp(d->name, drv->name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (found) {
		vmm_mutex_unlock(&ddctrl.driver_lock);
		return VMM_EINVALID;
	}

	INIT_LIST_HEAD(&drv->head);

	list_add_tail(&drv->head, &ddctrl.driver_list);

	vmm_mutex_unlock(&ddctrl.driver_lock);

	vmm_mutex_lock(&ddctrl.device_lock);
	vmm_mutex_lock(&ddctrl.pnode_lock);

	list_for_each(l, &ddctrl.pnode_list) {
		pnode = list_entry(l, struct probe_node, head);
		devdrv_probe(pnode->node, drv, pnode->getclk);
	}

	vmm_mutex_unlock(&ddctrl.pnode_lock);
	vmm_mutex_unlock(&ddctrl.device_lock);

	return VMM_OK;
}

int vmm_devdrv_unregister_driver(struct vmm_driver *drv)
{
	bool found;
	struct dlist *l;
	struct vmm_driver *d;
	struct probe_node *pnode;

	vmm_mutex_lock(&ddctrl.driver_lock);

	if (drv == NULL || list_empty(&ddctrl.driver_list)) {
		vmm_mutex_unlock(&ddctrl.driver_lock);
		return VMM_EFAIL;
	}

	d = NULL;
	found = FALSE;
	list_for_each(l, &ddctrl.driver_list) {
		d = list_entry(l, struct vmm_driver, head);
		if (strcmp(d->name, drv->name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (!found) {
		vmm_mutex_unlock(&ddctrl.driver_lock);
		return VMM_ENOTAVAIL;
	}

	list_del(&d->head);

	vmm_mutex_unlock(&ddctrl.driver_lock);

	vmm_mutex_lock(&ddctrl.device_lock);
	vmm_mutex_lock(&ddctrl.pnode_lock);

	list_for_each(l, &ddctrl.pnode_list) {
		pnode = list_entry(l, struct probe_node, head);
		devdrv_remove(pnode->node, drv, pnode->putclk);
	}

	vmm_mutex_unlock(&ddctrl.pnode_lock);
	vmm_mutex_unlock(&ddctrl.device_lock);

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

	vmm_mutex_lock(&ddctrl.driver_lock);

	list_for_each(l, &ddctrl.driver_list) {
		drv = list_entry(l, struct vmm_driver, head);
		if (strcmp(drv->name, name) == 0) {
			found = TRUE;
			break;
		}
	}

	vmm_mutex_unlock(&ddctrl.driver_lock);

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

	vmm_mutex_lock(&ddctrl.driver_lock);

	list_for_each(l, &ddctrl.driver_list) {
		retval = list_entry(l, struct vmm_driver, head);
		if (!index) {
			found = TRUE;
			break;
		}
		index--;
	}

	vmm_mutex_unlock(&ddctrl.driver_lock);

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

	vmm_mutex_lock(&ddctrl.driver_lock);

	list_for_each(l, &ddctrl.driver_list) {
		retval++;
	}

	vmm_mutex_unlock(&ddctrl.driver_lock);

	return retval;
}

int __init vmm_devdrv_init(void)
{
	memset(&ddctrl, 0, sizeof(ddctrl));

	INIT_MUTEX(&ddctrl.device_lock);
	INIT_LIST_HEAD(&ddctrl.device_list);
	INIT_MUTEX(&ddctrl.driver_lock);
	INIT_LIST_HEAD(&ddctrl.driver_list);
	INIT_MUTEX(&ddctrl.class_lock);
	INIT_LIST_HEAD(&ddctrl.class_list);
	INIT_MUTEX(&ddctrl.pnode_lock);
	INIT_LIST_HEAD(&ddctrl.pnode_list);

	return VMM_OK;
}
