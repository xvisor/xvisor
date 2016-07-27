/**
 * Copyright (C) 2016 Anup Patel.
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
 * @file vmm_msi.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Generic Host MSI framework interface.
 */

#ifndef __VMM_MSI_H__
#define __VMM_MSI_H__

#include <vmm_types.h>
#include <vmm_devtree.h>
#include <vmm_devdrv.h>
#include <vmm_host_irqdomain.h>
#include <libs/list.h>

struct vmm_msi_msg {
	u32	address_lo;	/* low 32 bits of msi message address */
	u32	address_hi;	/* high 32 bits of msi message address */
	u32	data;		/* 16 bits of msi message data */
};

/**
 * Platform device specific msi descriptor data
 * @msi_priv_data:	Pointer to platform private data
 * @msi_index:		The index of the MSI descriptor for multi MSI
 */
struct vmm_platform_msi_desc {
	struct platform_msi_priv_data	*msi_priv_data;
	u16				msi_index;
};

/**
 * Descriptor structure for MSI based interrupts
 * @list:	List head for management
 * @hirq:	The base interrupt number
 * @nvec_used:	The number of vectors used
 * @dev:	Pointer to the device which uses this descriptor
 * @msg:	The last set MSI message cached for reuse
 *
 * @masked:	[PCI MSI/X] Mask bits
 * @is_msix:	[PCI MSI/X] True if MSI-X
 * @multiple:	[PCI MSI/X] log2 num of messages allocated
 * @multi_cap:	[PCI MSI/X] log2 num of messages supported
 * @maskbit:	[PCI MSI/X] Mask-Pending bit supported?
 * @is_64:	[PCI MSI/X] Address size: 0=32bit 1=64bit
 * @entry_nr:	[PCI MSI/X] Entry which is described by this descriptor
 * @default_irq:[PCI MSI/X] The default pre-assigned non-MSI irq
 * @mask_pos:	[PCI MSI]   Mask register position
 * @mask_base:	[PCI MSI-X] Mask register base address
 * @platform:	[platform]  Platform device specific msi descriptor data
 */
struct vmm_msi_desc {
	/* Shared device/bus type independent data */
	struct dlist			list;
	unsigned int			hirq;
	unsigned int			nvec_used;
	struct vmm_device		*dev;
	struct vmm_msi_msg		msg;

	union {
		/* PCI MSI/X specific data */
		struct {
			u32 masked;
			struct {
				u8	is_msix		: 1;
				u8	multiple	: 3;
				u8	multi_cap	: 3;
				u8	maskbit		: 1;
				u8	is_64		: 1;
				u16	entry_nr;
				unsigned default_irq;
			} msi_attrib;
			union {
				u8	mask_pos;
				void	*mask_base;
			};
		};

		/*
		 * Non PCI variants add their data structure here. New
		 * entries need to use a named structure. We want
		 * proper name spaces for this. The PCI part is
		 * anonymous for now as it would require an immediate
		 * tree wide cleanup.
		 */
		struct vmm_platform_msi_desc platform;
	};
};

#ifndef NUM_MSI_ALLOC_SCRATCHPAD_REGS
# define NUM_MSI_ALLOC_SCRATCHPAD_REGS	2
#endif

/**
 * Default structure for MSI interrupt allocation.
 * @desc:	Pointer to msi descriptor
 * @hwirq:	Associated hw interrupt number in the domain
 * @scratchpad:	Storage for implementation specific scratch data
 *
 * Architectures can provide their own implementation by not including
 * asm-generic/msi.h into their arch specific header file.
 */
typedef struct vmm_msi_alloc_info {
	struct vmm_msi_desc		*desc;
	unsigned int			hwirq;
	union {
		unsigned long		ul;
		void			*ptr;
	} scratchpad[NUM_MSI_ALLOC_SCRATCHPAD_REGS];
} vmm_msi_alloc_info_t;

/* Helpers to hide struct msi_desc implementation details */
#define msi_desc_to_dev(desc)		((desc)->dev)
#define dev_to_msi_list(dev)		(&(dev)->msi_list)
#define first_msi_entry(dev)		\
	list_first_entry(dev_to_msi_list((dev)), struct vmm_msi_desc, list)
#define for_each_msi_entry(desc, dev)	\
	list_for_each_entry((desc), dev_to_msi_list((dev)), list)

struct vmm_msi_domain;
typedef void (*vmm_irq_write_msi_msg_t)(struct vmm_msi_desc *desc,
					struct vmm_msi_msg *msg);

