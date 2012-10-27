#/**
# Copyright (c) 2010 Himanshu Chauhan.
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
# @brief list of uip objects to be built.
# */

libs-objs-$(CONFIG_UIP)+= netstack/uip/uip_lib.o

uip_lib-y+= uip-daemon.o
uip_lib-y+= uip-netstack.o
uip_lib-y+= uip-netport.o
uip_lib-y+= uip-arp.o
uip_lib-y+= uip.o
uip_lib-y+= psock.o
uip_lib-y+= timer.o
uip_lib-y+= uip-fw.o
uip_lib-y+= uip-neighbor.o
uip_lib-y+= uip-split.o

%/uip_lib.o: $(foreach obj,$(uip_lib-y),%/$(obj))
	$(call merge_objs,$@,$^)

%/uip_lib.dep: $(foreach dep,$(uip_lib-y:.o=.dep),%/$(dep))
	$(call merge_deps,$@,$^)
