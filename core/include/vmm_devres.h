/**
 * Copyright (c) 2014 Anup Patel.
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
 * @file vmm_devres.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Device driver resource managment header
 */

#ifndef __VMM_DEVRES_H_
#define __VMM_DEVRES_H_

#include <vmm_types.h>

struct vmm_device;

/** Device resource match and release functions */
typedef void (*vmm_dr_release_t)(struct vmm_device *dev, void *res);
typedef int (*vmm_dr_match_t)(struct vmm_device *dev, void *res,
			      void *match_data);

/** Allocate device resource data of given size */
void *vmm_devres_alloc(vmm_dr_release_t release, size_t size);

/** Iterate over each device resource */
void vmm_devres_for_each_res(struct vmm_device *dev, vmm_dr_release_t release,
			     vmm_dr_match_t match, void *match_data,
			     void (*fn)(struct vmm_device *, void *, void *),
			     void *data);

/** Free device resource data */
void vmm_devres_free(void *res);

/** Register device resource */
void vmm_devres_add(struct vmm_device *dev, void *res);

/** Find device resource */
void *vmm_devres_find(struct vmm_device *dev, vmm_dr_release_t release,
		      vmm_dr_match_t match, void *match_data);

/** Find devres, if non-existent, add one atomically */
void *vmm_devres_get(struct vmm_device *dev, void *new_res,
		     vmm_dr_match_t match, void *match_data);

/** Find a device resource and remove it but don't free it. */
void *vmm_devres_remove(struct vmm_device *dev, vmm_dr_release_t release,
			vmm_dr_match_t match, void *match_data);

/** Find a device resource and destroy it, without calling release */
int vmm_devres_destroy(struct vmm_device *dev, vmm_dr_release_t release,
		       vmm_dr_match_t match, void *match_data);

/** Find a device resource and destroy it, calling release */
int vmm_devres_release(struct vmm_device *dev, vmm_dr_release_t release,
		       vmm_dr_match_t match, void *match_data);

/** Release all managed resources */
int vmm_devres_release_all(struct vmm_device *dev);

/** Resource-managed malloc */
void *vmm_devm_malloc(struct vmm_device *dev, size_t size);

/** Resource-managed zalloc */
void *vmm_devm_zalloc(struct vmm_device *dev, size_t size);

/** Resource-managed malloc array */
void *vmm_devm_malloc_array(struct vmm_device *dev, size_t n, size_t size);

/** Resource-managed calloc */
void *vmm_devm_calloc(struct vmm_device *dev, size_t n, size_t size);

/** Allocate resource managed space and copy an existing string into that. */
char *vmm_devm_strdup(struct vmm_device *dev, const char *s);

/** Resource-managed free */
void vmm_devm_free(struct vmm_device *dev, void *p);

#endif /* __VMM_DEVRES_H_ */
