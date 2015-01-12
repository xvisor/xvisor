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
# @brief list of the MTD driver objects
# */

drivers-objs-$(CONFIG_MTD)+= mtd/mtd.o

mtd-y += mtdcore.o mtdconcat.o mtdpart.o mtdchar.o
mtd-$(CONFIG_MTD_BLOCKDEV) += mtdblock.o

%/mtd.o: $(foreach obj,$(mtd-y),%/$(obj))
	$(call merge_objs,$@,$^)

%/mtd.dep: $(foreach dep,$(mtd-y:.o=.dep),%/$(dep))
	$(call merge_deps,$@,$^)

