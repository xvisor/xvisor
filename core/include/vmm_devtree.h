/**
 * Copyright (c) 2010 Anup Patel.
 * Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWide.
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
 * @author Anup Patel (anup@brainfault.org)
 * @author Jimmy Durand Wesolowski <jimmy.durand-wesolowski@openwide.fr>
 * @brief Device Tree Header File.
 */
#ifndef __VMM_DEVTREE_H_
#define __VMM_DEVTREE_H_

#include <vmm_limits.h>
#include <vmm_compiler.h>
#include <vmm_types.h>
#include <vmm_spinlocks.h>
#include <arch_atomic.h>
#include <libs/list.h>

#define VMM_DEVTREE_PATH_SEPARATOR		'/'
#define VMM_DEVTREE_PATH_SEPARATOR_STRING	"/"

#define VMM_DEVTREE_MODEL_ATTR_NAME		"model"
#define VMM_DEVTREE_DEVICE_TYPE_ATTR_NAME	"device_type"
#define VMM_DEVTREE_DEVICE_TYPE_VAL_CPU		"cpu"
#define VMM_DEVTREE_DEVICE_TYPE_VAL_GUEST	"guest"
#define VMM_DEVTREE_DEVICE_TYPE_VAL_VCPU	"vcpu"
#define VMM_DEVTREE_DEVICE_TYPE_VAL_RAM		"ram"
#define VMM_DEVTREE_DEVICE_TYPE_VAL_ALLOCED_RAM	"alloced_ram"
#define VMM_DEVTREE_DEVICE_TYPE_VAL_ROM		"rom"
#define VMM_DEVTREE_DEVICE_TYPE_VAL_ALLOCED_ROM	"alloced_rom"
#define VMM_DEVTREE_COMPATIBLE_ATTR_NAME	"compatible"
#define VMM_DEVTREE_CLOCK_FREQ_ATTR_NAME	"clock-frequency"
#define VMM_DEVTREE_CLOCKS_ATTR_NAME		"clocks"
#define VMM_DEVTREE_CLOCK_NAMES_ATTR_NAME	"clock-names"
#define VMM_DEVTREE_CLOCK_OUT_NAMES_ATTR_NAME	"clock-output-names"
#define VMM_DEVTREE_REG_ATTR_NAME		"reg"
#define VMM_DEVTREE_REG_NAMES_ATTR_NAME		"reg-names"
#define VMM_DEVTREE_VIRTUAL_REG_ATTR_NAME	"virtual-reg"
#define VMM_DEVTREE_RANGES_ATTR_NAME		"ranges"
#define VMM_DEVTREE_BIG_ENDIAN_ATTR_NAME	"big-endian"
#define VMM_DEVTREE_NATIVE_ENDIAN_ATTR_NAME	"native-endian"
#define VMM_DEVTREE_ADDR_CELLS_ATTR_NAME	"#address-cells"
#define VMM_DEVTREE_SIZE_CELLS_ATTR_NAME	"#size-cells"
#define VMM_DEVTREE_PHANDLE_ATTR_NAME		"phandle"

#define VMM_DEVTREE_DEBUG_ATTR_NAME		"debug"

#define VMM_DEVTREE_CHOSEN_NODE_NAME		"chosen"
#define VMM_DEVTREE_CONSOLE_ATTR_NAME		"console"
#define VMM_DEVTREE_RTCDEV_ATTR_NAME		"rtcdev"
#define VMM_DEVTREE_BOOTARGS_ATTR_NAME		"bootargs"
#define VMM_DEVTREE_BOOTCMD_ATTR_NAME		"bootcmd"

#define VMM_DEVTREE_ALIASES_NODE_NAME		"aliases"

#define VMM_DEVTREE_VMMINFO_NODE_NAME		"vmm"
#define VMM_DEVTREE_VMMNET_NODE_NAME		"net"
#define VMM_DEVTREE_NETSTACK_NODE_NAME		"hoststack"

