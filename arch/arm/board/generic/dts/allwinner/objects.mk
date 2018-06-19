#/**
# Copyright (c) 2018 Anup Patel.
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
# @author Jean-Christophe DUBOIS (jcd@tribudubois.net)
# @brief list of Allwinnner DTBs.
# */

board-dtbs-$(CONFIG_ARMV7A_VE)+=dts/allwinner/sun7i-a20-cubieboard2.dtb
board-dtbs-$(CONFIG_ARMV7A)+=dts/allwinner/sun4i-a10-cubieboard.dtb
board-dtbs-$(CONFIG_ARMV7A)+=dts/allwinner/sun4i-a10-hackberry.dtb
