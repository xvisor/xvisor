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
# @author Jimmy Durand Wesolowski <jimmy.durand-wesolowski@openwide.fr>
# @brief list of MXC driver objects
# */

drivers-objs-$(CONFIG_IMX_IPUV3_CORE) += gpu/ipu-v3/ipu-v3.o

ipu-v3-y+=ipu_common.o
ipu-v3-y+=ipu_disp.o
ipu-v3-y+=ipu_device.o
ipu-v3-y+=ipu_pixel_clk.o
ipu-v3-y+=ipu_ic.o
ipu-v3-y+=ipu_capture.o

%/ipu-v3.o: $(foreach obj,$(ipu-v3-y),%/$(obj))
	$(call merge_objs,$@,$^)

%/ipu-v3.dep: $(foreach dep,$(ipu-v3-y:.o=.dep),%/$(dep))
	$(call merge_deps,$@,$^)
