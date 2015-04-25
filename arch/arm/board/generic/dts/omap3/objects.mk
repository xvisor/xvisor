#/**
# Copyright (c) 2015 Anup Patel.
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
# @brief list of OMAP3 DTBs.
# */

board-dtbs-$(CONFIG_ARMV7A)+=dts/omap3/beagleboard/one_guest_pb-a8.dtb
board-dtbs-$(CONFIG_ARMV7A)+=dts/omap3/beagleboard/one_guest_vexpress-a9.dtb
board-dtbs-$(CONFIG_ARMV7A)+=dts/omap3/beagleboard/two_guest_pb-a8.dtb

board-dtbs-$(CONFIG_ARMV7A)+=dts/omap3/beagleboard-xm/one_guest_pb-a8.dtb
board-dtbs-$(CONFIG_ARMV7A)+=dts/omap3/beagleboard-xm/one_guest_vexpress-a9.dtb
board-dtbs-$(CONFIG_ARMV7A)+=dts/omap3/beagleboard-xm/two_guest_pb-a8.dtb