#define VMM_DEVTREE_MEMORY_NODE_NAME		"memory"
#define VMM_DEVTREE_MEMORY_PHYS_ADDR_ATTR_NAME	"physical_addr"
#define VMM_DEVTREE_MEMORY_PHYS_SIZE_ATTR_NAME	"physical_size"

#define VMM_DEVTREE_CPUS_NODE_NAME		"cpus"
#define VMM_DEVTREE_INTERRUPTS_ATTR_NAME	"interrupts"
#define VMM_DEVTREE_ENABLE_METHOD_ATTR_NAME	"enable-method"
#define VMM_DEVTREE_CPU_CLEAR_ADDR_ATTR_NAME	"cpu-clear-addr"
#define VMM_DEVTREE_CPU_RELEASE_ADDR_ATTR_NAME	"cpu-release-addr"

#define VMM_DEVTREE_GUESTINFO_NODE_NAME		"guests"
#define VMM_DEVTREE_VCPUS_NODE_NAME		"vcpus"
#define VMM_DEVTREE_ENDIANNESS_ATTR_NAME	"endianness"
#define VMM_DEVTREE_ENDIANNESS_VAL_BIG		"big"
#define VMM_DEVTREE_ENDIANNESS_VAL_LITTLE	"little"
#define VMM_DEVTREE_START_PC_ATTR_NAME		"start_pc"
#define VMM_DEVTREE_PRIORITY_ATTR_NAME		"priority"
#define VMM_DEVTREE_TIME_SLICE_ATTR_NAME	"time_slice"
#define VMM_DEVTREE_DEADLINE_ATTR_NAME		"deadline"
#define VMM_DEVTREE_PERIODICITY_ATTR_NAME	"periodicity"
#define VMM_DEVTREE_ADDRSPACE_NODE_NAME		"aspace"
#define VMM_DEVTREE_GUESTIRQCNT_ATTR_NAME	"guest_irq_count"
#define VMM_DEVTREE_MANIFEST_TYPE_ATTR_NAME	"manifest_type"
#define VMM_DEVTREE_MANIFEST_TYPE_VAL_REAL	"real"
#define VMM_DEVTREE_MANIFEST_TYPE_VAL_VIRTUAL	"virtual"
#define VMM_DEVTREE_MANIFEST_TYPE_VAL_ALIAS	"alias"
#define VMM_DEVTREE_ADDRESS_TYPE_ATTR_NAME	"address_type"
#define VMM_DEVTREE_ADDRESS_TYPE_VAL_MEMORY	"memory"
#define VMM_DEVTREE_ADDRESS_TYPE_VAL_IO		"io"
#define VMM_DEVTREE_GUEST_PHYS_ATTR_NAME	"guest_physical_addr"
#define VMM_DEVTREE_HOST_PHYS_ATTR_NAME		"host_physical_addr"
#define VMM_DEVTREE_ALIAS_PHYS_ATTR_NAME	"alias_physical_addr"
#define VMM_DEVTREE_PHYS_SIZE_ATTR_NAME		"physical_size"
#define VMM_DEVTREE_ALIGN_ORDER_ATTR_NAME	"align_order"
#define VMM_DEVTREE_SWITCH_ATTR_NAME		"switch"
#define VMM_DEVTREE_BLKDEV_ATTR_NAME		"blkdev"
#define VMM_DEVTREE_VCPU_AFFINITY_ATTR_NAME	"affinity"
#define VMM_DEVTREE_VCPU_POWEROFF_ATTR_NAME	"poweroff"
#define VMM_DEVTREE_NO_CHILD_PROBE_ATTR_NAME	"no-child-probe"

enum vmm_devtree_attrypes {
	VMM_DEVTREE_ATTRTYPE_UINT32	= 0,
	VMM_DEVTREE_ATTRTYPE_UINT64	= 1,
	VMM_DEVTREE_ATTRTYPE_VIRTADDR	= 2,
	VMM_DEVTREE_ATTRTYPE_VIRTSIZE	= 3,
	VMM_DEVTREE_ATTRTYPE_PHYSADDR	= 4,
	VMM_DEVTREE_ATTRTYPE_PHYSSIZE	= 5,
	VMM_DEVTREE_ATTRTYPE_STRING	= 6,
	VMM_DEVTREE_ATTRTYPE_BYTEARRAY	= 7,
	VMM_DEVTREE_MAX_ATTRTYPE	= 8
};

