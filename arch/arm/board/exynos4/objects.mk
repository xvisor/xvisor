#/**
# Copyright (c) 2012 Jean-Christophe Dubois.
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
# @brief list of Exynos4 board objects.
# */

board-objs-y+=brd_defterm.o
board-objs-y+=brd_main.o
board-objs-$(CONFIG_SMP)+=brd_smp.o
board-objs-$(CONFIG_SMDKC210_ONE_GUEST_PBA8_DTS)+=dts/smdkc210/one_guest_pb-a8.o
board-objs-$(CONFIG_SMDKC210_TWO_GUEST_PBA8_DTS)+=dts/smdkc210/two_guest_pb-a8.o
board-objs-$(CONFIG_SMDKC210_ONE_GUEST_VEXPRESSA9_DTS)+=dts/smdkc210/one_guest_vexpress-a9.o
board-objs-$(CONFIG_NURI_ONE_GUEST_PBA8_DTS)+=dts/nuri/one_guest_pb-a8.o
board-objs-$(CONFIG_NURI_TWO_GUEST_PBA8_DTS)+=dts/nuri/two_guest_pb-a8.o
board-objs-$(CONFIG_ODROIDX_ONE_GUEST_PBA8_DTS)+=dts/odroidx/one_guest_pb-a8.o
board-objs-$(CONFIG_ODROIDX_TWO_GUEST_PBA8_DTS)+=dts/odroidx/two_guest_pb-a8.o
board-objs-$(CONFIG_ODROIDX_ONE_GUEST_VEXPRESSA9_DTS)+=dts/odroidx/one_guest_vexpress-a9.o
