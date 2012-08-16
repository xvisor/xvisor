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

core-objs-$(CONFIG_FB)+= fb/vmm_fbmem.o
core-objs-$(CONFIG_FB)+= fb/vmm_fbcmap.o
core-objs-$(CONFIG_FB)+= fb/vmm_fbmon.o
core-objs-$(CONFIG_FB)+= fb/vmm_fbcvt.o
core-objs-$(CONFIG_FB)+= fb/vmm_modedb.o

core-objs-$(CONFIG_FB_CFB_COPYAREA)+= fb/vmm_cfbcopyarea.o
core-objs-$(CONFIG_FB_CFB_FILLRECT)+= fb/vmm_cfbfillrect.o
core-objs-$(CONFIG_FB_CFB_IMAGEBLIT)+= fb/vmm_cfbimgblt.o

core-objs-$(CONFIG_FB_SYS_COPYAREA)+= fb/vmm_syscopyarea.o
core-objs-$(CONFIG_FB_SYS_FILLRECT)+= fb/vmm_sysfillrect.o
core-objs-$(CONFIG_FB_SYS_IMAGEBLIT)+= fb/vmm_sysimgblt.o
