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
 * @file vmm_devtree.h
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief Device Tree Header File.
 */
#ifndef __VMM_DEVTREE_H_
#define __VMM_DEVTREE_H_

#include <vmm_types.h>
#include <vmm_list.h>

#define VMM_DEVTREE_PATH_SEPRATOR		'/'
#define VMM_DEVTREE_PATH_SEPRATOR_STRING	"/"

#define VMM_DEVTREE_MODEL_ATTR_NAME		"model"
#define VMM_DEVTREE_DEVICE_TYPE_ATTR_NAME	"device_type"
#define VMM_DEVTREE_DEVICE_TYPE_VAL_CPU		"cpu"
#define VMM_DEVTREE_DEVICE_TYPE_VAL_GUEST	"guest"
#define VMM_DEVTREE_DEVICE_TYPE_VAL_VCPU	"vcpu"
#define VMM_DEVTREE_DEVICE_TYPE_VAL_ROM		"rom"
#define VMM_DEVTREE_COMPATIBLE_ATTR_NAME	"compatible"
#define VMM_DEVTREE_CLOCK_FREQ_ATTR_NAME	"clock-frequency"
#define VMM_DEVTREE_REG_ATTR_NAME		"reg"
#define VMM_DEVTREE_VIRTUAL_REG_ATTR_NAME	"virtual-reg"

#define VMM_DEVTREE_VMMINFO_NODE_NAME		"vmm"
#define VMM_DEVTREE_MAX_VCPU_COUNT_ATTR_NAME	"max_vcpu_count"
#define VMM_DEVTREE_MAX_GUEST_COUNT_ATTR_NAME	"max_guest_count"
#define VMM_DEVTREE_TICK_DELAY_NSECS_ATTR_NAME	"tick_delay_nsecs"
#define VMM_DEVTREE_HCORE_TICK_COUNT_ATTR_NAME	"hypercore_tick_count"

#define VMM_DEVTREE_HOSTINFO_NODE_NAME		"host"
#define VMM_DEVTREE_HOST_IRQ_COUNT_ATTR_NAME	"host_irq_count"
#define VMM_DEVTREE_CPUS_NODE_NAME		"cpus"
#define VMM_DEVTREE_CPU_FREQ_MHZ_ATTR_NAME	"cpu_freq_mhz"

#define VMM_DEVTREE_GUESTINFO_NODE_NAME		"guests"
#define VMM_DEVTREE_VCPUS_NODE_NAME		"vcpus"
#define VMM_DEVTREE_START_PC_ATTR_NAME		"start_pc"
#define VMM_DEVTREE_TICK_COUNT_ATTR_NAME	"tick_count"
#define VMM_DEVTREE_BOOTPG_ADDR_ATTR_NAME	"bootpg_addr"
#define VMM_DEVTREE_BOOTPG_SIZE_ATTR_NAME	"bootpg_size"
#define VMM_DEVTREE_ADDRSPACE_NODE_NAME		"aspace"
#define VMM_DEVTREE_MANIFEST_TYPE_ATTR_NAME	"manifest_type"
#define VMM_DEVTREE_MANIFEST_TYPE_VAL_REAL	"real"
#define VMM_DEVTREE_MANIFEST_TYPE_VAL_VIRTUAL	"virtual"
#define VMM_DEVTREE_ADDRESS_TYPE_ATTR_NAME	"address_type"
#define VMM_DEVTREE_ADDRESS_TYPE_VAL_MEMORY	"memory"
#define VMM_DEVTREE_ADDRESS_TYPE_VAL_IO		"io"
#define VMM_DEVTREE_GUEST_PHYS_ATTR_NAME	"guest_physical_addr"
#define VMM_DEVTREE_HOST_PHYS_ATTR_NAME		"host_physical_addr"
#define VMM_DEVTREE_PHYS_SIZE_ATTR_NAME		"physical_size"
#define VMM_DEVTREE_HOST_VIRT_START_ATTR_NAME	"virt-addr-start"
#define VMM_DEVTREE_HOST_VIRT_SIZE_ATTR_NAME	"virt-addr-size"

enum vmm_devtree_nodetypes {
	VMM_DEVTREE_NODETYPE_UNKNOWN = 0,
	VMM_DEVTREE_NODETYPE_DEVICE = 1,
	VMM_DEVTREE_NODETYPE_EDEVICE = 2,
	VMM_DEVTREE_NODETYPE_MAXTYPES = 3
};

struct vmm_devtree_attr {
	struct dlist head;
	char *name;
	char *value;
	u32 len;
};

typedef struct vmm_devtree_attr vmm_devtree_attr_t;

struct vmm_devtree_node {
	struct dlist head;
	char *name;
	u32 type;
	void *priv;
	struct vmm_devtree_node *parent;
	struct dlist attr_list;
	struct dlist child_list;
};

typedef struct vmm_devtree_node vmm_devtree_node_t;

struct vmm_devtree_ctrl {
	char *str_buf;
	size_t str_buf_size;
	vmm_devtree_node_t *root;
};

typedef struct vmm_devtree_ctrl vmm_devtree_ctrl_t;

/** Get attribute value */
const char *vmm_devtree_attrval(vmm_devtree_node_t * node, const char *attrib);

/** Get lenght of attribute value */
u32 vmm_devtree_attrlen(vmm_devtree_node_t * node, const char *attrib);

/** Create a path string for a given node */
int vmm_devtree_getpath(char *out, vmm_devtree_node_t * node);

/** Get node corresponding to a path string */
vmm_devtree_node_t *vmm_devtree_getnode(const char *path);

/** Get child node below a given node */
vmm_devtree_node_t *vmm_devtree_getchildnode(vmm_devtree_node_t * node,
					     const char *path);

/** Get the root node */
vmm_devtree_node_t *vmm_devtree_rootnode(void);

/** Initialize device tree */
int vmm_devtree_init(void);

#endif /* __VMM_DEVTREE_H_ */