struct vmm_devtree_attr {
	struct dlist head;
	char name[VMM_FIELD_SHORT_NAME_SIZE];
	u32 type;
	void *value;
	u32 len;
};

struct vmm_devtree_nodeid {
	char name[VMM_FIELD_SHORT_NAME_SIZE];
	char type[VMM_FIELD_TYPE_SIZE];
	char compatible[VMM_FIELD_COMPAT_SIZE];
	const void *data;
};

#define VMM_DEVTREE_NIDTBL_SIGNATURE	0xDEADF001

struct vmm_devtree_nidtbl_entry {
	u32 signature;
	char subsys[VMM_FIELD_SHORT_NAME_SIZE];
	struct vmm_devtree_nodeid nodeid;
};

#ifndef __VMM_MODULES__

#define VMM_DEVTREE_NIDTBL_ENTRY(nid, _subsys, _name, _type, _compat, _data) \
__nidtbl struct vmm_devtree_nidtbl_entry __##nid = { \
	.signature = VMM_DEVTREE_NIDTBL_SIGNATURE, \
	.subsys = (_subsys), \
	.nodeid.name = (_name), \
	.nodeid.type = (_type), \
	.nodeid.compatible = (_compat), \
	.nodeid.data = (_data), \
}

#else

/**
 * TODO: NodeID table enteries cannot be created from runtime pluggable
 * modules. This will be added in future because vmm_modules needs to be
 * updated to support it.
 */
#define VMM_DEVTREE_NIDTBL_ENTRY(nid, _subsys, _name, _type, _compat, _data)

#endif

struct vmm_devtree_node {
	/* Private fields */
	struct dlist head;
	vmm_rwlock_t attr_lock;
	struct dlist attr_list;
	vmm_rwlock_t child_lock;
	struct dlist child_list;
	atomic_t ref_count;
	/* Public fields */
	char name[VMM_FIELD_SHORT_NAME_SIZE];
	struct vmm_devtree_node *parent;
	void *system_data; /* System data pointer
			      (Arch. specific code can use this to
			       pass inforation to device driver) */
	void *priv; /* Generic Private pointer */
};

#define VMM_MAX_PHANDLE_ARGS		8
struct vmm_devtree_phandle_args {
	struct vmm_devtree_node *np;
	int args_count;
	u32 args[VMM_MAX_PHANDLE_ARGS];
};

/** Check whether given attribute type is literal or literal list
 *  NOTE: literal means 32-bit or 64-bit number
 */
bool vmm_devtree_isliteral(u32 attrtype);

/** Get size of literal corresponding to attribute type */
u32 vmm_devtree_literal_size(u32 attrtype);

/** Estimate type of attribute from its name */
u32 vmm_devtree_estimate_attrtype(const char *name);

/** Get attribute value */
const void *vmm_devtree_attrval(const struct vmm_devtree_node *node,
				const char *attrib);

/** Get length of attribute value */
u32 vmm_devtree_attrlen(const struct vmm_devtree_node *node,
			const char *attrib);

/** Check if a device tree node have any attribute */
bool vmm_devtree_have_attr(const struct vmm_devtree_node *node);

/** Get next attribute of a device tree node */
struct vmm_devtree_attr *vmm_devtree_next_attr(
					const struct vmm_devtree_node *node,
					struct vmm_devtree_attr *current);

/** Itreate over each attribute of a device tree node */
#define vmm_devtree_for_each_attr(attr, node) \
	for (attr = vmm_devtree_next_attr(node, NULL); attr; \
	     attr = vmm_devtree_next_attr(node, attr))

/** Set an attribute for a device tree node */
int vmm_devtree_setattr(struct vmm_devtree_node *node,
			const char *name, void *value,
			u32 type, u32 len, bool value_is_be);

