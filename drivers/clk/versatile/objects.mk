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
# @brief list of ARM reference designe clocking objects
# */

drivers-objs-$(CONFIG_COMMON_CLK_VERSATILE)+= clk/versatile/clk-versatile-drv.o

clk-versatile-drv-y += icst.o
clk-versatile-drv-y += clk-icst.o
clk-versatile-drv-y += clk-integrator.o
clk-versatile-drv-y += clk-impd1.o
clk-versatile-drv-y += clk-sp810.o
clk-versatile-drv-y += clk-versatile.o
clk-versatile-drv-y += clk-realview.o
clk-versatile-drv-y += clk-vexpress.o
clk-versatile-drv-$(CONFIG_VEXPRESS_CONFIG) += clk-vexpress-osc.o

%/clk-versatile-drv.o: $(foreach obj,$(clk-versatile-drv-y),%/$(obj))
	$(call merge_objs,$@,$^)

%/clk-versatile-drv.dep: $(foreach dep,$(clk-versatile-drv-y:.o=.dep),%/$(dep))
	$(call merge_deps,$@,$^)
