#ifndef __ARCH_PCI_H
#define __ARCH_PCI_H

struct pci_bus;
struct pci_dev;
struct pci_host_bridge;
struct resource;

extern physical_addr_t pci_io_start;
extern physical_addr_t pci_mem_start;

/* Prepare PCI root bridge
 * NOTE: Architecture-specific versions may override this (weak)
 */
int pcibios_root_bridge_prepare(struct pci_host_bridge *bridge);

/* Achitecture specific fixups for PCI bus */
void pcibios_fixup_bus(struct pci_bus *b);

/* Check resource needs of PCI bus
 * NOTE: Architecture-specific versions may override this (weak)
 */
void pcibios_resource_survey_bus(struct pci_bus *bus);

/* Add PCI bus
 * NOTE: Architecture-specific versions may override this (weak)
 */
void pcibios_add_bus(struct pci_bus *bus);

/* Remove PCI bus
 * NOTE: Architecture-specific versions may override this (weak)
 */
void pcibios_remove_bus(struct pci_bus *bus);

/* Adjust window alignent of PCI bus
 * NOTE: Architecture-specific versions may override this (weak)
 */
resource_size_t pcibios_window_alignment(struct pci_bus *bus,
					 unsigned long type);

/* Set the PCI device as master
 * NOTE: Architecture-specific versions may override this (weak)
 */
void pcibios_set_master(struct pci_dev *dev);

/* Reset secondary PCI bus
 * NOTE: Architecture-specific versions may override this (weak)
 */
void pcibios_reset_secondary_bus(struct pci_dev *dev);

/* Get firmware address for given PCI device
 * NOTE: Architecture-specific versions may override this (weak)
 */
resource_size_t pcibios_retrieve_fw_addr(struct pci_dev *dev, int idx);

/* Update PCIe device reset state
 * NOTE: Architecture-specific versions may override this (weak)
 */
int pcibios_set_pcie_reset_state(struct pci_dev *dev,
				 enum pcie_reset_state state);

/* Add PCI device
 * NOTE: Architecture-specific versions may override this (weak)
 */
int pcibios_add_device(struct pci_dev *dev);

/* Release PCI device
 * NOTE: Architecture-specific versions may override this (weak)
 */
void pcibios_release_device(struct pci_dev *dev);

/* Enable PCI device
 * NOTE: Architecture-specific versions may override this (weak)
 */
int pcibios_enable_device(struct pci_dev *dev, int mask);

/* Disable PCI device
 * NOTE: Architecture-specific versions may override this (weak)
 */
void pcibios_disable_device(struct pci_dev *dev);

/* Penalize legacy ISA irq
 * NOTE: Architecture-specific versions may override this (weak)
 */
void pcibios_penalize_isa_irq(int irq, int active);

/* Process "pci=" early params
 * NOTE: Architecture-specific versions may override this (weak)
 */
char *pcibios_setup(char *str);

/* Legacy ISA devices required realigning of resources */
resource_size_t pcibios_align_resource(void *, const struct resource *,
				       resource_size_t,
				       resource_size_t);

/* Check if the kernel should re-assign all PCI bus numbers */
unsigned int pcibios_assign_all_busses(void);

#endif /* __ARCH_PCI_H */
