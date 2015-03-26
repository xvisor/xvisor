#/**
# Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWide.
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
# @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
# @brief list of I2C driver objects
# */

drivers-objs-$(CONFIG_I2C)		+= i2c/i2c-core.o
drivers-objs-$(CONFIG_I2C_BOARDINFO)	+= i2c/i2c-boardinfo.o
drivers-objs-$(CONFIG_I2C_IMX)		+= i2c/busses/i2c-imx.o
