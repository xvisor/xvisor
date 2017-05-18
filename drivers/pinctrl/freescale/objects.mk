#/**
# Copyright (c) 2017 Anup Patel.
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
# @brief list of Freescale pinctrl driver objects
# */

drivers-objs-$(CONFIG_PINCTRL_IMX)	+= pinctrl/freescale/pinctrl-imx.o
drivers-objs-$(CONFIG_PINCTRL_IMX6Q)	+= pinctrl/freescale/pinctrl-imx6q.o
drivers-objs-$(CONFIG_PINCTRL_IMX25)	+= pinctrl/freescale/pinctrl-imx25.o
