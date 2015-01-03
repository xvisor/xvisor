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
# @author Himanshu Chauhan (hschauhan@nulltrace.org)
# @brief list of arch specific PCI related files
# */

board-common-objs-$(CONFIG_PCI) += pci/pci_board.o

pci_board-y += i386.o
pci_board-y += init.o
pci_board-$(CONFIG_PCI_BIOS) += pcbios.o
pci_board-$(CONFIG_PCI_FIXUP) += fixup.o
pci_board-$(CONFIG_PCI_DIRECT) += direct.o
pci_board-y += legacy.o
pci_board-y += irq.o
pci_board-y += common.o
pci_board-y += early.o
pci_board-y += bus_numa.o

%/pci_board.o: $(foreach obj,$(pci_board-y),%/$(obj))
	$(call merge_objs,$@,$^)

%/pci_board.dep: $(foreach dep,$(pci_board-y:.o=.dep),%/$(dep))
	$(call merge_deps,$@,$^)
