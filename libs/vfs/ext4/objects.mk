#/**
# Copyright (c) 2013 Anup Patel.
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
# @author Anup Patel (anup@brainfault.org)
# @brief list of Ext4 objects to be build
# */

libs-objs-$(CONFIG_VFS_EXT4)+= vfs/ext4/ext4fs.o

ext4fs-y += ext4_control.o
ext4fs-y += ext4_node.o
ext4fs-y += ext4_main.o

%/ext4fs.o: $(foreach obj,$(ext4fs-y),%/$(obj))
	$(call merge_objs,$@,$^)

%/ext4fs.dep: $(foreach dep,$(ext4fs-y:.o=.dep),%/$(dep))
	$(call merge_deps,$@,$^)
