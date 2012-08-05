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
 * @file vmm_devdrv.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Device driver framework header
 */

#ifndef __VMM_DEVDRV_H_
#define __VMM_DEVDRV_H_

#include <list.h>
#include <vmm_types.h>
#include <vmm_devtree.h>
#include <vmm_spinlocks.h>

struct vmm_devid;
struct vmm_devclk;
struct vmm_device;

struct vmm_class;
struct vmm_classdev;

struct vmm_driver;

struct vmm_devid {
	char name[32];
	char type[32];
	char compatible[128];
	void *data;
};

struct vmm_devclk {
	struct vmm_devclk *parent;
	int (*isenabled) (struct vmm_devclk *);
	int (*enable) (struct vmm_devclk *);
	int (*disable) (struct vmm_devclk *);
	u32 (*getrate) (struct vmm_devclk *);
	u32 (*setrate) (struct vmm_devclk *, u32);
	void *priv;
};

struct vmm_device {
	vmm_spinlock_t lock;
	struct vmm_devclk *clk;
	struct vmm_devtree_node *node;
	struct vmm_class *class;
	struct vmm_classdev *classdev;
	struct vmm_driver *drv;
	void *priv;
};

struct vmm_classdev {
	struct dlist head;
	char name[32];
	struct vmm_device *dev;
	void *priv;
};

struct vmm_class {
	struct dlist head;
	char name[32];
	struct dlist classdev_list;
};

struct vmm_driver {
	struct dlist head;
	char name[32];
	const struct vmm_devid *match_table;
	int (*probe) (struct vmm_device *, const struct vmm_devid *);
	int (*suspend) (struct vmm_device *);
	int (*resume) (struct vmm_device *);
	int (*remove) (struct vmm_device *);
};

/** Probe device instances under a given device tree node */
int vmm_devdrv_probe(struct vmm_devtree_node *node, 
		     struct vmm_devclk *(*getclk) (struct vmm_devtree_node *));

/** Remove device instances under a given device tree node */
int vmm_devdrv_remove(struct vmm_devtree_node *node,
		      void (*putclk) (struct vmm_devclk *));

/** Map device registers to virtual address based on
 *  'reg' and 'virtual-reg' attributes of device tree node
 */
int vmm_devdrv_regmap(struct vmm_device *dev, 
		      virtual_addr_t *addr, int regset);

/** Unmap device registers from virtual address based on
 *  'reg' and 'virtual-reg' attributes of device tree node
 */
int vmm_devdrv_regunmap(struct vmm_device *dev, 
			virtual_addr_t addr, int regset);

/** Check if clock is enabled for given device */
bool vmm_devdrv_clock_isenabled(struct vmm_device *dev);

/** Enable clock for given device */
int vmm_devdrv_clock_enable(struct vmm_device *dev);

/** Disable clock for given device */
int vmm_devdrv_clock_disable(struct vmm_device *dev);

/** Get clock rate for given device 
 *  NOTE: If clock is not enabled then this will enable clock first.
 */
u32 vmm_devdrv_clock_getrate(struct vmm_device *dev);

/** Set clock rate for given device 
 *  NOTE: If clock is not enabled then this will enable clock first.
 */
int vmm_devdrv_clock_setrate(struct vmm_device *dev, u32 rate);

/** Register class */
int vmm_devdrv_register_class(struct vmm_class *cls);

/** Unregister class */
int vmm_devdrv_unregister_class(struct vmm_class *cls);

/** Find a registered class */
struct vmm_class *vmm_devdrv_find_class(const char *cname);

/** Get a registered class */
struct vmm_class *vmm_devdrv_class(int index);

/** Count available classes */
u32 vmm_devdrv_class_count(void);

/** Register device to a class */
int vmm_devdrv_register_classdev(const char *cname, 
				 struct vmm_classdev *cdev);

/** Unregister device from a class */
int vmm_devdrv_unregister_classdev(const char *cname, 
				   struct vmm_classdev *cdev);

/** Find a class device under a class */
struct vmm_classdev *vmm_devdrv_find_classdev(const char *cname,
					 const char *cdev_name);

/** Get a class device from a class */
struct vmm_classdev *vmm_devdrv_classdev(const char *cname, int index);

/** Count available class devices under a class */
u32 vmm_devdrv_classdev_count(const char *cname);

/** Register device driver */
int vmm_devdrv_register_driver(struct vmm_driver *drv);

/** Unregister device driver */
int vmm_devdrv_unregister_driver(struct vmm_driver *drv);

/** Find a registered driver */
struct vmm_driver *vmm_devdrv_find_driver(const char *name);

/** Get a registered driver */
struct vmm_driver *vmm_devdrv_driver(int index);

/** Count available device drivers */
u32 vmm_devdrv_driver_count(void);

/** Initalize device driver framework */
int vmm_devdrv_init(void);

#endif /* __VMM_DEVDRV_H_ */
