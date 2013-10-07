#/**
# Copyright (c) 2011 Anup Patel.
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
# @brief list of Realview board objects.
# */

board-objs-y+=brd_main.o
board-objs-$(CONFIG_SMP)+=brd_smp.o
board-objs-$(CONFIG_REALVIEW_EBMP_ONE_GUEST_EBMP_DTS)+=dts/eb-mpcore/one_guest_ebmp.o
board-objs-$(CONFIG_REALVIEW_EBMP_TWO_GUEST_EBMP_DTS)+=dts/eb-mpcore/two_guest_ebmp.o
board-objs-$(CONFIG_REALVIEW_PBA8_ONE_GUEST_PBA8_DTS)+=dts/pb-a8/one_guest_pb-a8.o
board-objs-$(CONFIG_REALVIEW_PBA8_ONE_GUEST_VEX_A9_DTS)+=dts/pb-a8/one_guest_vexpress-a9.o
board-objs-$(CONFIG_REALVIEW_PBA8_TWO_GUEST_PBA8_DTS)+=dts/pb-a8/two_guest_pb-a8.o

board-dtbs-$(CONFIG_CPU_ARM11MP)+=dts/eb-mpcore/one_guest_ebmp.dtb
board-dtbs-$(CONFIG_CPU_ARM11MP)+=dts/eb-mpcore/two_guest_ebmp.dtb
board-dtbs-$(CONFIG_CPU_CORTEX_A8)+=dts/pb-a8/one_guest_pb-a8.dtb
board-dtbs-$(CONFIG_CPU_CORTEX_A8)+=dts/pb-a8/one_guest_vexpress-a9.dtb
board-dtbs-$(CONFIG_CPU_CORTEX_A8)+=dts/pb-a8/two_guest_pb-a8.dtb

