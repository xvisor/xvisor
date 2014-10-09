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

core-objs-$(CONFIG_NET)+= net/vmm_netcore.o

vmm_netcore-y += vmm_mbuf.o
vmm_netcore-y += vmm_net.o
vmm_netcore-y += vmm_netswitch.o
vmm_netcore-y += vmm_netport.o
vmm_netcore-y += vmm_hub.o
vmm_netcore-y += vmm_bridge.o

%/vmm_netcore.o: $(foreach obj,$(vmm_netcore-y),%/$(obj))
	$(call merge_objs,$@,$^)

%/vmm_netcore.dep: $(foreach dep,$(vmm_netcore-y:.o=.dep),%/$(dep))
	$(call merge_deps,$@,$^)