/** Get an attribute from a device tree node */
struct vmm_devtree_attr *vmm_devtree_getattr(
					const struct vmm_devtree_node *node,
					const char *name);

/** Delete an attribute from a device tree node */
int vmm_devtree_delattr(struct vmm_devtree_node *node, const char *name);

/** Read u8 from attribute at particular index */
int vmm_devtree_read_u8_atindex(const struct vmm_devtree_node *node,
			        const char *attrib, u8 *out, int index);

/** Read an array of u8 from attribute */
int vmm_devtree_read_u8_array(const struct vmm_devtree_node *node,
			      const char *attrib, u8 *out, size_t sz);

/** Read u8 from attribute */
static inline int vmm_devtree_read_u8(const struct vmm_devtree_node *node,
				      const char *attrib, u8 *out)
{
	return vmm_devtree_read_u8_array(node, attrib, out, 1);
}

/** Read u16 from attribute at particular index */
int vmm_devtree_read_u16_atindex(const struct vmm_devtree_node *node,
			         const char *attrib, u16 *out, int index);

/** Read an array of u16 from attribute */
int vmm_devtree_read_u16_array(const struct vmm_devtree_node *node,
			       const char *attrib, u16 *out, size_t sz);

/** Read u16 from attribute */
static inline int vmm_devtree_read_u16(const struct vmm_devtree_node *node,
				       const char *attrib, u16 *out)
{
	return vmm_devtree_read_u16_array(node, attrib, out, 1);
}

/** Read u32 from attribute at particular index */
int vmm_devtree_read_u32_atindex(const struct vmm_devtree_node *node,
			         const char *attrib, u32 *out, int index);

/** Read an array of u32 from attribute */
int vmm_devtree_read_u32_array(const struct vmm_devtree_node *node,
			       const char *attrib, u32 *out, size_t sz);

/** Read u32 from attribute */
static inline int vmm_devtree_read_u32(const struct vmm_devtree_node *node,
				       const char *attrib, u32 *out)
{
	return vmm_devtree_read_u32_array(node, attrib, out, 1);
}

/** Read u64 from attribute at particular index */
int vmm_devtree_read_u64_atindex(const struct vmm_devtree_node *node,
			         const char *attrib, u64 *out, int index);

/** Read an array of u64 from attribute */
int vmm_devtree_read_u64_array(const struct vmm_devtree_node *node,
			       const char *attrib, u64 *out, size_t sz);

/** Read u64 from attribute */
static inline int vmm_devtree_read_u64(const struct vmm_devtree_node *node,
				       const char *attrib, u64 *out)
{
	return vmm_devtree_read_u64_array(node, attrib, out, 1);
}

/** Read physical address from attribute at particular index */
int vmm_devtree_read_physaddr_atindex(const struct vmm_devtree_node *node,
				      const char *attrib, physical_addr_t *out,
				      int index);

/** Read an array of physical address from attribute */
int vmm_devtree_read_physaddr_array(const struct vmm_devtree_node *node,
				    const char *attrib, physical_addr_t *out,
				    size_t sz);

/** Read physical address from attribute */
static inline int vmm_devtree_read_physaddr(const struct vmm_devtree_node *node,
					    const char *attrib,
					    physical_addr_t *out)
{
	return vmm_devtree_read_physaddr_array(node, attrib, out, 1);
}

/** Read physical size from attribute at particular index */
int vmm_devtree_read_physsize_atindex(const struct vmm_devtree_node *node,
				      const char *attrib, physical_size_t *out,
				      int index);

/** Read an array of physical size from attribute */
int vmm_devtree_read_physsize_array(const struct vmm_devtree_node *node,
				    const char *attrib, physical_size_t *out,
				    size_t sz);

/** Read physical size from attribute */
static inline int vmm_devtree_read_physsize(const struct vmm_devtree_node *node,
					    const char *attrib,
					    physical_size_t *out)
{
	return vmm_devtree_read_physsize_array(node, attrib, out, 1);
}

