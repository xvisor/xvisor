#/**
# Copyright (c) 2010 Himanshu Chauhan.
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
# @author Himanshu Chauhan (hschauhan@nulltrace.org)
# @brief list of driver objects
# */

drivers-objs-$(CONFIG_SERIAL)+= serial/serial.o
drivers-objs-$(CONFIG_SERIAL_8250_UART)+= serial/8250-uart.o
drivers-objs-$(CONFIG_SERIAL_OMAP_UART)+= serial/omap-uart.o
drivers-objs-$(CONFIG_SERIAL_PL01X)+= serial/pl011.o
drivers-objs-$(CONFIG_SERIAL_SAMSUNG)+= serial/samsung-uart.o
drivers-objs-$(CONFIG_SERIAL_IMX)+= serial/imx-uart.o
drivers-objs-$(CONFIG_SERIAL_SCIF)+= serial/scif.o
drivers-objs-$(CONFIG_SERIAL_BCM283X_MU)+= serial/bcm283x_mu.o
