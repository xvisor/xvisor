#/**
# Copyright (c) 2014 Jean-Christophe Dubois.
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
# @author Jean-Christophe Dubois (jcd@tribudubois.net)
# @brief list of imx SOC clocking objects
# */

drivers-objs-$(CONFIG_COMMON_CLK_MXC) += clk/imx/clk-imx-drv.o

clk-imx-drv-y += clk-busy.o
clk-imx-drv-y += clk-fixup-div.o
clk-imx-drv-y += clk-gate2.o
clk-imx-drv-$(CONFIG_CPU_GENERIC_V5) += clk-pllv1.o
clk-imx-drv-$(CONFIG_CPU_GENERIC_V6) += clk-pllv1.o
clk-imx-drv-$(CONFIG_CPU_GENERIC_V7) += clk-pllv3.o
clk-imx-drv-y += clk.o
clk-imx-drv-y += clk-fixup-mux.o
clk-imx-drv-y += clk-pfd.o
clk-imx-drv-$(CONFIG_CPU_GENERIC_V5) += clk-imx25.o
clk-imx-drv-$(CONFIG_CPU_GENERIC_V7) += clk-imx6q.o

%/clk-imx-drv.o: $(foreach obj,$(clk-imx-drv-y),%/$(obj))
	$(call merge_objs,$@,$^)

%/clk-imx-drv.dep: $(foreach dep,$(clk-imx-drv-y:.o=.dep),%/$(dep))
	$(call merge_deps,$@,$^)