/** Read virtual address from attribute at particular index */
int vmm_devtree_read_virtaddr_atindex(const struct vmm_devtree_node *node,
				      const char *attrib, virtual_addr_t *out,
				      int index);

/** Read an array of virtual address from attribute */
int vmm_devtree_read_virtaddr_array(const struct vmm_devtree_node *node,
				    const char *attrib, virtual_addr_t *out,
				    size_t sz);

/** Read virtual address from attribute */
static inline int vmm_devtree_read_virtaddr(const struct vmm_devtree_node *node,
					    const char *attrib,
					    virtual_addr_t *out)
{
	return vmm_devtree_read_virtaddr_array(node, attrib, out, 1);
}

/** Read virtual size from attribute at particular index */
int vmm_devtree_read_virtsize_atindex(const struct vmm_devtree_node *node,
				      const char *attrib, virtual_size_t *out,
				      int index);

/** Read an array of virtual size from attribute */
int vmm_devtree_read_virtsize_array(const struct vmm_devtree_node *node,
				    const char *attrib, virtual_size_t *out,
				    size_t sz);

/** Read virtual size from attribute */
static inline int vmm_devtree_read_virtsize(const struct vmm_devtree_node *node,
					    const char *attrib,
					    virtual_size_t *out)
{
	return vmm_devtree_read_virtsize_array(node, attrib, out, 1);
}

/** Read string from attribute */
int vmm_devtree_read_string(const struct vmm_devtree_node *node,
			    const char *attrib, const char **out);

/** Find string in a list and return index
 *
 *  This function searches a string list property and returns the index
 *  of a specific string value.
 */
int vmm_devtree_match_string(struct vmm_devtree_node *node,
			     const char *attrib, const char *string);

/** Find and return the number of strings from a multiple strings property.
 *
 *  Search for a attribute in a device tree node and retrieve the number
 *  of null terminated string contain in it. Returns the number of strings
 *  on success, VMM_EINVALID if the property does not exist, VMM_ENODATA
 *  if property does not have a value, and VMM_EILSEQ if the string is not
 *  null-terminated within the length of the property data.
 */
int vmm_devtree_count_strings(struct vmm_devtree_node *node,
			      const char *attrib);

/** Retrive string in a list based on index
 *
 *  Returns size of string (0 <=) upon success and VMM_Exxxx (< 0)
 *  upon failure
 */
int vmm_devtree_string_index(struct vmm_devtree_node *node,
			     const char *attrib, int index, const char **out);

/** Retrive the next u32 value.
 *
 *  Returns NULL when u32 is not available.
 */
const u32 *vmm_devtree_next_u32(struct vmm_devtree_attr *attr,
				const u32 *cur, u32 *val);

/** Retrive the next string.
 *
 *  Returns NULL when string is not available.
 */
const char *vmm_devtree_next_string(struct vmm_devtree_attr *attr,
				    const char *cur);

#define vmm_devtree_for_each_u32(np, attrname, attr, p, u)	\
	for (attr = vmm_devtree_getattr(np, attrname),		\
		p = vmm_devtree_next_u32(attr, NULL, &u);	\
		p;						\
		p = vmm_devtree_next_u32(attr, p, &u))

#define vmm_devtree_for_each_string(np, attrname, attr, s)      \
        for (attr = vmm_devtree_getattr(np, attrname),		\
                s = vmm_devtree_next_string(attr, NULL);	\
                s;						\
                s = vmm_devtree_next_string(attr, s))

/** Create a path string for a given node */
int vmm_devtree_getpath(char *out, size_t out_len,
			const struct vmm_devtree_node *node);

/** Get child node below a given node
 *  NOTE: The returned node will have increased refrence count
 */
struct vmm_devtree_node *vmm_devtree_getchild(struct vmm_devtree_node *node,
					      const char *path);

/** Get node corresponding to a path string
 *  NOTE: If path == NULL then root node will be returned
 *  NOTE: The returned node will have increased refrence count
 */
struct vmm_devtree_node *vmm_devtree_getnode(const char *path);

/** Match a node with nodeid table
 *  Returns NULL if node does not match otherwise nodeid table entry
 */
