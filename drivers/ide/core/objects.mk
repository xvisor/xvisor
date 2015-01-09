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
# @author Himanshu Chauhan
# @brief list of ide and ata core framework objects
# */

drivers-objs-$(CONFIG_IDE) += ide/core/ide_core.o

ide_core-y+= ide_main.o
ide_core-y+= ide_libata.o

%/ide_core.o: $(foreach obj,$(ide_core-y),%/$(obj))
	$(call merge_objs,$@,$^)

%/ide_core.dep: $(foreach dep,$(ide_core-y:.o=.dep),%/$(dep))
	$(call merge_deps,$@,$^)
