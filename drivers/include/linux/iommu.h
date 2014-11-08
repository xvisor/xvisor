#ifndef _LINUX_IOMMU_H
#define _LINUX_IOMMU_H

#include <vmm_iommu.h>

#include <linux/err.h>
#include <linux/errno.h>
#include <linux/types.h>

#define IOMMU_READ			VMM_IOMMU_READ
#define IOMMU_WRITE			VMM_IOMMU_WRITE
#define IOMMU_CACHE			VMM_IOMMU_CACHE
#define IOMMU_EXEC			VMM_IOMMU_EXEC

#define iommu_ops			vmm_iommu_ops
#define iommu_group			vmm_iommu_group
#define iommu_domain			vmm_iommu_domain

#define IOMMU_FAULT_READ		VMM_IOMMU_FAULT_READ
#define IOMMU_FAULT_WRITE		VMM_IOMMU_FAULT_WRITE

#define iommu_fault_handler_t		vmm_iommu_fault_handler_t
#define iommu_domain_geometry		vmm_iommu_domain_geometry

#define VMM_IOMMU_CAP_CACHE_COHERENCY	VMM_IOMMU_CAP_CACHE_COHERENCY
#define VMM_IOMMU_CAP_INTR_REMAP	VMM_IOMMU_CAP_INTR_REMAP

#define iommu_attr			vmm_iommu_attr
#define DOMAIN_ATTR_GEOMETRY		VMM_DOMAIN_ATTR_GEOMETRY
#define DOMAIN_ATTR_PAGING		VMM_DOMAIN_ATTR_PAGING
#define DOMAIN_ATTR_WINDOWS		VMM_DOMAIN_ATTR_WINDOWS
#define DOMAIN_ATTR_FSL_PAMU_STASH	VMM_DOMAIN_ATTR_FSL_PAMU_STASH
#define DOMAIN_ATTR_FSL_PAMU_ENABLE	VMM_DOMAIN_ATTR_FSL_PAMU_ENABLE
#define DOMAIN_ATTR_FSL_PAMUV1		VMM_DOMAIN_ATTR_FSL_PAMUV1
#define DOMAIN_ATTR_MAX			VMM_DOMAIN_ATTR_MAX

#define IOMMU_GROUP_NOTIFY_ADD_DEVICE	VMM_IOMMU_GROUP_NOTIFY_ADD_DEVICE
#define IOMMU_GROUP_NOTIFY_DEL_DEVICE	VMM_IOMMU_GROUP_NOTIFY_DEL_DEVICE
#define IOMMU_GROUP_NOTIFY_BIND_DRIVER	VMM_IOMMU_GROUP_NOTIFY_BIND_DRIVER
#define IOMMU_GROUP_NOTIFY_BOUND_DRIVER	VMM_IOMMU_GROUP_NOTIFY_BOUND_DRIVER
#define IOMMU_GROUP_NOTIFY_UNBIND_DRIVER	\
					VMM_IOMMU_GROUP_NOTIFY_UNBIND_DRIVER
#define IOMMU_GROUP_NOTIFY_UNBOUND_DRIVER	\
					VMM_IOMMU_GROUP_NOTIFY_UNBOUND_DRIVER

#define bus_set_iommu(bus, ops)		vmm_bus_set_iommu(bus, ops)
#define iommu_present(bus)		vmm_iommu_present(bus)
#define iommu_domain_alloc(bus)		vmm_iommu_domain_alloc(bus)
#define iommu_group_get_by_id(id)	vmm_iommu_group_get_by_id(id)
#define iommu_domain_free(domain)	vmm_iommu_domain_free(domain)
#define iommu_attach_device(domain, dev)	\
					vmm_iommu_attach_device(domain, dev)
#define iommu_detach_device(domain, dev)	\
					vmm_iommu_detach_device(domain, dev)
#define iommu_map(domain, iova, paddr, size, prot)	\
					vmm_iommu_map(domain, iova, paddr, size, prot)
#define iommu_unmap(domain, iova, size)		\
					vmm_iommu_unmap(domain, iova, size)
#define iommu_iova_to_phys(domain, iova)	\
					vmm_iommu_iova_to_phys(domain, iova)
#define iommu_domain_has_cap(domain, cap)	\
					vmm_iommu_domain_has_cap(domain, cap)
#define iommu_set_fault_handler(domain, handler, token)	\
				vmm_iommu_set_fault_handler(domain, handler, token)

#define iommu_attach_group(domain, group)	\
					vmm_iommu_attach_group(domain, group)
#define iommu_detach_group(domain, group)	\
					vmm_iommu_detach_group(domain, group)
#define iommu_group_alloc()		vmm_iommu_group_alloc()
#define iommu_group_get_iommudata(group)	\
					vmm_iommu_group_get_iommudata(group)
#define iommu_group_set_iommudata(group, iommu_data, release)	\
			vmm_iommu_group_set_iommudata(group, iommu_data, release)
#define iommu_group_set_name(group, name)	\
					vmm_iommu_group_set_name(group, name)
#define iommu_group_add_device(group, dev)	\
					vmm_iommu_group_add_device(group, dev)
#define iommu_group_remove_device(dev)	vmm_iommu_group_remove_device(dev)
#define iommu_group_for_each_dev(group, data, fn)	\
				vmm_iommu_group_for_each_dev(group, data, fn)
#define iommu_group_get(dev)		vmm_iommu_group_get(dev)
#define iommu_group_put(group)		vmm_iommu_group_put(group)
#define iommu_group_register_notifier(group, nb)	\
				vmm_iommu_group_register_notifier(group, nb)
#define iommu_group_unregister_notifier(group, nb)	\
				vmm_iommu_group_unregister_notifier(group, nb)
#define iommu_group_id(group)		vmm_iommu_group_id(group)

#define iommu_domain_get_attr(domain, attr, data)	\
				vmm_iommu_domain_get_attr(domain, attr, data)
#define iommu_domain_set_attr(domain, attr, data)	\
				vmm_iommu_domain_set_attr(domain, attr, data)

#define iommu_domain_window_enable(domain, wnd_nr, offset, size, prot)	\
	vmm_iommu_domain_window_enable(domain, wnd_nr, offset, size, prot)
#define iommu_domain_window_disable(domain, wnd_nr)	\
			vmm_iommu_domain_window_disable(domain, wnd_nr)

#define report_iommu_fault(domain, dev, iova, flags)	\
			vmm_report_iommu_fault(domain, dev, iova, flags)

#endif /* _LINUX_IOMMU_H */
