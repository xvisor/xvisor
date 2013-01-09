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

core-objs-$(CONFIG_FB)+= fb/vmm_fb.o

vmm_fb-y += vmm_fbmem.o
vmm_fb-y += vmm_fbnotify.o
vmm_fb-y += vmm_fbcmap.o
vmm_fb-y += vmm_fbmon.o
vmm_fb-y += vmm_fbcvt.o
vmm_fb-y += vmm_modedb.o

vmm_fb-$(CONFIG_FB_CFB_COPYAREA)  += vmm_cfbcopyarea.o
vmm_fb-$(CONFIG_FB_CFB_FILLRECT)  += vmm_cfbfillrect.o
vmm_fb-$(CONFIG_FB_CFB_IMAGEBLIT) += vmm_cfbimgblt.o

vmm_fb-$(CONFIG_FB_SYS_COPYAREA)  += vmm_syscopyarea.o
vmm_fb-$(CONFIG_FB_SYS_FILLRECT)  += vmm_sysfillrect.o
vmm_fb-$(CONFIG_FB_SYS_IMAGEBLIT) += vmm_sysimgblt.o

%/vmm_fb.o: $(foreach obj,$(vmm_fb-y),%/$(obj))
	$(call merge_objs,$@,$^)

%/vmm_fb.dep: $(foreach dep,$(vmm_fb-y:.o=.dep),%/$(dep))
	$(call merge_deps,$@,$^)
