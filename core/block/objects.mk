#/**
# Copyright (c) 2012 Anup Patel.
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
# @brief list of core objects to be build
# */

core-objs-$(CONFIG_BLOCK)+= block/vmm_blockdev_mod.o

vmm_blockdev_mod-y += vmm_blockdev.o
vmm_blockdev_mod-y += vmm_blockrq_nop.o

%/vmm_blockdev_mod.o: $(foreach obj,$(vmm_blockdev_mod-y),%/$(obj))
	$(call merge_objs,$@,$^)

%/vmm_blockdev_mod.dep: $(foreach dep,$(vmm_blockdev_mod-y:.o=.dep),%/$(dep))
	$(call merge_deps,$@,$^)

core-objs-$(CONFIG_BLOCKPART)+= block/vmm_blockpart.o
core-objs-$(CONFIG_BLOCKPART_DOS)+= block/vmm_blockpart_dos.o

