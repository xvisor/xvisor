#/**
# Copyright (c) 2014 Anup Patel.
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
# @brief list of ARM Ltd DTBs.
# */

board-dtbs-$(CONFIG_ARMV8)+=dts/arm/foundation-v8-gicv2.dtb
board-dtbs-$(CONFIG_ARMV8)+=dts/arm/foundation-v8-gicv3.dtb
board-dtbs-$(CONFIG_ARMV7A_VE)+=dts/arm/vexpress-v2p-ca15-tc1.dtb
board-dtbs-$(CONFIG_ARMV7A)+=dts/arm/arm-realview-pba8.dtb
board-dtbs-$(CONFIG_ARMV7A)+=dts/arm/vexpress-v2p-ca9.dtb
board-dtbs-$(CONFIG_ARMV7A)+=dts/arm/vexpress-v2p-ca15-tc1-nove.dtb
board-dtbs-$(CONFIG_ARMV6)+=dts/arm/arm-realview-eb-11mp-ctrevb.dtb
board-dtbs-$(CONFIG_ARMV5)+=dts/arm/versatile-pb.dtb