const struct vmm_devtree_nodeid *vmm_devtree_match_node(
				const struct vmm_devtree_nodeid *matches,
				const struct vmm_devtree_node *node);

/** Find node matching nodeid table starting from given node
 *  NOTE: If node == NULL then node == root
 *  NOTE: The returned node will have increased refrence count
 */
struct vmm_devtree_node *vmm_devtree_find_matching(
				struct vmm_devtree_node *node,
				const struct vmm_devtree_nodeid *matches);

/** Iterate over all matching nodes
 *  NOTE: If node == NULL then node == root
 */
void vmm_devtree_iterate_matching(struct vmm_devtree_node *node,
				  const struct vmm_devtree_nodeid *matches,
				  void (*found)(struct vmm_devtree_node *node,
				      const struct vmm_devtree_nodeid *match,
				      void *data),
				  void *found_data);

/** Find compatible node starting from given node
 *  NOTE: If node == NULL then node == root
 *  NOTE: The returned node will have increased refrence count
 */
struct vmm_devtree_node *vmm_devtree_find_compatible(
				struct vmm_devtree_node *node,
				const char *device_type,
				const char *compatible);

/** Check if node is compatible to given compatibility string */
bool vmm_devtree_is_compatible(const struct vmm_devtree_node *node,
			       const char *compatible);

/** Find a node with given phandle value
 *  NOTE: This is based on 'phandle' attributes of device tree node
 *  NOTE: The returned node will have increased refrence count
 */
struct vmm_devtree_node *vmm_devtree_find_node_by_phandle(u32 phandle);

/** Resolve a phandle property to a vmm_devtree_node pointer
 *  NOTE: The returned node will have increased refrence count
 */
struct vmm_devtree_node *vmm_devtree_parse_phandle(
					const struct vmm_devtree_node *node,
					const char *phandle_name,
					int index);

/** Find a node pointed by phandle in a list
 *
 *  This function is useful to parse lists of phandles and their arguments.
 *  Returns VMM_OK on success and fills out (i.e. args), on error returns
 *  appropriate errno value.
 *
 *  Example:
 *
 *  phandle1: node1 {
 *  	#list-cells = <2>;
 *  }
 *
 *  phandle2: node2 {
 * 	#list-cells = <1>;
 *  }
 *
 *  node3 {
 * 	list = <&phandle1 1 2 &phandle2 3>;
 *  }
 *
 *  To get a device_node of the `node2' node you may call this:
 *  vmm_devtree_parse_phandle_with_args(node3, "list", "#list-cells", 1, &out);
 *
 *  NOTE: The returned nodes will have increased refrence count
 */
int vmm_devtree_parse_phandle_with_args(const struct vmm_devtree_node *node,
					const char *list_name,
					const char *cells_name,
					int index,
					struct vmm_devtree_phandle_args *out);

/**
 * Find a node pointed by phandle in a list
 *
 * This function is useful to parse lists of phandles and their arguments.
 * Returns 0 on success and fills out_args, on error returns appropriate
 * errno value.
 *
 * Example:
 *
 * phandle1: node1 {
 * }
 *
 * phandle2: node2 {
 * }
 *
 * node3 {
 * 	list = <&phandle1 0 2 &phandle2 2 3>;
 * }
 *
 * To get a device_node of the `node2' node you may call this:
 * vmm_devtree_parse_phandle_with_fixed_args(node3, "list", 2, 1, &args);
 *
 *  NOTE: The returned nodes will have increased refrence count
 */
int vmm_devtree_parse_phandle_with_fixed_args(
					const struct vmm_devtree_node *node,
					const char *list_name,
					int cells_count,
					int index,
					struct vmm_devtree_phandle_args *out);

/** Find the number of phandles references in a property
 *
 *  Returns the number of phandle + argument tuples within a property. It
 *  is a typical pattern to encode a list of phandle and variable
 *  arguments into a single property. The number of arguments is encoded
 *  by a property in the phandle-target node. For example, a gpios
 *  property would contain a list of GPIO specifies consisting of a
 *  phandle and 1 or more arguments. The number of arguments are
 *  determined by the #gpio-cells property in the node pointed to by the
 *  phandle.
 */
