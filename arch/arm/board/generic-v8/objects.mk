#/**
# Copyright (c) 2013 Sukanto Ghosh.
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
# @author Sukanto Ghosh (sukantoghosh@gmail.com)
# @brief list of Generic ARMv8 board objects.
# */

board-objs-y+=brd_main.o
board-objs-y+=dts/skeleton.o

board-dtbs-$(CONFIG_FOUNDATION_V8_ONE_GUEST_PB_A8_DTS)+=dts/foundation-v8/one_guest_pb-a8.dtb
board-dtbs-$(CONFIG_FOUNDATION_V8_ONE_GUEST_VEXPRESS_A9_DTS)+=dts/foundation-v8/one_guest_vexpress-a9.dtb
board-dtbs-$(CONFIG_FOUNDATION_V8_ONE_GUEST_VEXPRESS_A15_DTS)+=dts/foundation-v8/one_guest_vexpress-a15.dtb
board-dtbs-$(CONFIG_FOUNDATION_V8_ONE_GUEST_VIRT_V8_DTS)+=dts/foundation-v8/one_guest_virt_v8.dtb

