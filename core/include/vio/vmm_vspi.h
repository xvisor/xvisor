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
 * @file vmm_vspi.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for virtual spi framework
 */
#ifndef _VMM_VSPI_H__
#define _VMM_VSPI_H__

#include <vmm_types.h>
#include <vmm_mutex.h>
#include <vmm_completion.h>
#include <libs/list.h>

#define VMM_VSPI_IPRIORITY			0

struct vmm_thread;
struct vmm_emudev;
struct vmm_vspihost;
struct vmm_vspislave;

/** Representation of a virtual spi slave */
struct vmm_vspislave {
	struct vmm_emudev *edev;
	struct vmm_vspihost *vsh;
	char name[VMM_FIELD_NAME_SIZE];
	u32 chip_select;
	u32 (*xfer) (struct vmm_vspislave *vss, u32 data, void *priv);
	void *priv;
};

/** Representation of a virtual spi host */
struct vmm_vspihost {
	struct dlist head;
	struct vmm_emudev *edev;
	char name[VMM_FIELD_NAME_SIZE];

	void (*xfer) (struct vmm_vspihost *vsh, void *priv);
	struct vmm_completion xfer_avail;
	struct vmm_thread *xfer_worker;

	u32 chip_select_count;

	struct vmm_mutex slaves_lock;
	struct vmm_vspislave **slaves;

	void *priv;
};

/** Get virtual spi host for virtual spi slave */
struct vmm_vspihost *vmm_vspislave_get_host(struct vmm_vspislave *vss);

/** Get name of virtual spi slave */
const char *vmm_vspislave_get_name(struct vmm_vspislave *vss);

/** Get chip select of virtual spi slave */
u32 vmm_vspislave_get_chip_select(struct vmm_vspislave *vss);

/** Create a virtual spi slave */
struct vmm_vspislave *vmm_vspislave_create(struct vmm_emudev *edev,
			u32 chip_select,
			u32 (*xfer) (struct vmm_vspislave *, u32, void *),
			void *priv);

/** Destroy a virtual spi slave */
int vmm_vspislave_destroy(struct vmm_vspislave *vss);

/** Transfer data to a virtual spi slave of given virtual spi host */
u32 vmm_vspihost_xfer_data(struct vmm_vspihost *vsh,
			   u32 chip_select, u32 data);

/** Schedule transfer for given virtual spi host */
void vmm_vspihost_schedule_xfer(struct vmm_vspihost *vsh);

/** Get name of virtual spi host */
const char *vmm_vspihost_get_name(struct vmm_vspihost *vsh);

/** Get number of chip selects for virtual spi host */
u32 vmm_vspihost_get_chip_select_count(struct vmm_vspihost *vsh);

/** Iterate over each virtual spi slave of given virtual spi host */
int vmm_vspihost_iterate_slaves(struct vmm_vspihost *vsh, void *data,
	int (*fn)(struct vmm_vspihost *, struct vmm_vspislave *, void *));

/** Create a virtual spi host */
struct vmm_vspihost *vmm_vspihost_create(const char *name_prefix,
				struct vmm_emudev *edev,
				void (*xfer) (struct vmm_vspihost *, void *),
				u32 chip_select_count, void *priv);

/** Destroy a virtual spi host */
int vmm_vspihost_destroy(struct vmm_vspihost *vsh);

/** Find virtual spi host for given emulated device */
struct vmm_vspihost *vmm_vspihost_find(struct vmm_emudev *edev);

/** Iterate over each virtual spi host */
int vmm_vspihost_iterate(struct vmm_vspihost *start, void *data,
			 int (*fn)(struct vmm_vspihost *, void *));

/** Count of available virtual spi hosts */
u32 vmm_vspihost_count(void);

#endif