int vmm_devtree_count_phandle_with_args(const struct vmm_devtree_node *node,
					const char *list_name,
					const char *cells_name);

/** Increase reference count of give node */
void vmm_devtree_ref_node(struct vmm_devtree_node *node);

/** De-refernce a device tree node */
void vmm_devtree_dref_node(struct vmm_devtree_node *node);

/** Check if a device tree node have any child node */
bool vmm_devtree_have_child(const struct vmm_devtree_node *node);

/** Get next child node of a device tree node */
struct vmm_devtree_node *vmm_devtree_next_child(
					const struct vmm_devtree_node *node,
					struct vmm_devtree_node *current);

/** Itreate over each child node of a device tree node
 *  NOTE: If we need to break-out of the loop in-between then
 *  we will need use vmm_devtree_dref_node() on child node of
 *  current iteration just before breaking-out loop.
 */
#define vmm_devtree_for_each_child(child, node) \
	for (child = vmm_devtree_next_child(node, NULL); child; \
	     child = vmm_devtree_next_child(node, child))

/** Find the child node by name for a given parent
 *  @node:  parent node
 *  @name:  child name to look for.
 *
 *  This function looks for child node for given matching name
 *
 *  Returns a node pointer if found, with refcount incremented, use
 *  vmm_devtree_dref_node() on it when done.
 *  Returns NULL if node is not found.
 */
struct vmm_devtree_node *vmm_devtree_get_child_by_name(
					struct vmm_devtree_node *node,
					const char *name);

/** Add new node to device tree with given name
 *  NOTE: This function allows parent == NULL to enable creation of
 *  root node but only once.
 *  NOTE: Once root node is created, subsequent calls to this function
 *  with parent == NULL will add nodes under root node.
 */
struct vmm_devtree_node *vmm_devtree_addnode(struct vmm_devtree_node *parent,
					     const char *name);

/** Copy a node to another location in device tree */
int vmm_devtree_copynode(struct vmm_devtree_node *parent,
			 const char *name,
			 struct vmm_devtree_node *src);

/** Delete a node from device tree */
int vmm_devtree_delnode(struct vmm_devtree_node *node);

/** Get device clock-frequency
 *  NOTE: This is based on 'clock-frequency' attribute of device tree node
 *  NOTE: This API if for hard-coding clock frequency in device tree node
 *  and it does not use clk_xxxx() APIs
 */
int vmm_devtree_clock_frequency(struct vmm_devtree_node *node,
				u32 *clock_freq);

/** Get device irq number
 *  NOTE: This is based on 'irq' attribute of device tree node
 */
int vmm_devtree_irq_get(struct vmm_devtree_node *node,
		        u32 *irq, int index);

/** Get count of device irqs
 *  NOTE: This is based on 'irq' attribute of device tree node
 */
u32 vmm_devtree_irq_count(struct vmm_devtree_node *node);

/**
 * Given a device tree node, find its interrupt parent node
 * @child: pointer to device node
 *
 * Returns a pointer to the interrupt parent node, or NULL if
 * the interrupt parent could not be determined.
 */
struct vmm_devtree_node *vmm_devtree_irq_find_parent(
				struct vmm_devtree_node *child);

/**
 * Resolve an interrupt for a device
 * @device: the device whose interrupt is to be resolved
 * @index: index of the interrupt to resolve
 * @out_irq: structure filled by this function
 *
 * This function resolves an interrupt for a node by walking the interrupt tree,
 * finding which interrupt controller node it is attached to, and returning the
 * interrupt specifier that can be used to retrieve an Xvisor IRQ number.
 */
int vmm_devtree_irq_parse_one(struct vmm_devtree_node *device, int index,
			      struct vmm_devtree_phandle_args *out_irq);

/**
 * Parse and map an interrupt into Xvisor space
 * @dev: Device node of the device whose interrupt is to be mapped
 * @index: Index of the interrupt to map
 */
