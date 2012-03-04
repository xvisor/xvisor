#/**
# Copyright (c) 2011 Pranav Sawargaonkar.
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
# @author Pranav Sawargaonkar (pranav.sawargaonkar@gmail.com)
# @brief list of OMAP3 beagle board objects.
# */

board-objs-y+=brd_defterm.o
board-objs-y+=brd_pic.o
board-objs-y+=brd_main.o
board-objs-y+=brd_timer.o
board-objs-$(CONFIG_OMAP3_ONE_GUEST_PBA8_DTS)+=dts/one_guest_pb-a8.o
board-objs-$(CONFIG_OMAP3_TWO_GUEST_PBA8_DTS)+=dts/two_guest_pb-a8.o

