#ifndef __ARCH_PCI_H
#define __ARCH_PCI_H

struct pci_bus;
struct pci_dev;
struct resource;

void pcibios_fixup_bus(struct pci_bus *b);
void pcibios_resource_survey_bus(struct pci_bus *bus);
void pcibios_add_bus(struct pci_bus *bus);
void pcibios_remove_bus(struct pci_bus *bus);
void pcibios_fixup_bus(struct pci_bus *);
int __mustcheck pcibios_enable_device(struct pci_dev *, int mask);

/* Architecture-specific versions may override this (weak) */
char *pcibios_setup(char *str);

/* Used only when drivers/pci/setup.c is used */
resource_size_t pcibios_align_resource(void *, const struct resource *,
				       resource_size_t,
				       resource_size_t);
void pcibios_update_irq(struct pci_dev *, int irq);
unsigned int pcibios_assign_all_busses(void);

#endif /* __ARCH_PCI_H */
