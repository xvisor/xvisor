#/**
# Copyright (c) 2014 Himanshu Chauhan.
# All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#
# @file objects.mk
# @author Himanshu Chauhan <hschauhan@nulltrace.org>
# @brief list of PCI framework files
# */

drivers-objs-$(CONFIG_PCI) += pci/pci_core.o

pci_core-y += access.o
pci_core-y += bus.o
pci_core-y += host-bridge.o
pci_core-y += remove.o
pci_core-y += pci.o
pci_core-y += pci-driver.o
pci_core-y += search.o
pci_core-y += rom.o
pci_core-y += setup-res.o
pci_core-y += irq.o
pci_core-y += setup-bus.o
pci_core-y += probe.o
pci_core-$(CONFIG_PCI_GENERIC_IO) += pci_iomap.o

#List of host controllers
pci_core-$(CONFIG_X86_LEGACY_HOST) += host/x86-pci-legacy-host.o

%/pci_core.o: $(foreach obj,$(pci_core-y),%/$(obj))
	$(call merge_objs,$@,$^)

%/pci_core.dep: $(foreach dep,$(pci_core-y:.o=.dep),%/$(dep))
	$(call merge_deps,$@,$^)
