#/**
# Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWide.
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
# @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
# @brief list of the MTD device objects
# */

drivers-objs-$(CONFIG_MTD_M25P80)+= mtd/devices/m25p80_mod.o

m25p80_mod-y += m25p80.o m25p80_chardev.o
m25p80_mod-$(CONFIG_MTD_M25P80_BLOCKDEV) += m25p80_blockdev.o

%/m25p80_mod.o: $(foreach obj,$(m25p80_mod-y),%/$(obj))
	$(call merge_objs,$@,$^)

%/m25p80_mod.dep: $(foreach dep,$(m25p80_mod-y:.o=.dep),%/$(dep))
	$(call merge_deps,$@,$^)