/**
 * MSI domain callbacks
 * @msi_init:		Domain specific init function for MSI interrupts
 * @msi_free:		Domain specific function to free a MSI interrupts
 * @msi_check:		Callback for verification of the domain/info/dev data
 * @msi_prepare:	Prepare the allocation of the interrupts in the domain
 * @msi_finish:		Optional callback to finalize the allocation
 * @set_desc:		Set the msi descriptor for an interrupt
 * @handle_error:	Optional error handler if the allocation fails
 * @compose_msi_msg	Domain specific callback to compose MSI mesage
 * @write_msi_msg	Domain specific callback to write MSI message
 *
 * All of the above callbacks are used by vmm_msi_domain_alloc_irqs()
 * vmm_msi_domain_free_irqs() and related interfaces.
 */
struct vmm_msi_domain_ops {
	int		(*msi_init)(struct vmm_msi_domain *domain,
				    unsigned int hirq, unsigned int hwirq,
				    vmm_msi_alloc_info_t *arg);
	void		(*msi_free)(struct vmm_msi_domain *domain,
				    unsigned int hirq);
	int		(*msi_check)(struct vmm_msi_domain *domain,
				     struct vmm_device *dev);
	int		(*msi_prepare)(struct vmm_msi_domain *domain,
				       struct vmm_device *dev, int nvec,
				       vmm_msi_alloc_info_t *arg);
	void		(*msi_finish)(vmm_msi_alloc_info_t *arg, int retval);
	void		(*set_desc)(vmm_msi_alloc_info_t *arg,
				    struct vmm_msi_desc *desc);
	int		(*handle_error)(struct vmm_msi_domain *domain,
					struct vmm_msi_desc *desc, int error);
	int		(*compose_msi_msg)(struct vmm_msi_domain *domain,
					   unsigned int hirq, unsigned int hwirq,
					   struct vmm_msi_msg *msg);
	void		(*write_msi_msg)(struct vmm_msi_domain *domain,
					 unsigned int hirq, unsigned int hwirq,
					 struct vmm_device *dev,
					 struct vmm_msi_msg *msg);
};

/** Types of MSI domains */
enum vmm_msi_domain_types {
	VMM_MSI_DOMAIN_UNKNOWN = 0,
	VMM_MSI_DOMAIN_PLATFORM,
	VMM_MSI_DOMAIN_PCI,
	VMM_MSI_DOMAIN_MAX,
};

/* Flags for MSI domain */
enum {
	/*
	 * Init non implemented ops callbacks with default MSI domain
	 * callbacks.
	 */
	VMM_MSI_FLAG_USE_DEF_DOM_OPS	= (1 << 0),
	/* Support multiple PCI MSI interrupts */
	VMM_MSI_FLAG_MULTI_PCI_MSI	= (1 << 1),
	/* Support PCI MSIX interrupts */
	VMM_MSI_FLAG_PCI_MSIX		= (1 << 2),
};

/**
 * MSI domain representation
 * @head:	List head for registration
 * @type:	Type of MSI domain
 * @fwnode:	Underlying devtree node
 * @ops:	Pointer to vmm_msi_domain_ops methods.
 * @parent:	Parent vmm_host_irqdomain
 * @flags	Flags specified for MSI domain
 * @data	Domain specific data
 */
struct vmm_msi_domain {
	struct dlist				head;
	enum vmm_msi_domain_types		type;
	struct vmm_devtree_node			*fwnode;
	struct vmm_msi_domain_ops		*ops;
	struct vmm_host_irqdomain		*parent;
	unsigned long				flags;
	void					*data;
};

struct vmm_msi_domain *vmm_msi_create_domain(
					enum vmm_msi_domain_types type,
					struct vmm_devtree_node *fwnode,
					struct vmm_msi_domain_ops *ops,
					struct vmm_host_irqdomain *parent,
					unsigned long flags,
					void *data);

void vmm_msi_destroy_domain(struct vmm_msi_domain *domain);

struct vmm_msi_domain *vmm_msi_find_domain(struct vmm_devtree_node *fwnode,
					   enum vmm_msi_domain_types type);

int vmm_msi_domain_alloc_irqs(struct vmm_msi_domain *domain,
			      struct vmm_device *dev,
			      int nvec);

void vmm_msi_domain_free_irqs(struct vmm_msi_domain *domain,
			      struct vmm_device *dev);

#endif /* __VMM_MSI_H__ */