unsigned int vmm_devtree_irq_parse_map(struct vmm_devtree_node *dev,
					int index);

/** vmm_devtree_is_available - check if a device is available for use
 *  @node: Node to check for availability
 *
 *  Returns TRUE if the status property is absent or set to "okay" or "ok",
 *  FALSE otherwise
 */
bool vmm_devtree_is_available(const struct vmm_devtree_node *node);

/** vmm_devtree_alias_get_id - Get alias id for the given device_node
 * @np:         Pointer to the given device_node
 * @stem:       Alias stem of the given device_node
 *
 * The function scans all the properties of 'aliases' node to get the alias id
 * for the given device_node and alias stem.  It returns the alias id if found.
 */
int vmm_devtree_alias_get_id(struct vmm_devtree_node *node,
			     const char *stem);

/** Get physical size of device registers
 *  NOTE: This is based on 'reg' and 'virtual-reg' attributes
 *  of device tree node
 */
int vmm_devtree_regsize(struct vmm_devtree_node *node,
		        physical_size_t *size, int regset);

/** Get physical address of device registers
 *  NOTE: This is based on 'reg' and 'virtual-reg' attributes
 *  of device tree node
 */
int vmm_devtree_regaddr(struct vmm_devtree_node *node,
		        physical_addr_t *addr, int regset);

/** Map device registers to virtual address
 *  NOTE: This is based on 'reg' and 'virtual-reg' attributes
 *  of device tree node
 */
int vmm_devtree_regmap(struct vmm_devtree_node *node,
		       virtual_addr_t *addr, int regset);

/** Unmap device registers from virtual address
 *  NOTE: This is based on 'reg' and 'virtual-reg' attributes
 *  of device tree node
 */
int vmm_devtree_regunmap(struct vmm_devtree_node *node,
			 virtual_addr_t addr, int regset);

/** Convert regname to regset index
 *  NOTE: This is based on 'reg-names' attribute of device tree node
 */
int vmm_devtree_regname_to_regset(struct vmm_devtree_node *node,
				  const char *regname);

/** Map device registers to virtual address
 *  NOTE: This is based on 'reg' and 'reg-names' attributes
 *  of device tree node
 */
int vmm_devtree_regmap_byname(struct vmm_devtree_node *node,
			      virtual_addr_t *addr, const char *regname);

/** Unmap device registers from virtual address
 *  NOTE: This is based on 'reg' and 'reg-names' attributes
 *  of device tree node
 */
int vmm_devtree_regunmap_byname(struct vmm_devtree_node *node,
				virtual_addr_t addr, const char *regname);

/** Request hostmem resource region for device registers physical
 *  address and Map device registers to a virtual address
 *  NOTE: This is based on 'reg' attribute of device tree node
 */
int vmm_devtree_request_regmap(struct vmm_devtree_node *node,
			       virtual_addr_t *addr, int regset,
			       const char *resname);

/** Unmap device registers virtual address and release hostmem
 *  resource region for device registers
 *  NOTE: This is based on 'reg' attribute of device tree node
 */
int vmm_devtree_regunmap_release(struct vmm_devtree_node *node,
				 virtual_addr_t addr, int regset);

/** Check whether device registers are big endian */
bool vmm_devtree_is_reg_big_endian(struct vmm_devtree_node *node);

/** Count number of enteries in nodeid table */
u32 vmm_devtree_nidtbl_count(void);

/** Get nodeid table entry at given index */
struct vmm_devtree_nidtbl_entry *vmm_devtree_nidtbl_get(int index);

/** Create matches table from nodeid table with given subsys
 *  NOTE: If subsys==NULL then matches table is created from all enteries
 */
const struct vmm_devtree_nodeid *
		vmm_devtree_nidtbl_create_matches(const char *subsys);

/** Destroy matches table created from nodeid table */
void vmm_devtree_nidtbl_destroy_matches(
				const struct vmm_devtree_nodeid *matches);

/** Initialize device tree */
int vmm_devtree_init(void);

#endif /* __VMM_DEVTREE_H_ */
