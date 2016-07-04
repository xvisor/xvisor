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
# @brief list of driver graphics objects
# */

drivers-objs-$(CONFIG_FB)+= video/fb.o

fb-y += fbmem.o
fb-y += fbnotify.o
fb-y += fbcmap.o
fb-y += fbmon.o
fb-y += fbcvt.o
fb-y += modedb.o

fb-$(CONFIG_FB_CFB_COPYAREA)  += cfbcopyarea.o
fb-$(CONFIG_FB_CFB_FILLRECT)  += cfbfillrect.o
fb-$(CONFIG_FB_CFB_IMAGEBLIT) += cfbimgblt.o

fb-$(CONFIG_FB_SYS_COPYAREA)  += syscopyarea.o
fb-$(CONFIG_FB_SYS_FILLRECT)  += sysfillrect.o
fb-$(CONFIG_FB_SYS_IMAGEBLIT) += sysimgblt.o

%/fb.o: $(foreach obj,$(fb-y),%/$(obj))
	$(call merge_objs,$@,$^)

%/fb.dep: $(foreach dep,$(fb-y:.o=.dep),%/$(dep))
	$(call merge_deps,$@,$^)

drivers-objs-$(CONFIG_FB_ARMCLCD)+= video/amba-clcd.o
drivers-objs-$(CONFIG_FB_VESA)+= video/vesafb.o
drivers-objs-$(CONFIG_FB_MXC) += video/mxcfb.o
drivers-objs-$(CONFIG_FB_MXC_LDB) += video/mxcldb.o
drivers-objs-$(CONFIG_FB_MXC_HDMI) += video/mxc_hdmi_i2c.o
drivers-objs-$(CONFIG_FB_MXC_HDMI) += video/mxc_hdmi.o
drivers-objs-$(CONFIG_FB_MXC_HDMI) += video/mxc_edid.o
